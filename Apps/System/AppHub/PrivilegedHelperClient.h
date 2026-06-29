#pragma once

#include <eacp/Updater/Updater.h>

#include <string>

namespace AppHub
{

struct PrivilegedHelperInstallResult
{
    bool ok = false;
    std::string error;
};

PrivilegedHelperInstallResult installPrivilegedHelper();
eacp::Updater::InstallResult installAppBundleWithPrivilegedHelper(
    const eacp::Updater::PrivilegedAppBundleInstallRequest& request);

} // namespace AppHub
