@@
 void sendChat(std::string_view chatString) {
     SceNetAdhocctlChatPacketC2S chat{};
     chat.base.opcode = OPCODE_CHAT;
@@
-        if (!chatString.empty()) {
-            //maximum char allowed is 64 character for compability with original server (pro.coldbird.net)
-            std::string message(chatString.substr(0, 60)); // 64 return chat variable corrupted is it out of memory?
-            strcpy(chat.message, message.c_str());
-            //Send Chat Messages
-            if (IsSocketReady((int)metasocket, false, true) > 0) {
-                int chatResult = (int)send((int)metasocket, (const char*)&chat, sizeof(chat), MSG_NOSIGNAL);
-                NOTICE_LOG(Log::sceNet, "Send Chat %s to Adhoc Server", chat.message);
-                std::string name = g_Config.sNickName;
-
-                std::lock_guard<std::mutex> guard(chatLogLock);
-                chatLog.emplace_back(name.substr(0, 8) + ": " + chat.message);
-                chatMessageGeneration++;
-            }
-        }
+        if (!chatString.empty()) {
+            // Keep wire-compatible packet size (ADHOCCTL_MESSAGE_LEN).
+            // If the message is longer than the packet allows, fragment it into multiple chat packets.
+            const size_t packSize = ADHOCCTL_MESSAGE_LEN - 1; // leave room for safety/null
+            std::string full(chatString);
+
+            // Unique fragment id so receivers can reassemble. Use time + low-rand.
+            uint32_t fragId = static_cast<uint32_t>(time(nullptr)) ^ (static_cast<uint32_t>(rand()) << 16);
+            std::ostringstream tmp;
+            tmp << std::hex << std::uppercase << fragId;
+            std::string fragIdHex = tmp.str();
+
+            // Prepare fragments. We'll add a short ASCII header to each fragment of the form:
+            // PPF:<FRAGIDHEX>:<IDX>/<TOTAL>|<payload>
+            // This keeps on-wire size <= ADHOCCTL_MESSAGE_LEN and allows updated clients to reassemble.
+            // Note: older clients/servers will still accept and display fragments individually.
+            size_t pos = 0;
+            size_t maxPayloadPerPacket = packSize; // we'll subtract header per-fragment below
+            // Pre-calc worst-case header length to compute total parts.
+            std::string sampleHeader = "PPF:" + fragIdHex + ":" + std::to_string(1) + "/" + std::to_string(1) + "|";
+            size_t headerOverhead = sampleHeader.size();
+            if (headerOverhead >= packSize) headerOverhead = 0; // paranoid
+            size_t usable = packSize - headerOverhead;
+            size_t totalParts = (full.size() + usable - 1) / usable;
+
+            for (size_t i = 0; i < totalParts; ++i) {
+                size_t idx = i + 1;
+                std::string header = "PPF:" + fragIdHex + ":" + std::to_string(idx) + "/" + std::to_string(totalParts) + "|";
+                size_t avail = packSize - header.size();
+                std::string chunk = full.substr(pos, avail);
+                std::string out = header + chunk;
+                // Safe copy into packet buffer
+                memset(chat.message, 0, sizeof(chat.message));
+                strncpy(chat.message, out.c_str(), sizeof(chat.message) - 1);
+
+                // Send
+                if (IsSocketReady((int)metasocket, false, true) > 0) {
+                    int chatResult = (int)send((int)metasocket, (const char*)&chat, sizeof(chat), MSG_NOSIGNAL);
+                    NOTICE_LOG(Log::sceNet, "Send Chat fragment %zu/%zu to Adhoc Server: %s", idx, totalParts, chat.message);
+                }
+
+                pos += avail;
+            }
+
+            // Add full message locally to chat log immediately (so sender sees it as one message)
+            std::string name = g_Config.sNickName;
+            std::lock_guard<std::mutex> guard(chatLogLock);
+            chatLog.emplace_back(name.substr(0, 8) + ": " + full);
+            chatMessageGeneration++;
+        }
     }
@@
 }
 
 std::vector<std::string> getChatLog() {
     std::lock_guard<std::mutex> guard(chatLogLock);
-    // If the log gets large, trim it down.
-    if (chatLog.size() > 50) {
-        chatLog.erase(chatLog.begin(), chatLog.begin() + (chatLog.size() - 50));
-    }
+    // If the log gets large, trim it down. Honor configurable max entries.
+    int maxEntries = g_Config.iChatLogMaxEntries > 0 ? g_Config.iChatLogMaxEntries : 50;
+    if (chatLog.size() > static_cast<size_t>(maxEntries)) {
+        chatLog.erase(chatLog.begin(), chatLog.begin() + (chatLog.size() - maxEntries));
+    }
     return chatLog;
 }
