# eacp_add_webview_app — single entry point for native + WebView + RPC apps.
#
# Folds five separate calls (add_executable, target_link_libraries,
# miro_export, eacp_webview_generate_backend, eacp_webview_add_vite)
# plus the macOS bundle plumbing into one call.
#
# Usage:
#   eacp_add_webview_app(<TARGET>
#       SOURCES         <files>           # main() + non-RPC sources
#       COMMAND_SOURCES <files>           # optional; files with MIRO_EXPORT_COMMAND
#       WEB_DIR         <path>            # vite project dir (must contain package.json)
#       BUNDLE_ID       <com.example.app>
#       BUNDLE_NAME     "Display name"
#       [NAMESPACE      <ns>]             # res-embed namespace; default WebResources
#       [CATEGORY       <cat>]            # res-embed category; default WebApp
#       [SCHEMA_NAME    <stem>]           # default: schema
#       [SCHEMA_FORMATS <formats>]        # default: ts backend ts-server
#   )
#
# Schema output is hardcoded to ${WEB_DIR}/src/generated. SOURCES and
# COMMAND_SOURCES both end up in the executable; only COMMAND_SOURCES feed
# the schema-export tool. When COMMAND_SOURCES is empty the helper skips
# the Miro pipeline and just builds the vite frontend.
#
# The schema target created here is named ${TARGET}Schema. To emit the
# same registrations to additional output dirs (e.g. C++ headers for a
# sibling target), call miro_export_emit(${TARGET}Schema ...) after
# this function returns — no need to declare a second exporter.
function(eacp_add_webview_app TARGET)
    set(oneValueArgs WEB_DIR BUNDLE_ID BUNDLE_NAME NAMESPACE CATEGORY SCHEMA_NAME)
    set(multiValueArgs SOURCES COMMAND_SOURCES SCHEMA_FORMATS)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_SOURCES)
        message(FATAL_ERROR "eacp_add_webview_app(${TARGET}): SOURCES is required")
    endif ()
    if (NOT ARG_WEB_DIR)
        message(FATAL_ERROR "eacp_add_webview_app(${TARGET}): WEB_DIR is required")
    endif ()
    if (NOT ARG_BUNDLE_ID)
        message(FATAL_ERROR "eacp_add_webview_app(${TARGET}): BUNDLE_ID is required")
    endif ()
    if (NOT ARG_BUNDLE_NAME)
        message(FATAL_ERROR "eacp_add_webview_app(${TARGET}): BUNDLE_NAME is required")
    endif ()
    if (NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "WebResources")
    endif ()
    if (NOT ARG_CATEGORY)
        set(ARG_CATEGORY "WebApp")
    endif ()
    if (NOT ARG_SCHEMA_NAME)
        set(ARG_SCHEMA_NAME "schema")
    endif ()
    if (NOT ARG_SCHEMA_FORMATS)
        set(ARG_SCHEMA_FORMATS ts backend ts-server)
    endif ()

    set(GENERATED_DIR "${ARG_WEB_DIR}/src/generated")

    add_executable(${TARGET} ${ARG_SOURCES} ${ARG_COMMAND_SOURCES})
    target_link_libraries(${TARGET} PRIVATE eacp-webview eacp-network-rpc)

    if (ARG_COMMAND_SOURCES)
        # SOURCES paths in miro_export are resolved against
        # CMAKE_CURRENT_SOURCE_DIR at *call* site, which inside this function
        # is the caller's directory — so bare filenames work.
        miro_export(${TARGET}Schema
                SOURCES ${ARG_COMMAND_SOURCES}
                OUTPUT_DIR ${GENERATED_DIR}
                OUTPUT_NAME ${ARG_SCHEMA_NAME}
                FORMATS ${ARG_SCHEMA_FORMATS})

        eacp_webview_generate_backend(
                OUTPUT_DIR ${GENERATED_DIR}
                BASENAME ${ARG_SCHEMA_NAME})

        add_dependencies(${TARGET} ${TARGET}Schema)
    endif ()

    eacp_webview_add_vite(${TARGET}
            SOURCE_DIR ${ARG_WEB_DIR}
            NAMESPACE ${ARG_NAMESPACE}
            CATEGORY ${ARG_CATEGORY})

    set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${ARG_BUNDLE_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER ${ARG_BUNDLE_ID}
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${ARG_BUNDLE_ID})

    set_default_target_setting(${TARGET})
endfunction()
