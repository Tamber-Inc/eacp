#include <eacp/Updater/Updater.h>

#include <eacp/Core/Utils/SHA256.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
namespace Updater = eacp::Updater;

namespace
{
constexpr auto* platform = "dev";
constexpr auto* editorId = "tamber.editor";
constexpr auto* captureId = "tamber.capture";

struct CliOptions
{
    fs::path root = fs::temp_directory_path() / "eacp-apphub-demo";
    std::string command = "tui";
    std::string productId;
};

void writeFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string readFile(const fs::path& path)
{
    auto in = std::ifstream(path, std::ios::binary);
    if (!in)
        return {};

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

fs::path catalogPath(const fs::path& root)
{
    return root / "catalog.json";
}

fs::path stagingRoot(const fs::path& root)
{
    return root / "staging";
}

fs::path artifactPath(const fs::path& root, const std::string& productId)
{
    return stagingRoot(root) / (productId + ".artifact");
}

Updater::Product makeProduct(const std::string& id,
                             const std::string& name,
                             const std::string& version,
                             const fs::path& artifact)
{
    auto product = Updater::Product {.id = id,
                                     .name = name,
                                     .channel = "stable",
                                     .latestVersion = version};
    product.artifacts.add({.platform = platform,
                           .url = "file://" + artifact.string(),
                           .sha256 = eacp::Crypto::sha256File(artifact.string()),
                           .signature = "dev-signature-placeholder"});
    return product;
}

Updater::ProductCatalog writeDevCatalog(const fs::path& root, bool updateEditor)
{
    auto editorArtifact = artifactPath(root, editorId);
    auto captureArtifact = artifactPath(root, captureId);

    writeFile(editorArtifact,
              updateEditor ? "Example Editor payload v2"
                           : "Example Editor payload v1");
    writeFile(captureArtifact, "Example Capture payload v1");

    auto catalog = Updater::ProductCatalog {
        .catalogVersion = updateEditor ? 2 : 1,
        .signature = "dev-catalog-signature-placeholder"};

    catalog.products.add(makeProduct(editorId,
                                     "Example Editor",
                                     updateEditor ? "2.0.0" : "1.0.0",
                                     editorArtifact));
    catalog.products.add(
        makeProduct(captureId, "Example Capture", "1.0.0", captureArtifact));

    writeFile(catalogPath(root), Updater::catalogToJson(catalog));
    return catalog;
}

Updater::ProductCatalog loadOrCreateCatalog(const fs::path& root)
{
    auto raw = readFile(catalogPath(root));
    if (!raw.empty())
        return Updater::parseCatalogJson(raw);

    return writeDevCatalog(root, false);
}

Updater::MockPrivilegedHelper makeHelper(const fs::path& root)
{
    return Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = stagingRoot(root).string()});
}

std::optional<CliOptions> parseArgs(int argc, char* argv[])
{
    auto options = CliOptions();

    for (auto i = 1; i < argc; ++i)
    {
        auto arg = std::string(argv[i]);
        if (arg == "--root")
        {
            if (i + 1 >= argc)
                return std::nullopt;
            options.root = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            options.command = "help";
        }
        else if (options.command == "tui")
        {
            options.command = arg;
        }
        else if (options.productId.empty())
        {
            options.productId = arg;
        }
        else
        {
            return std::nullopt;
        }
    }

    return options;
}

void printUsage()
{
    std::cout
        << "AppHub mock updater CLI\n\n"
        << "Usage:\n"
        << "  AppHub [--root <path>] tui\n"
        << "  AppHub [--root <path>] demo\n"
        << "  AppHub [--root <path>] reset\n"
        << "  AppHub [--root <path>] list\n"
        << "  AppHub [--root <path>] status\n"
        << "  AppHub [--root <path>] install <product-id>\n"
        << "  AppHub [--root <path>] update\n"
        << "  AppHub [--root <path>] remove <product-id>\n\n"
        << "Products:\n"
        << "  " << editorId << "\n"
        << "  " << captureId << "\n";
}

void printStatus(const fs::path& root,
                 const Updater::ProductCatalog& catalog,
                 const eacp::Vector<Updater::ProductReceipt>& receipts)
{
    std::cout << "Root: " << root << "\n";
    std::cout << "Catalog version: " << catalog.catalogVersion << "\n";
    std::cout << "Products:\n";

    for (const auto& product: catalog.products)
    {
        auto* receipt = Updater::findReceipt(receipts, product.id);
        auto installed = receipt != nullptr;
        auto updateAvailable =
            installed && Updater::isNewerVersion(product.latestVersion,
                                                 receipt->version);

        std::cout << "  " << product.id << " | " << product.name
                  << " | latest " << product.latestVersion << " | ";

        if (!installed)
        {
            std::cout << "not installed\n";
        }
        else
        {
            std::cout << "installed " << receipt->version;
            if (updateAvailable)
                std::cout << " | update available";
            std::cout << "\n";
        }
    }
}

void printReceipts(const eacp::Vector<Updater::ProductReceipt>& receipts)
{
    std::cout << "Receipts:\n";
    if (receipts.empty())
    {
        std::cout << "  none\n";
        return;
    }

    for (const auto& receipt: receipts)
    {
        std::cout << "  " << receipt.productId << " " << receipt.version
                  << " -> " << receipt.installPath << "\n";
    }
}

