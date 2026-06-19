// Miro Swift runtime — the transport seam shared by the generated client
// and server. Schema-independent (emitted by the `swift-runtime` format,
// the analogue of the TypeScript bridge runtime).

import Foundation

enum MiroError: Error {
    case transport(String)
}

enum MiroDispatchError: Error {
    case unknownCommand(String)
}

// The outgoing seam: hand (command, JSON bytes) to a transport and get JSON
// bytes back. The C-ABI bridge into a C++ Miro::Bridge is one implementation;
// an HTTP or in-process transport is another.
protocol MiroTransport {
    func invoke(_ command: String, _ payload: Data) throws -> Data
}

// Adapts a plain closure to MiroTransport, so `Client(invoke:)` keeps working.
struct MiroClosureTransport: MiroTransport {
    let body: (String, Data) throws -> Data

    init(_ body: @escaping (String, Data) throws -> Data) {
        self.body = body
    }

    func invoke(_ command: String, _ payload: Data) throws -> Data {
        try body(command, payload)
    }
}
