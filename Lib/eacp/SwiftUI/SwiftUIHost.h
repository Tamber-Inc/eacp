#pragma once

// Plain-C ABI seam between the eacp-swiftui C++ layer and the app's Swift
// module. The Swift side implements these entry points with @_cdecl; the C++
// SwiftUIView calls them. The header is deliberately pure C so Swift can import
// it directly via -import-objc-header with no C++ on the import path (the same
// approach Miro's BridgeC.h uses for its Swift clients).

#ifdef __cplusplus
extern "C"
{
#endif

    // Opaque handle to a SwiftUI host: an NSHostingView on macOS or a
    // UIHostingController on iOS, kept alive by the Swift side for as long as
    // the C++ owner holds this handle.
    typedef struct EacpSwiftUIHost EacpSwiftUIHost;

    // Builds the host for the SwiftUI view registered under rootKey. Returns an
    // owned handle (release with eacp_swiftui_host_destroy), or null when the
    // key is unknown.
    EacpSwiftUIHost* eacp_swiftui_host_create(const char* rootKey);

    // The native view to embed in the EACP view tree: an NSView* on macOS, a
    // UIView* on iOS. The handle owns it — embed it, but do not release it.
    void* eacp_swiftui_host_view(EacpSwiftUIHost* host);

    // Releases the host and the SwiftUI view/controller it owns.
    void eacp_swiftui_host_destroy(EacpSwiftUIHost* host);

#ifdef __cplusplus
}
#endif
