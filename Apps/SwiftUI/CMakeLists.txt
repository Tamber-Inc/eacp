set(CMAKE_FOLDER Examples/SwiftUI)

# Swift builds under the Ninja and Xcode generators (the Unix Makefiles
# generator is the only one that can't compile Swift). enable_language must run
# at file scope (not inside the eacp_add_swiftui_app function), and this is the
# highest directory common to every SwiftUI app target, so it belongs here.
include(CheckLanguage)
check_language(Swift)

if (CMAKE_Swift_COMPILER)
    enable_language(Swift)
    add_subdirectory(SimpleSwiftUI)
else ()
    message(STATUS
            "SwiftUI apps: no Swift compiler detected (use the Ninja or Xcode "
            "generator on Apple). Skipping.")
endif ()
