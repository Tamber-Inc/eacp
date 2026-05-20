#include "Types.h"

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

    WebView webView {embeddedOptions("TodoApp")};
    WebViewBridge transport {webView};
    Window window;
    Listener bridgeBinding =
        bindToBridge(todoState(), transport.getBridge(), "todos");
};

int main()
{
    eacp::Apps::run<MyApp>();

    return 0;
}
