@@
-        TextEditPopupScreen *popupScreen = new TextEditPopupScreen(&messageTemp_, "", n->T("Chat message"), 256);
+        TextEditPopupScreen *popupScreen = new TextEditPopupScreen(&messageTemp_, "", n->T("Chat message"), 1000);
         if (System_GetPropertyBool(SYSPROP_KEYBOARD_IS_SOFT)) {
             popupScreen->SetAlignTop(true);
         }
*** End Patch