add_executable(WebViewReactAnim Main.cpp)
target_link_libraries(WebViewReactAnim PRIVATE eacp-webview)

eacp_webview_add_vite(WebViewReactAnim
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web
        NAMESPACE WebResources
        CATEGORY ReactAnimApp)

set(BUNDLE_ID "com.eacp.webviewreactanim")

set_target_properties(WebViewReactAnim PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "WebView React Anim"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

set_default_target_setting(WebViewReactAnim)
