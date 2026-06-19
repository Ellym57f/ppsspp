@@
 struct Config : public ConfigBlock {
 public:
     ~Config();

     void Init();
@@
     // Adhoc/Networking
+    bool bChatOverlayEnabled = true;
+    float fChatOverlayFadeSeconds = 2.5f;
+    int iChatOverlayMaxLines = 4;
+    int iChatLogMaxEntries = 50;
+
     // These are Win UI only
     bool bTopMost;
*** End Patch