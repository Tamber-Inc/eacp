# eacp_add_swiftui_app(<TARGET>
#         SOURCES       <c++ sources>     # Main.cpp and any other C++/ObjC++
#         SWIFT_SOURCES <swift sources>   # the app's SwiftUI views + @_cdecl glue
#         [API          <Api::Type>]      # reflected C++ API to expose to Swift
#         [API_HEADER   <header>]         # header declaring the API (required with API)
#         BUNDLE_ID     <reverse-dns id>
#         BUNDLE_NAME   "Display Name")
#
# When API/API_HEADER are given, Miro generates a typed Swift client (Schema*.
# swift) from the reflected API and compiles it into the app's Swift module, so
# SwiftUI can call C++ commands. The generated files land in <app>/generated and
# are committed: cross-compiled (iOS) builds can't run the host codegen tool, so
# they consume the committed copies (see CMAKE_CROSSCOMPILING in miro_export).
#
# Mirrors eacp_add_webview_app: it wires a foreign-UI app (here SwiftUI) into an
# EACP executable. The app's Swift sources compile to a framework that exports
# the SwiftUIHost.h C-ABI symbols (via @_cdecl) and self-contains the Swift +
# SwiftUI runtime; the C++ executable links eacp-swiftui plus that framework,
# resolves the C symbols at link time, and embeds + signs the framework into the
# .app so it loads at runtime on macOS and iOS (device and simulator).
#
# Swift must already be enabled at file scope by the caller's parent directory
# (enable_language cannot run inside a function). Verified to build under both
# the Ninja and Xcode generators; only the Unix Makefiles generator can't
# compile Swift. The Swift sources and the C++/ObjC++ sources stay in separate
# targets (dylib vs executable) — that separation is what keeps the Xcode
# generator happy, since it can't mix Swift with other languages in one target.
# When no Swift compiler is present this is a no-op so non-Swift configurations
# still generate.

function(eacp_add_swiftui_app TARGET)
    set(oneValueArgs BUNDLE_ID BUNDLE_NAME API API_HEADER)
    set(multiValueArgs SOURCES SWIFT_SOURCES)
    cmake_parse_arguments(ESA "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT CMAKE_Swift_COMPILER)
        message(STATUS
                "${TARGET}: Swift not enabled; skipping SwiftUI app. Use the "
                "Ninja or Xcode generator on Apple to build it.")
        return()
    endif ()

    # The C-ABI seam header the Swift module imports. Located relative to this
    # helper file so it resolves no matter which app directory calls in.
    set(host_header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../SwiftUIHost.h")

    set(swift_sources ${ESA_SWIFT_SOURCES})

    # Optional: generate a typed Swift client from a reflected C++ API.
    if (ESA_API)
        if (NOT ESA_API_HEADER)
            message(FATAL_ERROR
                    "eacp_add_swiftui_app(${TARGET}): API requires API_HEADER")
        endif ()

        set(gen_dir "${CMAKE_CURRENT_SOURCE_DIR}/generated")
        set(gen_swift
                "${gen_dir}/Schema.swift"
                "${gen_dir}/Schema.runtime.swift"
                "${gen_dir}/Schema.client.swift")

        # Builds the ${TARGET}Schema_Codegen host tool (skipped automatically
        # when cross-compiling — iOS then uses the committed generated files).
        miro_export(${TARGET}Schema API ${ESA_API} API_HEADER ${ESA_API_HEADER})

        if (NOT CMAKE_CROSSCOMPILING)
            add_custom_command(
                    OUTPUT ${gen_swift}
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${gen_dir}"
                    COMMAND ${TARGET}Schema_Codegen --out "${gen_dir}" --name Schema
                            --format swift --format swift-runtime --format swift-client
                    DEPENDS ${TARGET}Schema_Codegen
                    COMMENT "eacp-swiftui: generating Swift client for ${TARGET}"
                    VERBATIM)
            add_custom_target(${TARGET}_GenerateSwift DEPENDS ${gen_swift})
        endif ()

        set_source_files_properties(${gen_swift} PROPERTIES GENERATED TRUE)
        list(APPEND swift_sources ${gen_swift})
    endif ()

    add_library(${TARGET}_Swift SHARED ${swift_sources})
    target_compile_options(${TARGET}_Swift PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${host_header}>")

    if (ESA_API AND NOT CMAKE_CROSSCOMPILING)
        add_dependencies(${TARGET}_Swift ${TARGET}_GenerateSwift)
    endif ()

    # Package the Swift module as a framework. A flat dylib links fine but isn't
    # found at runtime once the bundle leaves the build tree; an embedded
    # framework is the form iOS (and a self-contained macOS .app) needs.
    set_target_properties(${TARGET}_Swift PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER "${ESA_BUNDLE_ID}.swift")

    add_executable(${TARGET} ${ESA_SOURCES})
    target_link_libraries(${TARGET} PRIVATE eacp-swiftui ${TARGET}_Swift)

    set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${ESA_BUNDLE_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${ESA_BUNDLE_ID}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ESA_BUNDLE_ID}"
            # Embed + code-sign the Swift framework into the .app (Xcode
            # generator). Under Ninja the framework is found via the build-tree
            # rpath CMake adds automatically, which is enough for local runs.
            XCODE_EMBED_FRAMEWORKS "${TARGET}_Swift"
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
            # Locate the embedded framework from inside the bundle: iOS keeps
            # frameworks at App.app/Frameworks, macOS at App.app/Contents/
            # Frameworks. BUILD_RPATH is additive, so the automatic build-tree
            # entries that the Ninja flow relies on are preserved.
            BUILD_RPATH "@executable_path/Frameworks;@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/Frameworks;@executable_path/../Frameworks")

    set_default_target_setting(${TARGET})
endfunction()
