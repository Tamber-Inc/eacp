add_executable(HttpRpcClient Client.cpp)
target_link_libraries(HttpRpcClient PRIVATE eacp-network-rpc)
target_include_directories(HttpRpcClient PRIVATE
        ${WEBVIEWEMBED_CPP_GENERATED_DIR})
add_dependencies(HttpRpcClient WebViewEmbedSchema)
set_default_target_setting(HttpRpcClient)

add_executable(HttpRpcServer
        Server.cpp
        ${CMAKE_SOURCE_DIR}/Apps/WebViewEmbed/Commands.cpp)
target_link_libraries(HttpRpcServer PRIVATE eacp-network-rpc)
target_include_directories(HttpRpcServer PRIVATE
        ${WEBVIEWEMBED_CPP_GENERATED_DIR}
        ${CMAKE_SOURCE_DIR}/Apps/WebViewEmbed)
add_dependencies(HttpRpcServer WebViewEmbedSchema)
set_default_target_setting(HttpRpcServer)
