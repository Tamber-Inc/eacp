import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

// The SwiftUI content embedded into the EACP window.
struct RootView: View {
    @State private var taps = 0

    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "swift")
                .font(.system(size: 72))
                .foregroundStyle(.orange)
            Text("Hello from SwiftUI")
                .font(.largeTitle.bold())
            Text("hosted inside a native EACP window")
                .foregroundStyle(.secondary)
            Button("Tapped \(taps)") { taps += 1 }
                .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding(40)
    }
}

// Owns the SwiftUI host for as long as the C++ SwiftUIView holds the handle.
// On macOS the host is the NSHostingView itself; on iOS it is a
// UIHostingController whose view we embed.
@MainActor
final class SwiftUIHostBox {
#if os(macOS)
    let view: NSView

    init?(rootKey: String) {
        guard let root = SwiftUIHostBox.makeRoot(rootKey) else { return nil }
        view = NSHostingView(rootView: root)
    }
#else
    let controller: UIViewController
    var view: UIView { controller.view }

    init?(rootKey: String) {
        guard let root = SwiftUIHostBox.makeRoot(rootKey) else { return nil }
        controller = UIHostingController(rootView: root)
    }
#endif

    private static func makeRoot(_ rootKey: String) -> AnyView? {
        switch rootKey {
        case "Root": return AnyView(RootView())
        default: return nil
        }
    }
}

// --- SwiftUIHost.h C-ABI implementation. Calls land on the EACP main thread,
// so assumeIsolated lets the main-actor SwiftUI APIs be used synchronously. ---

// A raw pointer is not Sendable, so it can't be returned across the
// assumeIsolated boundary directly; pass an integer bit pattern out and rebuild
// the pointer on this side (the call is synchronous, same thread throughout).
@_cdecl("eacp_swiftui_host_create")
public func eacp_swiftui_host_create(
    _ rootKey: UnsafePointer<CChar>?
) -> UnsafeMutableRawPointer? {
    let key = rootKey.map { String(cString: $0) } ?? ""
    let bits = MainActor.assumeIsolated { () -> UInt in
        guard let box = SwiftUIHostBox(rootKey: key) else { return 0 }
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
