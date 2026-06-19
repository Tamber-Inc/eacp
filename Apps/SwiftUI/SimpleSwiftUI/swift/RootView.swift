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

// The count is owned by C++; this view reads and mutates it through the typed
// client. (No live push yet — the displayed value is refreshed from each
// command's response. Reactive state arrives in the events milestone.)
struct RootView: View {
    let client: Client?

    @State private var count = 0
    @State private var greeting = ""
    @State private var errorText: String?

    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "swift")
                .font(.system(size: 64))
                .foregroundColor(.orange)
            Text("Count (owned by C++): \(count)")
                .font(Font.title.weight(.bold))
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
        .onAppear(perform: refresh)
    }

    private func increment() {
        guard let client else { errorText = "no bridge"; return }
        do {
            count = try client.increment(IncrementRequest(by: 1)).value
            greeting = try client.greet(GreetRequest(name: "SwiftUI")).message
        } catch {
            errorText = "\(error)"
        }
    }

    private func refresh() {
        guard let client else { return }
        count = (try? client.getCount().value) ?? 0
    }
}

// Owns the SwiftUI host for as long as the C++ SwiftUIView holds the handle.
@MainActor
final class SwiftUIHostBox {
#if os(macOS)
    let view: NSView

    init?(_ root: AnyView?) {
        guard let root else { return nil }
        view = NSHostingView(rootView: root)
    }
#else
    let controller: UIViewController
    var view: UIView { controller.view }

    init?(_ root: AnyView?) {
        guard let root else { return nil }
        controller = UIHostingController(rootView: root)
    }
#endif

    static func makeRoot(
        _ rootKey: String,
        _ bridge: OpaquePointer?,
        _ dispatch: EacpSwiftUIDispatchFn?,
        _ stringFree: EacpSwiftUIStringFreeFn?
    ) -> AnyView? {
        switch rootKey {
        case "Root":
            return AnyView(RootView(client: makeClient(bridge, dispatch, stringFree)))
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
        let root = SwiftUIHostBox.makeRoot(key, bridge, dispatch, stringFree)
        guard let box = SwiftUIHostBox(root) else { return 0 }
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
