#include "PrivilegedHelperClient.h"

namespace AppHub
{

PrivilegedHelperInstallResult installPrivilegedHelper()
{
    auto result = PrivilegedHelperInstallResult();
    result.ok = false;
    result.error = "privileged helper blessing is only available on macOS";
    return result;
}

eacp::Updater::InstallResult installAppBundleWithPrivilegedHelper(
    const eacp::Updater::PrivilegedAppBundleInstallRequest&)
{
    auto result = eacp::Updater::InstallResult();
    result.ok = false;
    result.error = "privileged helper installs are only available on macOS";
    return result;
}

} // namespace AppHub
