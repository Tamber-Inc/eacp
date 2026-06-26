import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

enum BridgeError: Error { case dispatch(String) }

// Wraps the C-ABI dispatch seam as a typed Client bound to the C++ bridge
// handle the host was created with, calling through the dispatch/free function
// pointers the host supplied. Every call crosses Swift -> C ABI -> Miro::Bridge
// -> a CounterApi handler in C++ and back, decoding the JSON response into the
// generated Swift types. Returns nil when there is no bridge.
private func makeClient(
    _ bridge: OpaquePointer?,
    _ dispatch: EacpSwiftUIDispatchFn?,
    _ stringFree: EacpSwiftUIStringFreeFn?
) -> Client? {
    guard let bridge, let dispatch, let stringFree else { return nil }

    return Client(invoke: { command, payload in
        let payloadStr = String(data: payload, encoding: .utf8) ?? "{}"

        var errPtr: UnsafeMutablePointer<CChar>?
        let resultPtr = command.withCString { cmd in
            payloadStr.withCString { pl in
                dispatch(bridge, cmd, pl, &errPtr)
            }
        }

        guard let resultPtr else {
            let message = errPtr.map { String(cString: $0) } ?? "unknown error"
            stringFree(errPtr)
            throw BridgeError.dispatch(message)
        }

        let data = Data(String(cString: resultPtr).utf8)
        stringFree(resultPtr)
        return data
    })
}

// Observable state the SwiftUI view renders. Updated from C++ through the event
// sink (eacp_swiftui_host_deliver_event); @Published drives the re-render.
final class AppModel: ObservableObject {
    @Published var count = 0
}

// The count is owned by C++. Commands (the button) mutate it; the new value
// arrives back through the event channel and updates `model`, so this view
// never holds its own copy of the count.
struct RootView: View {
    @ObservedObject var model: AppModel
    let client: Client?

    @State private var greeting = ""
    @State private var errorText: String?

    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "swift")
                .font(.system(size: 64))
                .foregroundColor(.orange)
            Text("Count (owned by C++): \(model.count)")
                .font(Font.title.weight(.bold))
            Text("incrementing from a C++ timer — pushed live")
                .font(.footnote)
                .foregroundColor(.secondary)
            Button("Increment via C++") { increment() }
            if !greeting.isEmpty {
                Text(greeting).foregroundColor(.secondary)
            }
            if let errorText {
                Text(errorText).foregroundColor(.red)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding(40)
    }

    private func increment() {
        guard let client else { errorText = "no bridge"; return }
        do {
            // C++ increments and publishes; the new value comes back over the
            // event channel, so we deliberately don't set the count here.
            _ = try client.increment(IncrementRequest(by: 1))
            greeting = try client.greet(GreetRequest(name: "SwiftUI")).message
        } catch {
            errorText = "\(error)"
        }
    }
}

// Owns the SwiftUI host (and its model) for as long as the C++ SwiftUIView
// holds the handle.
@MainActor
final class SwiftUIHostBox {
    let model = AppModel()

#if os(macOS)
    let view: NSView
#else
    let controller: UIViewController
    var view: UIView { controller.view }
#endif

    init?(
        rootKey: String,
        bridge: OpaquePointer?,
        dispatch: EacpSwiftUIDispatchFn?,
        stringFree: EacpSwiftUIStringFreeFn?
    ) {
        guard let root = SwiftUIHostBox.makeRoot(
            rootKey, model, bridge, dispatch, stringFree
        ) else { return nil }

#if os(macOS)
        view = NSHostingView(rootView: root)
#else
        controller = UIHostingController(rootView: root)
#endif
    }

    func deliver(event: String, payload: String) {
        switch event {
        case "count":
            if let data = payload.data(using: .utf8),
               let response = try? JSONDecoder().decode(CounterResponse.self, from: data) {
                model.count = response.value
            }
        default:
            break
        }
    }

    private static func makeRoot(
        _ rootKey: String,
        _ model: AppModel,
        _ bridge: OpaquePointer?,
        _ dispatch: EacpSwiftUIDispatchFn?,
        _ stringFree: EacpSwiftUIStringFreeFn?
    ) -> AnyView? {
        switch rootKey {
        case "Root":
            return AnyView(
                RootView(model: model, client: makeClient(bridge, dispatch, stringFree)))
        default:
            return nil
        }
    }
}

// --- SwiftUIHost.h C-ABI implementation. Calls land on the EACP main thread,
// so assumeIsolated lets the main-actor SwiftUI APIs be used synchronously. A
// raw pointer is not Sendable, so pass a UInt bit pattern out of the isolated
// closure and rebuild the pointer on this side. ---

@_cdecl("eacp_swiftui_host_create")
public func eacp_swiftui_host_create(
    _ rootKey: UnsafePointer<CChar>?,
    _ bridge: OpaquePointer?,
    _ dispatch: EacpSwiftUIDispatchFn?,
    _ stringFree: EacpSwiftUIStringFreeFn?
) -> UnsafeMutableRawPointer? {
    let key = rootKey.map { String(cString: $0) } ?? ""
    let bits = MainActor.assumeIsolated { () -> UInt in
        guard let box = SwiftUIHostBox(
            rootKey: key, bridge: bridge, dispatch: dispatch, stringFree: stringFree
        ) else { return 0 }
        return UInt(bitPattern: Unmanaged.passRetained(box).toOpaque())
    }
    return UnsafeMutableRawPointer(bitPattern: bits)
}

@_cdecl("eacp_swiftui_host_view")
public func eacp_swiftui_host_view(
    _ handle: UnsafeMutableRawPointer?
) -> UnsafeMutableRawPointer? {
    guard let handle else { return nil }
    let box = Unmanaged<SwiftUIHostBox>.fromOpaque(handle).takeUnretainedValue()
    let bits = MainActor.assumeIsolated { () -> UInt in
        UInt(bitPattern: Unmanaged.passUnretained(box.view).toOpaque())
    }
    return UnsafeMutableRawPointer(bitPattern: bits)
}

@_cdecl("eacp_swiftui_host_destroy")
public func eacp_swiftui_host_destroy(_ handle: UnsafeMutableRawPointer?) {
    guard let handle else { return }
    Unmanaged<SwiftUIHostBox>.fromOpaque(handle).release()
}

@_cdecl("eacp_swiftui_host_deliver_event")
public func eacp_swiftui_host_deliver_event(
    _ handle: UnsafeMutableRawPointer?,
    _ name: UnsafePointer<CChar>?,
    _ payload: UnsafePointer<CChar>?
) {
    guard let handle else { return }
    let box = Unmanaged<SwiftUIHostBox>.fromOpaque(handle).takeUnretainedValue()
    let eventName = name.map { String(cString: $0) } ?? ""
    let json = payload.map { String(cString: $0) } ?? "{}"
    MainActor.assumeIsolated { box.deliver(event: eventName, payload: json) }
}
