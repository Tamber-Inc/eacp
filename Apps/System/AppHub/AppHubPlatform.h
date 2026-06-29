#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <eacp/Updater/Updater.h>

namespace AppHub
{

std::filesystem::path defaultStateRoot();
eacp::Updater::Target currentTarget();
std::filesystem::path installedApplicationsRoot();
std::filesystem::path installedAppBundlePath(std::string_view bundleName);
std::filesystem::path installedDemoAppBundlePath();
std::filesystem::path installedDemoAppExecutablePath();
std::filesystem::path installedHubAppBundlePath();
std::filesystem::path installedHubAppExecutablePath();
std::optional<std::filesystem::path> currentExecutablePath();
bool createAppBundleZip(const std::filesystem::path& bundle,
                        const std::filesystem::path& output);

struct LaunchResult
{
    bool ok = false;
    std::string error;
};

using PlatformResult = LaunchResult;

LaunchResult openAppBundle(std::string_view appPath);
LaunchResult openNewAppBundleInstance(std::string_view appPath);
PlatformResult directInstallAppBundle(
    const std::filesystem::path& root,
    const eacp::Updater::RemoteAppManifest& manifest,
    const std::filesystem::path& artifactPath);

} // namespace AppHub
