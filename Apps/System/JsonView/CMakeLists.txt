function(add_json_view_app target title bundle_id variant)
    add_executable(${target} Main.cpp)
    target_link_libraries(${target} PRIVATE eacp-graphics)
    target_compile_definitions(${target}
        PRIVATE
            EACP_JSON_VIEW_TITLE="${title}"
            EACP_JSON_VIEW_VARIANT="${variant}"
            EACP_JSON_VIEW_ASSET_ID="shared.json-view-demo")

    set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${title}"
            MACOSX_BUNDLE_GUI_IDENTIFIER ${bundle_id}
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${bundle_id})

    eacp_set_gui_subsystem(${target})
    set_default_target_setting(${target})
endfunction()

add_json_view_app(JsonView1 "JsonView1" "com.eacp.jsonview1" "JsonView1")
add_json_view_app(JsonView2 "JsonView2" "com.eacp.jsonview2" "JsonView2")
