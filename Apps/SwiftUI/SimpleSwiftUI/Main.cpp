#include <eacp/Graphics/Graphics.h>
#include <eacp/SwiftUI/SwiftUIView.h>

using namespace eacp;

// The SwiftUI content lives in swift/RootView.swift, registered under "Root".
// SwiftUIView fetches it across the C-ABI seam and embeds it as a native
// subview, so from the app's point of view it is just another EACP View.
struct MyApp
{
    MyApp() { window.setContentView(ui); }

    Graphics::SwiftUIView ui {"Root"};
    Graphics::Window window;
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
