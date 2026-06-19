struct IncrementRequest: Codable {
    var by: Int
}

struct CounterResponse: Codable {
    var value: Int
}

struct GreetRequest: Codable {
    var name: String
}

struct GreetResponse: Codable {
    var message: String
}

