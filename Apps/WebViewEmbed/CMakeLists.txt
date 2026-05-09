eacp_add_webview_app(WebViewEmbed
        SOURCES         Main.cpp
        COMMAND_SOURCES Commands.cpp
        WEB_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/web
        BUNDLE_ID       com.eacp.webviewembed
        BUNDLE_NAME     "WebView Embed")

# Same schema, additional emit so the HttpRpcDemo sibling can consume
# the typed C++ headers without spawning a second exporter executable.
set(WEBVIEWEMBED_CPP_GENERATED_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-generated)

miro_export_emit(WebViewEmbedSchema
        OUTPUT_DIR ${WEBVIEWEMBED_CPP_GENERATED_DIR}
        OUTPUT_NAME schema
        FORMATS cpp-miro cpp-client)

set(WEBVIEWEMBED_CPP_GENERATED_DIR
        ${WEBVIEWEMBED_CPP_GENERATED_DIR} PARENT_SCOPE)
