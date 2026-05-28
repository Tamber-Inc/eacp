set(CMAKE_FOLDER Tests)

find_package(NanoTest REQUIRED)

add_subdirectory(Core)

if (APPLE)
    add_subdirectory(ObjC)
endif ()

add_subdirectory(Network)

# Codegen tests exercise eacp's WebView events/hooks format registrations,
# so they only run on the platforms where the WebView library (and its
# eacp-webview-codegen object lib) are built.
if ((APPLE OR WIN32) AND EACP_BUILD_GRAPHICS AND EACP_BUILD_WEBVIEW)
    add_subdirectory(Codegen)
endif ()

# WebView tests live behind the same gate as the eacp-webview-test
# target itself (Apple-not-iOS + Windows). Check by target presence
# so the gate stays in one place — Lib/eacp/WebView/Test/CMakeLists.txt.
if (TARGET eacp-webview-test)
    add_subdirectory(WebView)
endif ()
