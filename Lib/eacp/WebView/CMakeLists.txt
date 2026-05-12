include(FetchContent)

find_package(ResEmbed REQUIRED)
find_package(Miro REQUIRED)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewVite.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewBackend.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewApp.cmake)

add_library(eacp-webview STATIC)

target_link_libraries(eacp-webview PUBLIC eacp-graphics ResEmbed Miro)

target_sources(eacp-webview PRIVATE
        WebView/WebView-Shared.cpp
        WebView/Bridge.cpp
        WebView/MenuHelpers.cpp)

if (APPLE)
    target_sources(eacp-webview PRIVATE WebView/WebView.mm)
    target_link_libraries(eacp-webview PRIVATE
            "-framework Foundation"
            "-framework WebKit")

    if (IOS)
        target_link_libraries(eacp-webview PRIVATE
                "-framework UIKit")
    else ()
        target_link_libraries(eacp-webview PRIVATE
                "-framework AppKit")
    endif ()
elseif (WIN32)
    target_sources(eacp-webview PRIVATE WebView/WebView-Windows.cpp)

    find_package(WebView2 REQUIRED)

    target_link_libraries(eacp-webview PRIVATE
            WebView2::WebView2)
endif ()

set_default_target_setting(eacp-webview)
eacp_enable_unity_build(eacp-webview)
add_ide_sources(eacp-webview)

# Build-time tool: turns WebView/Resources/EacpBackend.ts.template into a
# concrete backend.ts shim for a given schema basename. The template is
# embedded into the executable via ResEmbed, mirroring how Miro ships its
# BridgeRuntime.ts inside libMiro. Skipped when cross-compiling — the
# tool only runs on the host during the developer build.
if (NOT CMAKE_CROSSCOMPILING)
    add_executable(eacp-webview-backend-gen Generator/Main.cpp)
    target_link_libraries(eacp-webview-backend-gen PRIVATE ResEmbed)

    res_embed_add(eacp-webview-backend-gen
            FILES WebView/Resources/EacpBackend.ts.template
            NAMESPACE EacpWebViewBackendGen
            CATEGORY EacpWebViewBackendGen)

    set_default_target_setting(eacp-webview-backend-gen)
endif ()
