add_library(eacp-swiftui STATIC)

target_link_libraries(eacp-swiftui PUBLIC eacp-graphics)

# SwiftUI hosting is Apple-only. The C++ side only touches AppKit/UIKit (it
# embeds the native view Swift hands back); the SwiftUI framework itself is
# linked by the app's Swift module, not here.
if (APPLE)
    target_sources(eacp-swiftui PRIVATE SwiftUIView-Apple.mm)
    target_link_libraries(eacp-swiftui PRIVATE "-framework Foundation")

    if (IOS)
        target_link_libraries(eacp-swiftui PRIVATE "-framework UIKit")
    else ()
        target_link_libraries(eacp-swiftui PRIVATE "-framework Cocoa")
    endif ()
endif ()

# Make the eacp_add_swiftui_app() helper available to app CMakeLists. Functions
# are global once defined, and Lib is processed before Apps.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EacpSwiftUIApp.cmake)

set_default_target_setting(eacp-swiftui)
eacp_enable_unity_build(eacp-swiftui)
add_ide_sources(eacp-swiftui)
