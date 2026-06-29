#include <eacp/Updater/Updater.h>

#include <Miro/Miro.h>

#import <Foundation/Foundation.h>

#include <iostream>
#include <string>

#ifndef EACP_APPHUB_HELPER_ALLOWED_TEAM_ID
#define EACP_APPHUB_HELPER_ALLOWED_TEAM_ID "MBHR5VAUVQ"
#endif

@protocol EACPAppHubPrivilegedHelperProtocol
- (void)invokeCommand:(NSString*)command
              payload:(NSString*)payload
            withReply:(void (^)(NSString* reply))reply;
@end

namespace
{
std::string stringFromNSString(NSString* value)
{
    if (value == nil)
        return {};
    return std::string([value UTF8String]);
}

NSString* nsString(const std::string& value)
{
    return [NSString stringWithUTF8String:value.c_str()];
}

eacp::Updater::InstallResult handleInstallAppBundle(const std::string& payload)
{
    auto request = eacp::Updater::PrivilegedAppBundleInstallRequest();
    Miro::fromJSONString(request, payload);

    request.requiredTeamIdentifier = EACP_APPHUB_HELPER_ALLOWED_TEAM_ID;
    return eacp::Updater::installAppBundleArtifact(request);
}

eacp::Updater::InstallResult handleCommand(const std::string& command,
                                           const std::string& payload)
{
    if (command == "installAppBundle")
        return handleInstallAppBundle(payload);

    auto result = eacp::Updater::InstallResult();
    result.ok = false;
    result.error = "unknown privileged helper command";
    return result;
}
} // namespace

@interface EACPAppHubPrivilegedHelper
    : NSObject <EACPAppHubPrivilegedHelperProtocol>
@end

@implementation EACPAppHubPrivilegedHelper
- (void)invokeCommand:(NSString*)command
              payload:(NSString*)payload
            withReply:(void (^)(NSString* reply))reply
{
    auto result = eacp::Updater::InstallResult();
    try
    {
        result =
            handleCommand(stringFromNSString(command), stringFromNSString(payload));
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error = e.what();
    }
    catch (...)
    {
        result.ok = false;
        result.error = "unknown privileged helper error";
    }

    reply(nsString(Miro::toJSONString(result)));
}
@end

@interface EACPAppHubPrivilegedHelperDelegate
    : NSObject <NSXPCListenerDelegate>
@end

@implementation EACPAppHubPrivilegedHelperDelegate
- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)connection
{
    (void) listener;

    connection.exportedInterface =
        [NSXPCInterface interfaceWithProtocol:
                            @protocol(EACPAppHubPrivilegedHelperProtocol)];
    connection.exportedObject = [EACPAppHubPrivilegedHelper new];
    [connection resume];
    return YES;
}
@end

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "--version")
    {
        std::cout << EACP_APPHUB_HELPER_LABEL << " 1.0.0\n";
        return 0;
    }

    @autoreleasepool
    {
        auto* delegate = [EACPAppHubPrivilegedHelperDelegate new];
        auto* listener = [[NSXPCListener alloc] initWithMachServiceName:
                                                  @EACP_APPHUB_HELPER_LABEL];
        listener.delegate = delegate;
        [listener resume];
        [[NSRunLoop currentRunLoop] run];
    }

    return 0;
}
