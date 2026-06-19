#include <eacp/Graphics/Graphics.h>
#include <eacp/SwiftUI/SwiftUIBridge.h>
#include <eacp/SwiftUI/SwiftUIView.h>

#include "cpp/CounterApi.h"

using namespace eacp;

// The SwiftUI content (swift/RootView.swift, registered under "Root") gets a
// typed client into this C++ API: SwiftUIBridge binds CounterApi to a
// Miro::Bridge, and SwiftUIView hands that bridge to the Swift side so its
// generated Client can dispatch commands in-process.
struct MyApp
{
    MyApp() { window.setContentView(ui); }

    Api::CounterApi counter;
    Graphics::SwiftUIBridge bridge {counter};
    Graphics::SwiftUIView ui {"Root", bridge.handle()};
    Graphics::Window window;
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
