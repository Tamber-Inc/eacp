#include "AppHubApi.h"

#include <NanoTest/NanoTest.h>

#include <cstdlib>
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

void writeCatalogAt(const fs::path& path, const Updater::ProductCatalog& catalog)
{
    writeFile(path, Updater::catalogToJson(catalog));
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

void writeReceiptFor(const fs::path& root,
                     const std::string& productId,
                     const std::string& name,
                     const std::string& version,
                     const std::string& installPath,
                     const std::string& artifactSha256)
{
    auto receipt = Updater::ProductReceipt();
    receipt.productId = productId;
    receipt.name = name;
    receipt.version = version;
    receipt.installPath = installPath;
    receipt.channel = "stable";
    receipt.artifactSha256 = artifactSha256;
    receipt.installedAt = "2026-06-29T00:00:00Z";

    auto options = Updater::MockHelperOptions();
    options.root = root.string();
    options.stagingRoot = (root / "staging").string();
    auto helper = Updater::MockPrivilegedHelper(options);
    writeFile(fs::path(helper.receiptsRoot()) / (productId + ".json"),
              Updater::receiptToJson(receipt));
}

class ScopedEnvironmentVariable
{
public:
    ScopedEnvironmentVariable(const std::string& name, const std::string& value)
        : name_(name)
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value.c_str());
#else
        ::setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ~ScopedEnvironmentVariable()
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), "");
#else
        ::unsetenv(name_.c_str());
#endif
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

private:
    std::string name_;
};
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

auto tLoadsConfiguredDevCatalog =
    test("AppHub/loadsConfiguredDevCatalog") = []
{
    auto root = testRoot("configured-dev-catalog");
    auto catalogPath = root / "generated" / "apphub-catalog.json";

    auto product = Updater::Product();
    product.id = "com.eacp.webviewtodo";
    product.name = "WebView Todo";
    product.kind = Updater::PackageKind::App;
    product.bundleName = "WebView Todo.app";
    product.channel = "stable";
    product.latestVersion = "1.0.0";

    auto artifact = Updater::ProductArtifact();
    artifact.platform = Updater::Platform::MacOS;
    artifact.architecture = Updater::Architecture::Universal;
    artifact.url = "file:///tmp/webviewtodo.zip";
    artifact.sha256 = "test";
    product.artifacts.add(artifact);

    auto catalog = Updater::ProductCatalog();
    catalog.catalogVersion = 42;
    catalog.signature = "test";
    catalog.products.add(product);
    writeCatalogAt(catalogPath, catalog);

    auto catalogOverride = ScopedEnvironmentVariable(
        "EACP_APPHUB_DEV_CATALOG_PATH", catalogPath.string());
    auto api = Api::AppHubApi(root);
    auto state = api.getHubState();

    check(state.catalogVersion == 42);
    check(state.products.size() == 1);
    if (state.products.size() == 1)
    {
        check(state.products[0].id == "com.eacp.webviewtodo");
        check(state.products[0].name == "WebView Todo");
    }
};

auto tUpdateProductUpdatesOnlyRequestedInstalledProduct =
    test("AppHub/updateProductUpdatesRequestedInstalledProduct") = []
{
    auto root = testRoot("targeted-update-product");
    auto artifact = root / "artifacts" / "shared.clap.artifact";
    writeFile(artifact, "clap-v2");

    auto artifactHash = eacp::Crypto::sha256File(artifact.string());

    auto product = Updater::Product();
    product.id = "shared.clap";
    product.name = "CLAP Model";
    product.kind = Updater::PackageKind::Model;
    product.channel = "stable";
    product.latestVersion = "2.0.0";

    auto productArtifact = Updater::ProductArtifact();
    productArtifact.platform = Updater::Platform::Any;
    productArtifact.architecture = Updater::Architecture::Any;
    productArtifact.url = "file://" + artifact.string();
    productArtifact.sha256 = artifactHash;
    product.artifacts.add(productArtifact);

    auto catalog = Updater::ProductCatalog();
    catalog.catalogVersion = 2;
    catalog.signature = "test";
    catalog.products.add(product);
    writeCatalogAt(root / "catalog.json", catalog);

    writeReceiptFor(root,
                    "shared.clap",
                    "CLAP Model",
                    "1.0.0",
                    (root / "installed" / "shared.clap").string(),
                    "old-hash");

    auto api = Api::AppHubApi(root);
    auto result = api.updateProduct({.productId = "shared.clap"});
    auto helper = Updater::MockPrivilegedHelper(
        Updater::MockHelperOptions {.root = root.string(),
                                    .stagingRoot = (root / "staging").string()});
    auto receipts = helper.receipts();
    auto* receipt = Updater::findReceipt(receipts, "shared.clap");

    check(result.ok);
    check(receipt != nullptr);
    if (receipt != nullptr)
    {
        check(receipt->version == "2.0.0");
        check(receipt->artifactSha256 == artifactHash);
    }
};
