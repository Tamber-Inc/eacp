# eacp_add_swiftui_app(<TARGET>
#         SOURCES       <c++ sources>     # Main.cpp and any other C++/ObjC++
#         SWIFT_SOURCES <swift sources>   # the app's SwiftUI views + @_cdecl glue
#         BUNDLE_ID     <reverse-dns id>
#         BUNDLE_NAME   "Display Name")
#
# Mirrors eacp_add_webview_app: it wires a foreign-UI app (here SwiftUI) into an
# EACP executable. The app's Swift sources compile to a shared library that
# exports the SwiftUIHost.h C-ABI symbols (via @_cdecl) and self-contains the
# Swift + SwiftUI runtime; the C++ executable links eacp-swiftui plus that dylib
# and resolves the C symbols at link time.
#
# Swift must already be enabled at file scope by the caller's parent directory
# (enable_language cannot run inside a function). When no Swift compiler is
# present this is a no-op so non-Swift configurations still generate.

function(eacp_add_swiftui_app TARGET)
    set(oneValueArgs BUNDLE_ID BUNDLE_NAME)
    set(multiValueArgs SOURCES SWIFT_SOURCES)
    cmake_parse_arguments(ESA "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT CMAKE_Swift_COMPILER)
        message(STATUS
                "${TARGET}: Swift not enabled; skipping SwiftUI app. Configure "
                "with -G Ninja on Apple to build it.")
        return()
    endif ()

    # The C-ABI seam header the Swift module imports. Located relative to this
    # helper file so it resolves no matter which app directory calls in.
    set(host_header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../SwiftUIHost.h")

    add_library(${TARGET}_Swift SHARED ${ESA_SWIFT_SOURCES})
    target_compile_options(${TARGET}_Swift PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${host_header}>")

    add_executable(${TARGET} ${ESA_SOURCES})
    target_link_libraries(${TARGET} PRIVATE eacp-swiftui ${TARGET}_Swift)

    set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${ESA_BUNDLE_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${ESA_BUNDLE_ID}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ESA_BUNDLE_ID}")

    set_default_target_setting(${TARGET})
endfunction()
