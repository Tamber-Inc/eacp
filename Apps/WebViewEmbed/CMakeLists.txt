eacp_add_webview_app(WebViewEmbed
        SOURCES         Main.cpp
        COMMAND_SOURCES Commands.cpp
        WEB_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/web
        BUNDLE_ID       com.eacp.webviewembed
        BUNDLE_NAME     "WebView Embed")

# Secondary codegen: emit C++ headers for the same schema so the
# HttpRpcDemo sibling can consume them without repeating the export.
set(WEBVIEWEMBED_CPP_GENERATED_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/cpp-generated)

miro_add_type_export(
        NAME WebViewEmbedCppSchema
        SOURCES Commands.cpp
        OUTPUT_DIR ${WEBVIEWEMBED_CPP_GENERATED_DIR}
        OUTPUT_NAME schema
        FORMATS cpp-miro cpp-client)

set(WEBVIEWEMBED_CPP_GENERATED_DIR
        ${WEBVIEWEMBED_CPP_GENERATED_DIR} PARENT_SCOPE)

add_dependencies(WebViewEmbed WebViewEmbedCppSchema)
