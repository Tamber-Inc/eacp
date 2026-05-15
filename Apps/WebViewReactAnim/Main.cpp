#include "Types.h"

#include <eacp/Core/Threads/Timer.h>
#include <eacp/WebView/WebView.h>

using namespace eacp;
using namespace Graphics;

struct MyApp
{
    MyApp()
    {
        setApplicationMenuBar(buildDefaultWebViewMenuBar());
        window.setContentView(webView);
    }

    WebView webView {embeddedOptions("ReactAnimApp")};
    WebViewBridge transport {webView};
    Window window;
    Threads::Timer timer {
        [this] { transport.getBridge().emit("tick", getCurrentTick()); }, 120};
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
