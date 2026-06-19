#include "SwiftUIHost.h"

#include <Miro/Swift/BridgeC.h>

// The Swift side calls these to drive a command into the bound Miro bridge.
// They forward straight to Miro's BridgeC entry points, keeping Miro off the
// Swift import path (Swift only sees SwiftUIHost.h).

extern "C"
{
    char* eacp_swiftui_dispatch(MiroBridge* bridge,
                                const char* command,
                                const char* payloadJson,
                                char** errorOut)
    {
        return miro_bridge_dispatch(bridge, command, payloadJson, errorOut);
    }

    void eacp_swiftui_string_free(char* str)
    {
        miro_string_free(str);
    }
}
