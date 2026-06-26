#include <eacp/Core/Threads/Timer.h>
#include <eacp/Graphics/Graphics.h>
#include <eacp/SwiftUI/SwiftUIBridge.h>
#include <eacp/SwiftUI/SwiftUIView.h>

#include <ea_data_structures/Pointers/Broadcaster.h>

#include "cpp/CounterApi.h"

using namespace eacp;

// The SwiftUI content (swift/RootView.swift, registered under "Root") gets a
// typed client into this C++ API (commands), and the API pushes its count back
// out through `changes` (events). SwiftUIBridge binds CounterApi for commands;
// the listener below serialises each new count and delivers it to the view,
// which SwiftUI observes. A 1 Hz timer ticks the count from C++ to show the
// push channel working with no SwiftUI action at all.
struct MyApp
{
    MyApp() { window.setContentView(ui); }

    void pushCount()
    {
        // toJSON yields a Json::Value (an object); print() turns it into the
        // JSON text the Swift sink decodes. Passing the Value straight to a
        // std::string would hit its string-only conversion and throw.
        ui.deliverEvent("count",
                        Miro::Json::print(Miro::toJSON(counter.changes.snapshot())));
    }

    Api::CounterApi counter;
    Graphics::SwiftUIBridge bridge {counter};
    Graphics::SwiftUIView ui {"Root", bridge.handle()};
    Graphics::Window window;

    EA::Listener countChanged {counter.changes.broadcaster(),
                               [this] { pushCount(); },
                               EA::Listener::Modes::TriggerOnEvent};

    Threads::Timer timer {[this] { counter.tick(); }, 1};
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
