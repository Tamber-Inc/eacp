#pragma once

// Plain-C ABI seam between the eacp-swiftui C++ layer and the app's Swift
// module. The host entry points are implemented in Swift (@_cdecl) and called
// from the C++ SwiftUIView; the dispatch pair runs the other way — Swift calls
// it to drive a typed command into the bound Miro bridge on the C++ side.
//
// Pure C so Swift can import it via -import-objc-header with no C++ or Miro on
// the import path (the same approach Miro's BridgeC.h uses for its clients).

#ifdef __cplusplus
extern "C"
{
#endif

    // Opaque handle to a SwiftUI host: an NSHostingView on macOS or a
    // UIHostingController on iOS, kept alive by the Swift side for as long as
    // the C++ owner holds this handle.
    typedef struct EacpSwiftUIHost EacpSwiftUIHost;

    // Opaque handle to the bound Miro::Bridge on the C++ side. Declared with
    // the same tag as Miro's BridgeC.h MiroBridge, so the pointer is
    // interchangeable while keeping Miro headers off the Swift import path.
    typedef struct MiroBridge MiroBridge;

    // The dispatch + free routines, handed to Swift as function pointers at
    // host creation. Passing pointers (rather than letting the Swift framework
    // bind these symbols from the host executable) avoids a cross-image
    // undefined symbol under the default two-level namespace.
    typedef char* (*EacpSwiftUIDispatchFn)(MiroBridge* bridge,
                                           const char* command,
                                           const char* payloadJson,
                                           char** errorOut);
    typedef void (*EacpSwiftUIStringFreeFn)(char* str);

    // --- Implemented in Swift (@_cdecl) ---

    // Builds the host for the SwiftUI view registered under rootKey, wiring it
    // to the bound command bridge through the supplied dispatch/free callbacks
    // (all three null for view-only hosting). Returns an owned handle (release
    // with eacp_swiftui_host_destroy), or null when the key is unknown.
    EacpSwiftUIHost* eacp_swiftui_host_create(const char* rootKey,
                                              MiroBridge* bridge,
                                              EacpSwiftUIDispatchFn dispatch,
                                              EacpSwiftUIStringFreeFn stringFree);

    // The native view to embed in the EACP view tree: an NSView* on macOS, a
    // UIView* on iOS. The handle owns it — embed it, but do not release it.
    void* eacp_swiftui_host_view(EacpSwiftUIHost* host);

    // Releases the host and the SwiftUI view/controller it owns.
    void eacp_swiftui_host_destroy(EacpSwiftUIHost* host);

    // Delivers a named event payload (JSON) to the host's observable model,
    // which SwiftUI observes — the C++ -> SwiftUI push channel. Called on the
    // main thread (where EACP state changes fire), so the Swift side updates
    // its @Published model synchronously.
    void eacp_swiftui_host_deliver_event(EacpSwiftUIHost* host,
                                         const char* eventName,
                                         const char* payloadJson);

    // --- Implemented in eacp-swiftui (C++), called from Swift ---

    // Dispatches one command synchronously through the bound bridge. Same
    // JSON-in / JSON-out + error + ownership contract as miro_bridge_dispatch:
    // returns owned JSON text (free with eacp_swiftui_string_free) on success,
    // or null and a *errorOut message on failure.
    char* eacp_swiftui_dispatch(MiroBridge* bridge,
                                const char* command,
                                const char* payloadJson,
                                char** errorOut);

    // Releases a string returned by eacp_swiftui_dispatch (result or error).
    void eacp_swiftui_string_free(char* str);

#ifdef __cplusplus
}
#endif