@@
     // Chat Packet
     else if (rx[0] == OPCODE_CHAT) {
         // Enough Data available
         if (rxpos >= (int)sizeof(SceNetAdhocctlChatPacketS2C)) {
             // Cast Packet
             SceNetAdhocctlChatPacketS2C* packet = (SceNetAdhocctlChatPacketS2C*)rx;
             INFO_LOG(Log::sceNet, "FriendFinder: Incoming OPCODE_CHAT");
 
             // Fix strings with null-terminated
             packet->name.data[ADHOCCTL_NICKNAME_LEN - 1] = 0;
             packet->base.message[ADHOCCTL_MESSAGE_LEN - 1] = 0;
 
-            // Add Incoming Chat to HUD
-            NOTICE_LOG(Log::sceNet, "Received chat message %s", packet->base.message);
-            std::string incoming = "";
-            std::string name = (char*)packet->name.data;
-            incoming.append(name.substr(0, 8));
-            incoming.append(": ");
-            incoming.append((char*)packet->base.message);
-
-            std::lock_guard<std::mutex> guard(chatLogLock);
-            chatLog.push_back(incoming);
-            chatMessageGeneration++;
+            // Add Incoming Chat to HUD. Support reassembly of fragments sent by updated clients.
+            const char *raw = (const char*)packet->base.message;
+            std::string msg(raw);
+            std::string name = (char*)packet->name.data;
+
+            // Fragment reassembly storage (static, keyed by name+fragid)
+            struct FragEntry { int total = 0; std::map<int, std::string> parts; uint64_t lastSeen = 0; };
+            static std::map<std::string, FragEntry> fragMap;
+
+            auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
+                std::chrono::system_clock::now().time_since_epoch()).count());
+
+            bool handled = false;
+            if (msg.rfind("PPF:", 0) == 0) {
+                // Format: PPF:<FRAGIDHEX>:<IDX>/<TOTAL>|<payload>
+                size_t firstColon = msg.find(':', 4);
+                size_t slash = std::string::npos;
+                size_t pipe = msg.find('|');
+                if (firstColon != std::string::npos && pipe != std::string::npos) {
+                    std::string fragIdHex = msg.substr(4, firstColon - 4);
+                    std::string idxTotal = msg.substr(firstColon + 1, pipe - (firstColon + 1));
+                    slash = idxTotal.find('/');
+                    if (slash != std::string::npos) {
+                        int idx = atoi(idxTotal.substr(0, slash).c_str());
+                        int total = atoi(idxTotal.substr(slash + 1).c_str());
+                        std::string payload = msg.substr(pipe + 1);
+
+                        std::string key = name + "|" + fragIdHex;
+                        auto &entry = fragMap[key];
+                        entry.total = total;
+                        entry.parts[idx] = payload;
+                        entry.lastSeen = now_ms;
+
+                        // If all parts received, assemble and push single message.
+                        if ((int)entry.parts.size() == entry.total) {
+                            std::string full;
+                            for (int i = 1; i <= entry.total; ++i) {
+                                full += entry.parts[i];
+                            }
+                            std::string incoming = name.substr(0, 8) + ": " + full;
+                            std::lock_guard<std::mutex> guard(chatLogLock);
+                            chatLog.push_back(incoming);
+                            chatMessageGeneration++;
+                            handled = true;
+                            fragMap.erase(key);
+                        }
+                    }
+                }
+            }
+
+            // Periodically flush stale fragment assemblies (older than 10s)
+            for (auto it = fragMap.begin(); it != fragMap.end();) {
+                if (now_ms > it->second.lastSeen + 10000) {
+                    // assemble whatever we have in order
+                    std::string assembled;
+                    for (auto &p : it->second.parts) assembled += p.second;
+                    if (!assembled.empty()) {
+                        // key format: name|fragid
+                        std::string senderName = it->first.substr(0, it->first.find('|'));
+                        std::string incoming = senderName.substr(0, 8) + ": " + assembled;
+                        std::lock_guard<std::mutex> guard(chatLogLock);
+                        chatLog.push_back(incoming);
+                        chatMessageGeneration++;
+                    }
+                    it = fragMap.erase(it);
+                } else {
+                    ++it;
+                }
+            }
+
+            if (!handled) {
+                // Not a fragment or not yet complete: treat as a normal chat message
+                std::string incoming = "";
+                incoming.append(name.substr(0, 8));
+                incoming.append(": ");
+                incoming.append(msg);
+
+                std::lock_guard<std::mutex> guard(chatLogLock);
+                chatLog.push_back(incoming);
+                chatMessageGeneration++;
+            }
@@
             // Move RX Buffer
             memmove(rx, rx + sizeof(SceNetAdhocctlChatPacketS2C), sizeof(rx) - sizeof(SceNetAdhocctlChatPacketS2C));
 
             // Fix RX Buffer Length
             rxpos -= sizeof(SceNetAdhocctlChatPacketS2C);
         }
     }
