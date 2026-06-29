#include "AppHubApi.h"

#include <NanoTest/NanoTest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using namespace nano;
namespace fs = std::filesystem;
namespace Updater = eacp::Updater;

namespace
{
struct LaunchProbe
{
    bool shouldLaunch = false;
    std::string launchedPath;
};

LaunchProbe& launchProbe()
{
    static auto probe = LaunchProbe();
    return probe;
}

fs::path testRoot(const std::string& name)
{
    auto root = fs::temp_directory_path() / ("eacp-apphub-tests-" + name);
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    return root;
}

void writeFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    out << text;
}

Updater::Product makeAppProduct()
{
    auto product = Updater::Product();
    product.id = "com.eacp.maze";
    product.name = "Maze";
    product.kind = Updater::PackageKind::App;
    product.bundleName = "Maze.app";
    product.channel = "stable";
    product.latestVersion = "1.0.0";
    return product;
}

void writeCatalog(const fs::path& root)
{
    auto catalog = Updater::ProductCatalog();
    catalog.catalogVersion = 1;
    catalog.signature = "test";
    catalog.products.add(makeAppProduct());
    writeFile(root / "catalog.json", Updater::catalogToJson(catalog));
}

void writeReceipt(const fs::path& root, const std::string& installPath)
{
    auto receipt = Updater::ProductReceipt();
    receipt.productId = "com.eacp.maze";
    receipt.name = "Maze";
    receipt.version = "1.0.0";
    receipt.installPath = installPath;
    receipt.channel = "stable";
    receipt.artifactSha256 = "test";
    receipt.installedAt = "2026-06-29T00:00:00Z";

    auto options = Updater::MockHelperOptions();
    options.root = root.string();
    options.stagingRoot = (root / "staging").string();
    auto helper = Updater::MockPrivilegedHelper(options);
    writeFile(fs::path(helper.receiptsRoot()) / "com.eacp.maze.json",
              Updater::receiptToJson(receipt));
}
} // namespace

namespace AppHub
{
fs::path defaultStateRoot()
{
    return testRoot("default-state");
}

Updater::Target currentTarget()
{
    auto target = Updater::Target();
    target.platform = Updater::Platform::MacOS;
    target.architecture = Updater::Architecture::Universal;
    return target;
}

fs::path installedApplicationsRoot()
{
    return fs::temp_directory_path() / "eacp-apphub-tests-applications";
}

fs::path installedAppBundlePath(std::string_view bundleName)
{
    return installedApplicationsRoot() / std::string(bundleName);
}

fs::path installedDemoAppBundlePath()
{
    return installedAppBundlePath("Demo.app");
}

fs::path installedDemoAppExecutablePath()
{
    return installedDemoAppBundlePath() / "Demo";
}

fs::path installedHubAppBundlePath()
{
    return installedAppBundlePath("AppHub.app");
}

fs::path installedHubAppExecutablePath()
{
    return installedHubAppBundlePath() / "AppHub";
}

std::optional<fs::path> currentExecutablePath()
{
    return std::nullopt;
}

bool createAppBundleZip(const fs::path&, const fs::path&)
{
    return false;
}

LaunchResult openAppBundle(std::string_view appPath)
{
    launchProbe().launchedPath = std::string(appPath);
    if (launchProbe().shouldLaunch)
        return {.ok = true};
    return {.ok = false, .error = "test launch failed"};
}

LaunchResult openNewAppBundleInstance(std::string_view appPath)
{
    return openAppBundle(appPath);
}

PlatformResult directInstallAppBundle(const fs::path&,
                                      const Updater::RemoteAppManifest&,
                                      const fs::path&)
{
    return {.ok = false, .error = "not used"};
}

PrivilegedHelperInstallResult installPrivilegedHelper()
{
    return {.ok = false, .error = "not used"};
}

Updater::InstallResult installAppBundleWithPrivilegedHelper(
    const Updater::PrivilegedAppBundleInstallRequest&)
{
    return {.ok = false, .error = "not used"};
}
} // namespace AppHub

auto tOpenProductDoesNotMarkRunningWhenLaunchFails =
    test("AppHub/openProductDoesNotMarkRunningWhenLaunchFails") = []
{
    auto root = testRoot("launch-fails");
    auto appPath = (root / "Installed" / "Maze.app").string();
    writeCatalog(root);
    writeReceipt(root, appPath);
    launchProbe() = {.shouldLaunch = false};

    auto api = Api::AppHubApi(root);
    auto result = api.openProduct({.productId = "com.eacp.maze"});

    check(!result.ok);
    check(result.message == "test launch failed");
    check(launchProbe().launchedPath == appPath);
    check(!fs::exists(root / "running" / "com.eacp.maze.running"));
};

auto tOpenProductMarksRunningOnlyAfterLaunchSucceeds =
    test("AppHub/openProductMarksRunningOnlyAfterLaunchSucceeds") = []
{
    auto root = testRoot("launch-succeeds");
    auto appPath = (root / "Installed" / "Maze.app").string();
    writeCatalog(root);
    writeReceipt(root, appPath);
    launchProbe() = {.shouldLaunch = true};

    auto api = Api::AppHubApi(root);
    auto result = api.openProduct({.productId = "com.eacp.maze"});

    check(result.ok);
    check(launchProbe().launchedPath == appPath);
    check(fs::exists(root / "running" / "com.eacp.maze.running"));
};

auto tOpenProductFallsBackToCatalogBundleWhenReceiptIsMissing =
    test("AppHub/openProductFallsBackToCatalogBundleWhenReceiptIsMissing") = []
{
    auto root = testRoot("launch-catalog-fallback");
    writeCatalog(root);
    launchProbe() = {.shouldLaunch = true};

    auto api = Api::AppHubApi(root);
    auto result = api.openProduct({.productId = "com.eacp.maze"});

    check(result.ok);
    check(launchProbe().launchedPath
          == AppHub::installedAppBundlePath("Maze.app").string());
    check(fs::exists(root / "running" / "com.eacp.maze.running"));
};
