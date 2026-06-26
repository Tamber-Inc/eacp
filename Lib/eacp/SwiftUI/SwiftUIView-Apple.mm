#import <Foundation/Foundation.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include "SwiftUIView.h"
#include "SwiftUIHost.h"

namespace eacp::Graphics
{

#if TARGET_OS_IPHONE
using PlatformView = UIView;
#else
using PlatformView = NSView;
#endif

namespace
{
PlatformView* hostedView(EacpSwiftUIHost* host)
{
    return (__bridge PlatformView*) eacp_swiftui_host_view(host);
}

void makeFlexible(PlatformView* hosted)
{
#if TARGET_OS_IPHONE
    hosted.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
#else
    hosted.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
#endif
}
} // namespace

struct SwiftUIView::Native
{
    explicit Native(SwiftUIView& viewToUse)
        : view(viewToUse)
    {
        host = eacp_swiftui_host_create(view.rootKey.c_str(),
                                        view.bridgeHandle,
                                        &eacp_swiftui_dispatch,
                                        &eacp_swiftui_string_free);

        if (host == nullptr)
            return;

        auto* hosted = hostedView(host);
        auto* parent = (__bridge PlatformView*) view.getHandle();

        if (hosted == nullptr || parent == nullptr)
            return;

        hosted.frame = parent.bounds;
        makeFlexible(hosted);
        [parent addSubview:hosted];
    }

    ~Native()
    {
        if (host == nullptr)
            return;

        [hostedView(host) removeFromSuperview];
        eacp_swiftui_host_destroy(host);
    }

    void updateSize()
    {
        if (host == nullptr)
            return;

        auto* parent = (__bridge PlatformView*) view.getHandle();
        hostedView(host).frame = parent.bounds;
    }

    void deliverEvent(const std::string& name, const std::string& json)
    {
        if (host != nullptr)
            eacp_swiftui_host_deliver_event(host, name.c_str(), json.c_str());
    }

    SwiftUIView& view;
    EacpSwiftUIHost* host = nullptr;
};

SwiftUIView::SwiftUIView(std::string rootKeyToUse, MiroBridge* bridge)
    : rootKey(std::move(rootKeyToUse))
    , bridgeHandle(bridge)
    , impl(*this)
{
}

SwiftUIView::~SwiftUIView() = default;

void SwiftUIView::resized()
{
    View::resized();
    impl->updateSize();
}

void SwiftUIView::deliverEvent(const std::string& name, const std::string& json)
{
    impl->deliverEvent(name, json);
}

} // namespace eacp::Graphics
