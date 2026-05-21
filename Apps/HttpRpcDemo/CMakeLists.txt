miro_export(HttpRpcDemoSchema
        SOURCES    Schema.cpp
        OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated
        OUTPUT_NAME schema
        FORMATS    ts backend ts-server bridge)

miro_export_emit(HttpRpcDemoSchema
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/cpp-generated
        OUTPUT_NAME schema
        FORMATS    cpp-miro cpp-client)

add_executable(HttpRpcServer Server.cpp)
target_link_libraries(HttpRpcServer PRIVATE eacp-network-rpc)
eacp_target_uses_schema(HttpRpcServer HttpRpcDemoSchema HANDLERS)
set_default_target_setting(HttpRpcServer)

add_executable(HttpRpcClient Client.cpp)
target_link_libraries(HttpRpcClient PRIVATE eacp-network-rpc)
eacp_target_uses_schema(HttpRpcClient HttpRpcDemoSchema)
set_default_target_setting(HttpRpcClient)
