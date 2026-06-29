nano_add_executable(AppHubTests
        SOURCES AppHubApiTests.cpp
        TARGETS eacp-updater eacp-network)

target_include_directories(AppHubTests PRIVATE
        ${CMAKE_SOURCE_DIR}/Apps/System/AppHub)

target_compile_definitions(AppHubTests PRIVATE
        EACP_APPHUB_CATALOG_URL="file:///dev/null"
        EACP_APPHUB_DEMO_MANIFEST_URL="file:///dev/null"
        EACP_APPHUB_MANIFEST_URL="file:///dev/null")
