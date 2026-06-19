// Typed client: one method per command over a MiroTransport.
import Foundation

final class Client {
    private let transport: MiroTransport
    private let encoder = JSONEncoder()
    private let decoder = JSONDecoder()

    init(_ transport: MiroTransport) {
        self.transport = transport
    }

    // Convenience: wrap a plain closure as the transport.
    convenience init(
        invoke: @escaping (_ command: String, _ payload: Data) throws -> Data
    ) {
        self.init(MiroClosureTransport(invoke))
    }

    func increment(_ req: IncrementRequest) throws -> CounterResponse {
        let payload = try encoder.encode(req)
        let result = try transport.invoke("increment", payload)
        return try decoder.decode(CounterResponse.self, from: result)
    }

    func getCount() throws -> CounterResponse {
        let payload = Data("{}".utf8)
        let result = try transport.invoke("getCount", payload)
        return try decoder.decode(CounterResponse.self, from: result)
    }

    func greet(_ req: GreetRequest) throws -> GreetResponse {
        let payload = try encoder.encode(req)
        let result = try transport.invoke("greet", payload)
        return try decoder.decode(GreetResponse.self, from: result)
    }
}
