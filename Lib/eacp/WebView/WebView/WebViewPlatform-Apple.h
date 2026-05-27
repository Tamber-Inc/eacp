#pragma once

#include "WebView.h"

@class WKWebView;

namespace eacp::Graphics::detail
{
// Shared accessors implemented in WebView.mm so the platform translation
// units can reach the registry and the WKWebView held inside Native
// without pulling Native's full definition into a header.
EA::Vector<WebView*>& registeredWebViews();
WKWebView* wkWebViewOf(WebView* view);

// Platform-specific implementations live in WebViewPlatform-iOS.mm and
// WebViewPlatform-macOS.mm — the CMake IOS branch selects the right TU.
void attachWKWebViewToParent(WKWebView* webView, void* parentHandle);
void applyNativeZoom(WKWebView* webView, double clamped, double& storedZoom);
double readNativeZoom(WKWebView* webView, double storedZoom);
WebView* findFocusedWebView();
} // namespace eacp::Graphics::detail
