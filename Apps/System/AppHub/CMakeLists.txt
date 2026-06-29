add_executable(AppHub Main.cpp)
target_link_libraries(AppHub PRIVATE eacp-updater)

set_default_target_setting(AppHub)
