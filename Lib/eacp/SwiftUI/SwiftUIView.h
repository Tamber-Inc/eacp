#pragma once

#include <eacp/Graphics/View/View.h>

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
    explicit SwiftUIView(std::string rootKey);
    ~SwiftUIView() override;

    void resized() override;

private:
    std::string rootKey;

    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::Graphics
