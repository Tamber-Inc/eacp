#pragma once

#include <eacp/Graphics/View/View.h>

#include "SwiftUIHost.h"

#include <string>

namespace eacp::Graphics
{
// A View that hosts a SwiftUI view. The SwiftUI content is authored in Swift
// and registered under a string key; this C++ side requests it across a
// plain-C seam (SwiftUIHost.h), embeds the returned NSHostingView (macOS) or
// UIHostingController view (iOS) as a native subview, and keeps it sized to the
// view. It lives in the normal eacp::Graphics::View hierarchy and so composes
// with GPUView, WebView and painted Views in one window. Apple-only.
class SwiftUIView : public View
{
public:
    // rootKey selects the Swift-registered SwiftUI view. Pass a bound command
    // bridge handle (SwiftUIBridge::handle()) to give that view a typed client
    // into C++; leave it null for view-only hosting.
    explicit SwiftUIView(std::string rootKey, MiroBridge* bridge = nullptr);
    ~SwiftUIView() override;

    void resized() override;

private:
    std::string rootKey;
    MiroBridge* bridgeHandle = nullptr;

    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::Graphics
