#pragma once

#include <eacp/Core/Utils/Containers.h>

#include <Miro/Miro.h>

#include <string>

namespace eacp::Updater
{

struct ProductId
{
    std::string value;

    MIRO_REFLECT(value)
};

struct ProductArtifact
{
    std::string platform;
    std::string url;
    std::string sha256;
    std::string signature;

    MIRO_REFLECT(platform, url, sha256, signature)
};

struct Product
{
    std::string id;
    std::string name;
    std::string channel = "stable";
    std::string latestVersion;
    Vector<std::string> dependencies;
    Vector<ProductArtifact> artifacts;

    MIRO_REFLECT(id, name, channel, latestVersion, dependencies, artifacts)
};

struct ProductCatalog
{
    int catalogVersion = 0;
    Vector<Product> products;
    std::string signature;

    MIRO_REFLECT(catalogVersion, products, signature)
};

struct ProductReceipt
{
    std::string productId;
    std::string name;
    std::string version;
    std::string installPath;
    std::string channel;
    std::string artifactSha256;
    std::string installedAt;

    MIRO_REFLECT(productId,
                 name,
                 version,
                 installPath,
                 channel,
                 artifactSha256,
                 installedAt)
};

enum class PlanAction
{
    Install,
    Update,
    Remove
};

struct PlanOperation
{
    PlanAction action = PlanAction::Install;
    std::string productId;
    std::string name;
    std::string channel;
    std::string version;
    std::string artifactPath;
    std::string artifactSha256;

    MIRO_REFLECT(action,
                 productId,
                 name,
                 channel,
                 version,
                 artifactPath,
                 artifactSha256)
};

struct InstallPlan
{
    Vector<PlanOperation> operations;

    MIRO_REFLECT(operations)
};

struct InstallResult
{
    bool ok = false;
    std::string error;

    MIRO_REFLECT(ok, error)
};

struct MockHelperOptions
{
    std::string root;
    std::string stagingRoot;
    bool allowDowngrade = false;
};

ProductCatalog parseCatalogJson(const std::string& json);
std::string catalogToJson(const ProductCatalog& catalog);
ProductReceipt parseReceiptJson(const std::string& json);
std::string receiptToJson(const ProductReceipt& receipt);
InstallPlan parseInstallPlanJson(const std::string& json);
std::string installPlanToJson(const InstallPlan& plan);

const Product* findProduct(const ProductCatalog& catalog,
                           const std::string& productId);
const ProductReceipt* findReceipt(const Vector<ProductReceipt>& receipts,
                                  const std::string& productId);

int compareVersions(const std::string& lhs, const std::string& rhs);
bool isNewerVersion(const std::string& candidate, const std::string& current);
bool isValidProductId(const std::string& productId);

ProductArtifact artifactForPlatform(const Product& product,
                                    const std::string& platform);
InstallPlan planInstall(const ProductCatalog& catalog,
                        const Vector<ProductReceipt>& receipts,
                        const std::string& productId,
                        const std::string& platform,
                        const std::string& artifactPath);
InstallPlan planUpdateAll(const ProductCatalog& catalog,
                          const Vector<ProductReceipt>& receipts,
                          const std::string& platform,
                          const std::string& artifactDirectory);
InstallPlan planRemove(const std::string& productId);

bool pathIsUnder(const std::string& path, const std::string& root);

class MockPrivilegedHelper
{
public:
    explicit MockPrivilegedHelper(MockHelperOptions options);

    bool isInstalled() const;
    Vector<ProductReceipt> receipts() const;
    InstallResult submit(const InstallPlan& plan);

    std::string applicationsRoot() const;
    std::string receiptsRoot() const;

private:
    MockHelperOptions options;
};

} // namespace eacp::Updater
