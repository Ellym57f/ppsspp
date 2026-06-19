@@
 	networkingSettings->Add(new ItemHeader(n->T("Ad Hoc multiplayer")));
@@
 	if (AdhocServerNameIsCustom()) {
@@
 	} else if (!g_Config.sProAdhocServer.empty()) {
 		AdhocServerListEntry entry;
 		if (AdhocGetServerByHost(g_Config.sProAdhocServer, &entry)) {
@@
 		}
 	}
+
+	// Chat UI options
+	auto chatHeader = networkingSettings->Add(new ItemHeader(n->T("Chat")));
+	networkingSettings->Add(new CheckBox(&g_Config.bChatOverlayEnabled, n->T("Show chat overlay")));
+
+	// Fade seconds
+	static std::string chatFadeStr = std::to_string(g_Config.fChatOverlayFadeSeconds);
+	PopupTextInputChoice *fadeChoice = networkingSettings->Add(new PopupTextInputChoice(NON_EPHEMERAL_TOKEN, &chatFadeStr, n->T("Chat overlay fade seconds"), "", 8, screenManager(), new LinearLayoutParams(1.0)));
+	fadeChoice->OnChange.Add([](UI::EventParams &e) {
+		try {
+			float v = std::stof(e.s);
+			if (v < 0.1f) v = 0.1f;
+			g_Config.fChatOverlayFadeSeconds = v;
+		} catch(...) {}
+	});
+
+	// Max overlay lines
+	static std::string chatLinesStr = std::to_string(g_Config.iChatOverlayMaxLines);
+	PopupTextInputChoice *linesChoice = networkingSettings->Add(new PopupTextInputChoice(NON_EPHEMERAL_TOKEN, &chatLinesStr, n->T("Chat overlay max lines"), "", 4, screenManager(), new LinearLayoutParams(1.0)));
+	linesChoice->OnChange.Add([](UI::EventParams &e) {
+		try { g_Config.iChatOverlayMaxLines = std::stoi(e.s); } catch(...) {}
+	});
+
+	// Max chat log entries
+	static std::string chatLogMaxStr = std::to_string(g_Config.iChatLogMaxEntries);
+	PopupTextInputChoice *logChoice = networkingSettings->Add(new PopupTextInputChoice(NON_EPHEMERAL_TOKEN, &chatLogMaxStr, n->T("Max chat log entries"), "", 5, screenManager(), new LinearLayoutParams(1.0)));
+	logChoice->OnChange.Add([](UI::EventParams &e) {
+		try { g_Config.iChatLogMaxEntries = std::stoi(e.s); } catch(...) {}
+	});
*** End Patch