int printResult(const std::string& label, const Updater::InstallResult& result)
{
    std::cout << label << ": " << (result.ok ? "ok" : result.error) << "\n";
    return result.ok ? 0 : 1;
}

int resetRoot(const fs::path& root)
{
    std::error_code ec;
    fs::remove_all(root, ec);
    if (ec)
    {
        std::cout << "Reset failed: " << ec.message() << "\n";
        return 1;
    }

    writeDevCatalog(root, false);
    auto helper = makeHelper(root);
    if (!helper.isInstalled())
    {
        std::cout << "Reset failed: helper root could not be created\n";
        return 1;
    }

    std::cout << "Reset mock AppHub root: " << root << "\n";
    return 0;
}

int installProduct(const fs::path& root, const std::string& productId)
{
    auto catalog = loadOrCreateCatalog(root);
    auto helper = makeHelper(root);
    auto plan = Updater::planInstall(catalog,
                                     helper.receipts(),
                                     productId,
                                     platform,
                                     artifactPath(root, productId).string());

    if (plan.operations.empty())
    {
        std::cout << "No install plan for product: " << productId << "\n";
        return 1;
    }

    return printResult("Install " + productId, helper.submit(plan));
}

int updateAll(const fs::path& root)
{
    auto catalog = writeDevCatalog(root, true);
    auto helper = makeHelper(root);
    auto plan =
        Updater::planUpdateAll(catalog, helper.receipts(), platform, stagingRoot(root).string());

    if (plan.operations.empty())
    {
        std::cout << "Update all: no updates available\n";
        return 0;
    }

    return printResult("Update all", helper.submit(plan));
}

int removeProduct(const fs::path& root, const std::string& productId)
{
    auto helper = makeHelper(root);
    return printResult("Remove " + productId,
                       helper.submit(Updater::planRemove(productId)));
}

int runDemo(const fs::path& root)
{
    auto status = resetRoot(root);
    if (status != 0)
        return status;

    status = installProduct(root, editorId);
    if (status != 0)
        return status;

    status = installProduct(root, captureId);
    if (status != 0)
        return status;

    status = updateAll(root);
    if (status != 0)
        return status;

    status = removeProduct(root, captureId);
    if (status != 0)
        return status;

    auto helper = makeHelper(root);
    printReceipts(helper.receipts());
    std::cout << "Demo complete. All writes were constrained to the mock helper root.\n";
    return 0;
}

int showList(const fs::path& root)
{
    auto catalog = loadOrCreateCatalog(root);
    auto helper = makeHelper(root);
    printStatus(root, catalog, helper.receipts());
    return 0;
}

int showReceipts(const fs::path& root)
{
    auto helper = makeHelper(root);
    std::cout << "Root: " << root << "\n";
    printReceipts(helper.receipts());
    return 0;
}

int runTui(const fs::path& root)
{
    loadOrCreateCatalog(root);

    while (true)
    {
        std::cout << "\nAppHub mock updater\n"
                  << "Root: " << root << "\n"
                  << "1. List products\n"
                  << "2. Install Example Editor\n"
                  << "3. Install Example Capture\n"
                  << "4. Update all\n"
                  << "5. Remove Example Editor\n"
                  << "6. Remove Example Capture\n"
                  << "7. Show receipts\n"
                  << "8. Reset\n"
                  << "9. Quit\n"
                  << "> ";

        auto choice = std::string();
        if (!std::getline(std::cin, choice))
            return 0;

        if (choice == "1")
            showList(root);
        else if (choice == "2")
            installProduct(root, editorId);
        else if (choice == "3")
            installProduct(root, captureId);
        else if (choice == "4")
            updateAll(root);
        else if (choice == "5")
            removeProduct(root, editorId);
        else if (choice == "6")
            removeProduct(root, captureId);
        else if (choice == "7")
            showReceipts(root);
        else if (choice == "8")
            resetRoot(root);
        else if (choice == "9" || choice == "q" || choice == "quit")
            return 0;
        else
            std::cout << "Unknown choice\n";
    }
}
} // namespace

int main(int argc, char* argv[])
{
    auto parsed = parseArgs(argc, argv);
    if (!parsed)
    {
        printUsage();
        return 2;
    }

    const auto& options = *parsed;
    const auto& command = options.command;

    if (command == "help")
    {
        printUsage();
        return 0;
    }
    if (command == "tui")
        return runTui(options.root);
    if (command == "demo")
        return runDemo(options.root);
    if (command == "reset")
        return resetRoot(options.root);
    if (command == "list")
        return showList(options.root);
    if (command == "status")
        return showReceipts(options.root);
    if (command == "install")
    {
        if (options.productId.empty())
        {
            std::cout << "install requires a product id\n";
            return 2;
        }
        return installProduct(options.root, options.productId);
    }
    if (command == "update")
        return updateAll(options.root);
    if (command == "remove")
    {
        if (options.productId.empty())
        {
            std::cout << "remove requires a product id\n";
            return 2;
        }
        return removeProduct(options.root, options.productId);
    }

    printUsage();
    return 2;
}
