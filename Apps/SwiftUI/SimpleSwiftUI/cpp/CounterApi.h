#pragma once

// The single source of truth for the SwiftUI <-> C++ command contract. The same
// reflect(ApiReflector&) body both binds the API to a Miro::Bridge at runtime
// (SwiftUIBridge) and drives Swift codegen (Schema.swift + Schema.client.swift).
// Add a command here and it shows up on the generated Swift Client with no other
// edits.

#include <Miro/Miro.h>

#include <string>

namespace Api
{
struct IncrementRequest
{
    int by = 1;

    MIRO_REFLECT(by)
};

struct CounterResponse
{
    int value = 0;

    MIRO_REFLECT(value)
};

struct GreetRequest
{
    std::string name;

    MIRO_REFLECT(name)
};

struct GreetResponse
{
    std::string message;

    MIRO_REFLECT(message)
};

// The count lives here, in C++ — SwiftUI calls in to mutate and read it.
class CounterApi
{
public:
    void reflect(Miro::ApiReflector& r)
    {
        r.command(&CounterApi::increment, "increment");
        r.command(&CounterApi::getCount, "getCount");
        r.command(&CounterApi::greet, "greet");
    }

    CounterResponse increment(const IncrementRequest& req)
    {
        count += req.by;
        return {count};
    }

    CounterResponse getCount() const { return {count}; }

    GreetResponse greet(const GreetRequest& req)
    {
        return {"Hello from C++, " + req.name + "!"};
    }

private:
    int count = 0;
};
} // namespace Api
