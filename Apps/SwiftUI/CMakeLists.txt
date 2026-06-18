set(CMAKE_FOLDER Examples/SwiftUI)

# Swift needs the Ninja generator on Apple. enable_language must run at file
# scope (not inside the eacp_add_swiftui_app function), and this is the highest
# directory common to every SwiftUI app target, so it belongs here.
include(CheckLanguage)
check_language(Swift)

if (CMAKE_Swift_COMPILER)
    enable_language(Swift)
    add_subdirectory(SimpleSwiftUI)
else ()
    message(STATUS
            "SwiftUI apps: no Swift compiler detected (configure with -G Ninja "
            "on Apple). Skipping.")
endif ()
