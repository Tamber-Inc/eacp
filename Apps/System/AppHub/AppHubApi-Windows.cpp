#include "AppHubPlatform.h"

#include <eacp/Core/Process/Process.h>

#if defined(_WIN32)
#include <windows.h>

#include <array>
#include <cstdlib>
#endif

namespace AppHub
{
namespace fs = std::filesystem;
namespace Processes = eacp::Processes;

fs::path defaultStateRoot()
{
#if defined(_WIN32)
    if (auto* data = std::getenv("ProgramData"))
        return fs::path(data) / "Tamber" / "AppHub";
    return fs::temp_directory_path() / "Tamber" / "AppHub";
#else
    return fs::temp_directory_path() / "Tamber" / "AppHub";
#endif
}

eacp::Updater::Target currentTarget()
{
    auto target = eacp::Updater::Target();
    target.platform = eacp::Updater::Platform::Windows;
#if defined(_M_ARM64) || defined(__aarch64__)
    target.architecture = eacp::Updater::Architecture::Arm64;
#else
    target.architecture = eacp::Updater::Architecture::X64;
#endif
    return target;
}

fs::path installedApplicationsRoot()
{
    // Share the helper's protected root so the hub reads installs back from the
    // same machine-level location the helper writes to.
    return eacp::Updater::protectedApplicationsRoot();
}

fs::path installedAppBundlePath(std::string_view bundleName)
{
    return installedApplicationsRoot() / std::string(bundleName);
}

fs::path installedDemoAppBundlePath()
{
    return installedAppBundlePath("Tamber Local Update Demo");
}

fs::path installedDemoAppExecutablePath()
{
    return installedDemoAppBundlePath() / "Tamber Local Update Demo.exe";
}

fs::path installedHubAppBundlePath()
{
    return installedAppBundlePath("AppHub");
}

fs::path installedHubAppExecutablePath()
{
    return installedHubAppBundlePath() / "AppHub.exe";
}

std::optional<fs::path> currentExecutablePath()
{
#if defined(_WIN32)
    auto buffer = std::array<wchar_t, MAX_PATH> {};
    auto size = GetModuleFileNameW(nullptr,
                                   buffer.data(),
                                   static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size())
        return std::nullopt;
    std::error_code ec;
    return fs::weakly_canonical(fs::path(buffer.data()), ec);
#else
    return std::nullopt;
#endif
}

bool createAppBundleZip(const fs::path& bundle, const fs::path& output)
{
    std::error_code ec;
    fs::create_directories(output.parent_path(), ec);
    fs::remove(output, ec);

    // bsdtar ships as tar.exe on Windows 10+; -a selects the zip format from the
    // output extension and --keepParent semantics come from archiving the bundle
    // directory by name relative to its parent.
    auto result = Processes::run("tar",
                                 {"-a",
                                  "-c",
                                  "-f",
                                  output.string(),
                                  "-C",
                                  bundle.parent_path().string(),
                                  bundle.filename().string()});
    return result.exited && result.exitCode == 0 && fs::exists(output, ec);
}

LaunchResult openAppBundle(std::string_view appPath)
{
#if defined(_WIN32)
    auto path = fs::path(std::string(appPath));
    auto instance = ShellExecuteW(nullptr,
                                  L"open",
                                  path.wstring().c_str(),
                                  nullptr,
                                  nullptr,
                                  SW_SHOWNORMAL);
    if (reinterpret_cast<std::intptr_t>(instance) > 32)
        return {.ok = true};
    return {.ok = false, .error = "ShellExecuteW failed"};
#else
    return {.ok = false, .error = "app launching is not implemented"};
#endif
}

LaunchResult openNewAppBundleInstance(std::string_view appPath)
{
    return openAppBundle(appPath);
}

PlatformResult directInstallAppBundle(const fs::path& root,
                                      const eacp::Updater::RemoteAppManifest& manifest,
                                      const fs::path& artifactPath)
{
    auto unpack = root / "remote-unpack";

    std::error_code ec;
    fs::remove_all(unpack, ec);
    fs::create_directories(unpack, ec);
    if (ec)
        return {.ok = false, .error = "failed to create unpack directory"};

    auto extracted = Processes::run(
        "tar",
        {"-x", "-f", artifactPath.string(), "-C", unpack.string()});
    if (!extracted.exited || extracted.exitCode != 0)
        return {.ok = false, .error = "failed to unpack artifact"};

    auto unpackedApp = unpack / manifest.bundleName;
    if (!fs::is_directory(unpackedApp, ec))
        return {.ok = false,
                .error = "artifact did not contain " + manifest.bundleName};

    auto installPath = installedAppBundlePath(manifest.bundleName);
    auto rollbackPath =
        installedApplicationsRoot() / (manifest.bundleName + ".rollback");

    fs::create_directories(installedApplicationsRoot(), ec);
    fs::remove_all(rollbackPath, ec);
    if (ec)
        return {.ok = false, .error = "failed to remove old rollback"};

    if (fs::exists(installPath, ec))
    {
        fs::rename(installPath, rollbackPath, ec);
        if (ec)
            return {.ok = false, .error = "failed to create rollback"};
    }

    fs::rename(unpackedApp, installPath, ec);
    if (ec)
    {
        auto copyEc = std::error_code();
        fs::copy(unpackedApp,
                 installPath,
                 fs::copy_options::recursive
                     | fs::copy_options::overwrite_existing,
                 copyEc);
        if (copyEc)
            return {.ok = false, .error = "failed to install app"};
    }

    return {.ok = true};
}

} // namespace AppHub
