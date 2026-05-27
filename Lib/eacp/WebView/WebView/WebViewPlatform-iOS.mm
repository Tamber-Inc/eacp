#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "WebViewPlatform-Apple.h"

#include <eacp/Core/ObjC/Strings.h>

#include <string>

namespace eacp::Graphics::detail
{
void attachWKWebViewToParent(WKWebView* webView, void* parentHandle)
{
    auto* parentView = (__bridge UIView*) parentHandle;
    [parentView addSubview:webView];
}

void applyNativeZoom(WKWebView* webView, double clamped, double& storedZoom)
{
    storedZoom = clamped;
    auto script = std::string("document.documentElement.style.zoom = '")
        + std::to_string(clamped) + "';";
    [webView evaluateJavaScript:Strings::toNSString(script)
              completionHandler:nil];
}

double readNativeZoom(WKWebView*, double storedZoom)
{
    return storedZoom;
}

WebView* findFocusedWebView()
{
    auto& registered = registeredWebViews();
    return registered.empty() ? nullptr : registered.back();
}
} // namespace eacp::Graphics::detail
