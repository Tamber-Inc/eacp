eacp_add_webview_app(WebViewTodo
        SOURCES         Main.cpp
        COMMAND_SOURCES Commands.cpp
        WEB_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY        TodoApp
        BUNDLE_ID       com.eacp.webviewtodo
        BUNDLE_NAME     "WebView Todo"
        REACT)
