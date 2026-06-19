#pragma once

#include "SwiftUIHost.h"

#include <Miro/Miro.h>
#include <Miro/Swift/BridgeC.h>

namespace eacp::Graphics
{
// Binds a reflected C++ API to a Miro::Bridge and exposes it as the opaque
// MiroBridge handle a SwiftUIView hands to its Swift module, so the generated
// Swift Client can dispatch typed commands straight into C++ — in-process and
// synchronous, no IPC.
//
// The API must outlive this bridge, so declare it before the SwiftUIBridge
// member in your app struct (members destruct in reverse order):
//
//     Api::CounterApi    counter;
//     SwiftUIBridge      bridge {counter};
//     SwiftUIView        ui {"Root", bridge.handle()};
class SwiftUIBridge
{
public:
    template <class Api>
    explicit SwiftUIBridge(Api& api)
    {
        bridge.use(api);
    }

    MiroBridge* handle() { return reinterpret_cast<MiroBridge*>(&bridge); }

private:
    Miro::Bridge bridge;
};
} // namespace eacp::Graphics
