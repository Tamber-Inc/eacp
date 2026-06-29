#include "PrivilegedHelperClient.h"

#include <Miro/Miro.h>

#import <Foundation/Foundation.h>
#import <Security/Authorization.h>
#import <ServiceManagement/ServiceManagement.h>

@protocol EACPAppHubPrivilegedHelperProtocol
- (void)invokeCommand:(NSString*)command
              payload:(NSString*)payload
            withReply:(void (^)(NSString* reply))reply;
@end

namespace AppHub
{
namespace
{

std::string stringFromCFString(CFStringRef value)
{
    if (value == nullptr)
        return {};

    auto length = CFStringGetLength(value);
    auto maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    auto buffer = std::string(static_cast<std::size_t>(maxSize), '\0');
    if (!CFStringGetCString(value,
                            buffer.data(),
                            maxSize,
                            kCFStringEncodingUTF8))
        return {};

    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    return buffer;
}

std::string stringFromCFError(CFErrorRef error)
{
    if (error == nullptr)
        return {};

    auto description = CFErrorCopyDescription(error);
    auto out = stringFromCFString(description);
    if (description != nullptr)
        CFRelease(description);
    return out;
}

NSString* nsString(const std::string& value)
{
    return [NSString stringWithUTF8String:value.c_str()];
}

std::string stringFromNSString(NSString* value)
{
    if (value == nil)
        return {};
    return {[value UTF8String]};
}

} // namespace

PrivilegedHelperInstallResult installPrivilegedHelper()
{
    auto result = PrivilegedHelperInstallResult();

    AuthorizationRef auth = nullptr;
    auto status = AuthorizationCreate(nullptr,
                                      kAuthorizationEmptyEnvironment,
                                      kAuthorizationFlagDefaults,
                                      &auth);
    if (status != errAuthorizationSuccess)
    {
        result.error = "AuthorizationCreate failed";
        return result;
    }

    AuthorizationItem right = {
        kSMRightBlessPrivilegedHelper,
        0,
        nullptr,
        0,
    };
    AuthorizationRights rights = {1, &right};
    auto flags = static_cast<AuthorizationFlags>(
        kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed
        | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights);

    status = AuthorizationCopyRights(auth,
                                     &rights,
                                     kAuthorizationEmptyEnvironment,
                                     flags,
                                     nullptr);
    if (status != errAuthorizationSuccess)
    {
        AuthorizationFree(auth, kAuthorizationFlagDefaults);
        result.error = "AuthorizationCopyRights failed";
        return result;
    }

    CFErrorRef error = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    auto blessed = SMJobBless(kSMDomainSystemLaunchd,
                              CFSTR(EACP_APPHUB_HELPER_LABEL),
                              auth,
                              &error);
#pragma clang diagnostic pop
    AuthorizationFree(auth, kAuthorizationFlagDefaults);

    if (!blessed)
    {
        result.error = "SMJobBless failed";
        auto description = stringFromCFError(error);
        if (error != nullptr)
            CFRelease(error);
        if (!description.empty())
            result.error += ": " + description;
        return result;
    }

    result.ok = true;
    return result;
}

eacp::Updater::InstallResult installAppBundleWithPrivilegedHelper(
    const eacp::Updater::PrivilegedAppBundleInstallRequest& request)
{
    auto connection =
        [[NSXPCConnection alloc] initWithMachServiceName:@EACP_APPHUB_HELPER_LABEL
                                                 options:NSXPCConnectionPrivileged];
    connection.remoteObjectInterface =
        [NSXPCInterface interfaceWithProtocol:
                            @protocol(EACPAppHubPrivilegedHelperProtocol)];

    __block auto result = eacp::Updater::InstallResult();
    result.ok = false;
    result.error = "privileged helper did not reply";

    auto semaphore = dispatch_semaphore_create(0);
    connection.invalidationHandler = ^{
      if (!result.ok && result.error == "privileged helper did not reply")
          result.error = "privileged helper connection invalidated";
      dispatch_semaphore_signal(semaphore);
    };
    connection.interruptionHandler = ^{
      if (!result.ok)
          result.error = "privileged helper connection interrupted";
      dispatch_semaphore_signal(semaphore);
    };
    [connection resume];

    auto payload = Miro::toJSONString(request);
    id remote = [connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
      result.error = "privileged helper connection failed: "
                   + stringFromNSString(error.localizedDescription);
      dispatch_semaphore_signal(semaphore);
    }];

    [(id<EACPAppHubPrivilegedHelperProtocol>) remote
        invokeCommand:@"installAppBundle"
              payload:nsString(payload)
            withReply:^(NSString* reply) {
              try
              {
                  Miro::fromJSONString(result, stringFromNSString(reply));
              }
              catch (const std::exception& e)
              {
                  result.ok = false;
                  result.error = std::string("invalid privileged helper reply: ")
                               + e.what();
              }
              dispatch_semaphore_signal(semaphore);
            }];

    auto timeout =
        dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(120 * NSEC_PER_SEC));
    if (dispatch_semaphore_wait(semaphore, timeout) != 0)
    {
        result.ok = false;
        result.error = "privileged helper timed out";
    }

    [connection invalidate];
    return result;
}

} // namespace AppHub
