--- a/UI/EmuScreen.h
+++ b/UI/EmuScreen.h
@@
 	UI::Button *cardboardDisableButton_ = nullptr;
 
 	std::string extraAssertInfoStr_;
+
+	// Chat overlay HUD (transient messages shown on-screen)
+	UI::LinearLayout *chatOverlay_ = nullptr;
+	double chatOverlayExpireTime_ = 0.0;
+	int chatOverlayCorner_ = 1; // 0=top-left,1=top-right,2=bottom-left,3=bottom-right
+	std::vector<std::string> chatOverlayMessages_;
+
 	std::unique_ptr<ImDebugger> imDebugger_;
 	ImCommand imCmd_{};  // needed to buffer commands in case imgui wasn't created yet.
*** End Patch