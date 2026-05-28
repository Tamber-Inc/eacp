# eacp_add_webview_test — define a NanoTest binary that drives a
# WebView app over its embedded HTTP RPC test server.
#
# Usage:
#   eacp_add_webview_test(<TARGET>
#       APP     <app-target>          # WebView app the test spawns
#       SOURCES <files>               # NanoTest source files
#   )
#
# The app target's runtime binary path is baked into the test as the
# EACP_WEBVIEW_TEST_APP_BINARY compile define (a quoted string), so
# tests can spawn the app with one line and zero env-var setup:
#
#   #include <eacp/WebView/Test/Launch.h>
#   auto app = eacp::WebView::Test::launchApp(
#       {.bundle = EACP_WEBVIEW_TEST_APP_BINARY});
#
# The macro expands to the .app bundle's inner Mach-O on macOS (e.g.
# `.../MyApp.app/Contents/MacOS/MyApp`) — Launch does not look inside
# bundles. $<TARGET_FILE:...> gives the inner binary directly, which
# is the path posix_spawn needs.
#
# Platform notes:
#   - Apple iOS / Linux: eacp-webview-test is not built, so this
#     function is a no-op. Apps can call it unconditionally from
#     cross-platform CMakeLists.txt.
#   - NanoTest is fetched lazily on first call so projects that
#     never call this function don't drag it in.
function(eacp_add_webview_test TARGET)
    set(oneValueArgs APP)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_APP)
        message(FATAL_ERROR
                "eacp_add_webview_test(${TARGET}): APP is required")
    endif ()
    if (NOT ARG_SOURCES)
        message(FATAL_ERROR
                "eacp_add_webview_test(${TARGET}): SOURCES is required")
    endif ()

    if (NOT TARGET eacp-webview-test)
        return ()
    endif ()

    if (NOT COMMAND nano_add_executable)
        find_package(NanoTest REQUIRED)
    endif ()

    nano_add_executable(${TARGET}
            SOURCES ${ARG_SOURCES}
            TARGETS eacp-webview-test)

    # SOURCES paths in nano_add_executable resolve against the
    # caller's CMAKE_CURRENT_SOURCE_DIR, which inside this function
    # is the test target's source dir — no extra plumbing needed.

    # Ensure the app binary exists and is current before any test
    # in this target runs (the test spawns it as its first action).
    add_dependencies(${TARGET} ${ARG_APP})

    target_compile_definitions(${TARGET} PRIVATE
            EACP_WEBVIEW_TEST_APP_BINARY="$<TARGET_FILE:${ARG_APP}>")
endfunction()
