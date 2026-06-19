eacp_add_swiftui_app(SimpleSwiftUI
        SOURCES       Main.cpp
        SWIFT_SOURCES swift/RootView.swift
        API           Api::CounterApi
        API_HEADER    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/CounterApi.h
        BUNDLE_ID     "com.eacp.simpleswiftui"
        BUNDLE_NAME   "Simple SwiftUI")
