--- a/UI/EmuScreen.cpp
+++ b/UI/EmuScreen.cpp
@@
 void EmuScreen::OpenChat(bool focus) {
     if (chatButton_ != nullptr && chatButton_->GetVisibility() == UI::V_VISIBLE) {
         chatButton_->SetVisibility(UI::V_GONE);
     }
     if (chatMenu_ != nullptr) {
         chatMenu_->SetVisibility(UI::V_VISIBLE);
@@
         }
     }
+
+    // Hide transient overlay when opening full chat
+    if (chatOverlay_) {
+        chatOverlay_->SetVisibility(UI::V_GONE);
+    }
 }
@@
 void EmuScreen::update() {
     using namespace UI;
@@
     if (chatButton_ && chatMenu_) {
         if (chatMenu_->GetVisibility() != V_GONE) {
             chatMessages_ = GetChatMessageCount();
             newChatMessages_ = 0;
         } else {
             int diff = GetChatMessageCount() - chatMessages_;
             // Cap the count at 50.
             newChatMessages_ = diff > 50 ? 50 : diff;
         }
     }
+
+    // Lazily create the chat overlay UI group if needed.
+    if (!chatOverlay_ && root_ && g_Config.bChatOverlayEnabled) {
+        using namespace UI;
+        // Create a small vertical layout to show lines of text.
+        chatOverlay_ = new LinearLayout(ORIENT_VERTICAL, new AnchorLayoutParams(WRAP_CONTENT, WRAP_CONTENT, NONE, NONE, NONE, NONE));
+        chatOverlay_->SetBG(Drawable(0x99303030));
+        chatOverlay_->SetHasDropShadow(true);
+        chatOverlay_->SetVisibility(V_GONE);
+        // Add to root so it's rendered on top of game.
+        root_->Add(chatOverlay_);
+    }
+
+    // If there are new chat messages and overlay is enabled, populate and show overlay.
+    if (g_Config.bChatOverlayEnabled && chatOverlay_ && newChatMessages_ > 0) {
+        // Pull the latest messages from getChatLog().
+        std::vector<std::string> all = getChatLog();
+        int maxLines = g_Config.iChatOverlayMaxLines > 0 ? g_Config.iChatOverlayMaxLines : 4;
+        chatOverlayMessages_.clear();
+        for (int i = (int)all.size() - maxLines; i < (int)all.size(); ++i) {
+            if (i >= 0) chatOverlayMessages_.push_back(all[i]);
+        }
+
+        // Recreate overlay children
+        chatOverlay_->Clear();
+        for (auto &line : chatOverlayMessages_) {
+            TextView *tv = chatOverlay_->Add(new TextView(line, ALIGN_LEFT | FLAG_WRAP_TEXT, true, new LayoutParams(WRAP_CONTENT, WRAP_CONTENT)));
+            tv->SetTextColor(0xFFFFFFFF);
+        }
+
+        // Position overlay based on user preference. Use simple anchor offsets.
+        float margin = 12.0f;
+        Bounds b = root_->GetBounds();
+        // We'll place using SetBounds via the view's layout parameters: AnchorLayoutParams are used by AnchorLayout, but root_ may not be an AnchorLayout.
+        // As a simple method, move overlay's translation to approximate anchor.
+        float ox = 0.0f, oy = 0.0f;
+        switch (g_Config.iChatOverlayCorner) {
+        case 0: // top-left
+            ox = margin; oy = margin;
+            break;
+        case 1: // top-right
+            ox = b.w - chatOverlay_->GetBounds().w - margin; oy = margin;
+            break;
+        case 2: // bottom-left
+            ox = margin; oy = b.h - chatOverlay_->GetBounds().h - margin;
+            break;
+        case 3: // bottom-right
+            ox = b.w - chatOverlay_->GetBounds().w - margin; oy = b.h - chatOverlay_->GetBounds().h - margin;
+            break;
+        default:
+            ox = b.w - chatOverlay_->GetBounds().w - margin; oy = margin;
+            break;
+        }
+        chatOverlay_->SetTranslation(ox, oy);
+
+        chatOverlay_->SetVisibility(V_VISIBLE);
+        chatOverlayExpireTime_ = OS::GetTimeMillisDouble() / 1000.0 + g_Config.fChatOverlayFadeSeconds;
+    }
+
+    // Manage overlay fade/hide per-frame
+    if (chatOverlay_ && chatOverlay_->GetVisibility() == V_VISIBLE) {
+        double now = OS::GetTimeMillisDouble() / 1000.0;
+        double remain = chatOverlayExpireTime_ - now;
+        if (remain <= 0.0) {
+            chatOverlay_->SetVisibility(V_GONE);
+        } else {
+            // Apply alpha proportional to remaining time (fade out near end)
+            double fade = remain / std::max(0.001, (double)g_Config.fChatOverlayFadeSeconds);
+            uint8_t baseAlpha = 0x99; // same as earlier
+            uint8_t a = static_cast<uint8_t>(baseAlpha * fade);
+            uint32_t bg = (uint32_t(a) << 24) | (0x00303030);
+            chatOverlay_->SetBG(Drawable(bg));
+            // adjust text alpha
+            uint32_t txtBase = 0xFFFFFF;
+            uint32_t txtColor = (uint32_t(0xFF & (a)) << 24) | txtBase;
+            for (auto child : chatOverlay_->Children()) {
+                if (auto tv = dynamic_cast<TextView*>(child)) {
+                    tv->SetTextColor(txtColor);
+                }
+            }
+        }
+    }
*** End Patch