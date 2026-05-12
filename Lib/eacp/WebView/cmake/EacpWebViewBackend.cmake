# eacp_webview_generate_backend — emit the TypeScript shim that binds
# Miro's transport-agnostic makeBridge runtime to the eacp WebView
# transport (window.eacp). The shim is rendered from
# WebView/Resources/EacpBackend.ts.template, which is embedded into the
# eacp-webview-backend-gen executable via ResEmbed. Mirrors Miro's
# BridgeRuntime.ts / ${NAME}_codegen pattern: the .ts content lives as a
# real source file, gets compiled into a tiny host-side tool, and the
# tool writes the file out at build time.
#
# Usage:
#   eacp_webview_generate_backend(
#       TARGET      <consumer-target>          # exec that consumes the shim
#       OUTPUT_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/web/src/generated
#       BASENAME    schema                     # stem of schema files (default: schema)
#       [OUTPUT_NAME backend]                  # filename stem (default: backend)
#   )
#
# Side effect: registers a build-time custom command that writes
# ${OUTPUT_DIR}/${OUTPUT_NAME}.ts and adds it as a dependency of
# <consumer-target>, so cmake --build produces the shim before the
# consumer (and any Vite step hung off it) runs.
function(eacp_webview_generate_backend)
    cmake_parse_arguments(ARG ""
            "TARGET;BASENAME;OUTPUT_DIR;OUTPUT_NAME" "" ${ARGN})

    if (NOT ARG_TARGET)
        message(FATAL_ERROR
                "eacp_webview_generate_backend: TARGET is required")
    endif ()
    if (NOT TARGET ${ARG_TARGET})
        message(FATAL_ERROR
                "eacp_webview_generate_backend: target '${ARG_TARGET}' does not exist")
    endif ()
    if (NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR
                "eacp_webview_generate_backend: OUTPUT_DIR is required")
    endif ()
    if (NOT ARG_BASENAME)
        set(ARG_BASENAME "schema")
    endif ()
    if (NOT ARG_OUTPUT_NAME)
        set(ARG_OUTPUT_NAME "backend")
    endif ()

    set(SHIM_PATH "${ARG_OUTPUT_DIR}/${ARG_OUTPUT_NAME}.ts")

    if (CMAKE_CROSSCOMPILING)
        # The generator is a host-only tool. For cross builds the shim is
        # expected to be committed to the repo alongside the prebuilt
        # Vite dist (same constraint Miro applies to its codegen output).
        return()
    endif ()

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")

    add_custom_command(
            OUTPUT "${SHIM_PATH}"
            COMMAND eacp-webview-backend-gen
                    --out "${SHIM_PATH}"
                    --basename "${ARG_BASENAME}"
            DEPENDS eacp-webview-backend-gen
            COMMENT "Generating WebView backend shim ${SHIM_PATH}"
            VERBATIM)

    set(SHIM_TARGET ${ARG_TARGET}-webview-backend-shim)
    add_custom_target(${SHIM_TARGET} DEPENDS "${SHIM_PATH}")
    add_dependencies(${ARG_TARGET} ${SHIM_TARGET})
endfunction()
