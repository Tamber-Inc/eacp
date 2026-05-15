eacp_add_webview_app(WebViewReactAnim
        SOURCES         Main.cpp
        COMMAND_SOURCES Commands.cpp
        WEB_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY        ReactAnimApp
        BUNDLE_ID       com.eacp.webviewreactanim
        BUNDLE_NAME     "WebView React Anim")
