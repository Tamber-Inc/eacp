#include <eacp/Updater/Updater.h>

#include <eacp/Core/Utils/SHA256.h>

#include <Miro/Miro.h>
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
fs::path testRoot(const std::string& name)
{
    auto root = fs::temp_directory_path() / ("eacp-updater-tests-" + name);
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

Updater::ProductCatalog makeCatalog(const std::string& editorHash,
                                    const std::string& captureHash = {})
{
    auto catalog = Updater::ProductCatalog {.catalogVersion = 1,
                                            .signature = "dev-signature"};

    auto editor = Updater::Product {.id = "tamber.editor",
                                    .name = "Example Editor",
                                    .channel = "stable",
                                    .latestVersion = "1.0.0"};
    editor.artifacts.add({.platform = "test-platform",
                          .url = "file://editor",
                          .sha256 = editorHash,
                          .signature = "dev-artifact"});
    catalog.products.add(editor);

    if (!captureHash.empty())
    {
        auto capture = Updater::Product {.id = "tamber.capture",
                                         .name = "Example Capture",
                                         .channel = "stable",
                                         .latestVersion = "1.0.0"};
        capture.artifacts.add({.platform = "test-platform",
                               .url = "file://capture",
                               .sha256 = captureHash,
                               .signature = "dev-artifact"});
        catalog.products.add(capture);
    }

    return catalog;
}
} // namespace

auto tCatalogUsesMiroJson = test("Updater/catalogRoundTripsThroughMiroJson") = []
{
    auto catalog = makeCatalog("hash");
    auto json = Updater::catalogToJson(catalog);

    check(json.find("tamber.editor") != std::string::npos);

    auto parsed = Updater::parseCatalogJson(json);
    check(parsed.catalogVersion == 1);
    check(parsed.products.size() == 1);
    check(parsed.products[0].id == "tamber.editor");
    check(parsed.products[0].artifacts[0].sha256 == "hash");
};

auto tInstallPlanUsesEnumClassWithMiroJson =
    test("Updater/installPlanRoundTripsEnumClassThroughMiroJson") = []
{
    auto plan = Updater::InstallPlan();
    plan.operations.add({.action = Updater::PlanAction::Update,
                         .productId = "tamber.editor",
                         .version = "2.0.0"});

    auto json = Updater::installPlanToJson(plan);
    check(json.find("\"Update\"") != std::string::npos);

    auto parsed = Updater::parseInstallPlanJson(json);
    check(parsed.operations.size() == 1);
    check(parsed.operations[0].action == Updater::PlanAction::Update);
};

auto tProductIdValidation = test("Updater/productIdValidation") = []
{
    check(Updater::isValidProductId("tamber.editor"));
    check(Updater::isValidProductId("com.example-App_1"));
    check(!Updater::isValidProductId(""));
    check(!Updater::isValidProductId("."));
    check(!Updater::isValidProductId(".."));
    check(!Updater::isValidProductId("../evil"));
    check(!Updater::isValidProductId("evil/path"));
    check(!Updater::isValidProductId("evil path"));
};

auto tPlanRemoveRejectsInvalidProductId =
    test("Updater/planRemoveRejectsInvalidProductId") = []
{
    check(Updater::planRemove("../evil").operations.empty());
    check(Updater::planRemove("").operations.empty());
};

auto tMockHelperInstallsAndWritesReceipt =
    test("Updater/mockHelperInstallsAndWritesReceipt") = []
{
    auto root = testRoot("install");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v1");

    auto hash = eacp::Crypto::sha256File(artifact.string());
    auto catalog = makeCatalog(hash);
    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});

    auto plan = Updater::planInstall(
        catalog, {}, "tamber.editor", "test-platform", artifact.string());

    auto result = helper.submit(plan);
    check(result.ok);

    auto receipts = helper.receipts();
    check(receipts.size() == 1);
    check(receipts[0].productId == "tamber.editor");
    check(receipts[0].version == "1.0.0");
    check(fs::exists(root / "Applications" / "tamber.editor" / "artifact.bin"));
};

auto tMockHelperRejectsForgedUnsafeProductId =
    test("Updater/mockHelperRejectsForgedUnsafeProductId") = []
{
    auto root = testRoot("unsafe-product-id");
    auto staging = root / "staging";
    auto artifact = staging / "evil.artifact";
    writeFile(artifact, "evil");

    auto plan = Updater::InstallPlan();
    plan.operations.add({.action = Updater::PlanAction::Install,
                         .productId = "../evil",
                         .name = "Evil",
                         .channel = "stable",
                         .version = "1.0.0",
                         .artifactPath = artifact.string(),
                         .artifactSha256 =
                             eacp::Crypto::sha256File(artifact.string())});

    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});
    auto result = helper.submit(plan);

    check(!result.ok);
    check(result.error.find("product id") != std::string::npos);
    check(helper.receipts().empty());
    check(!fs::exists(root / "Applications" / "evil"));
};

auto tMockHelperRejectsDuplicateOperationsBeforeMutation =
    test("Updater/mockHelperRejectsDuplicateOperationsBeforeMutation") = []
{
    auto root = testRoot("duplicate-op");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v1");

    auto hash = eacp::Crypto::sha256File(artifact.string());
    auto plan = Updater::InstallPlan();
    plan.operations.add({.action = Updater::PlanAction::Install,
                         .productId = "tamber.editor",
                         .name = "Example Editor",
                         .channel = "stable",
                         .version = "1.0.0",
                         .artifactPath = artifact.string(),
                         .artifactSha256 = hash});
    plan.operations.add({.action = Updater::PlanAction::Remove,
                         .productId = "tamber.editor"});

    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});
    auto result = helper.submit(plan);

    check(!result.ok);
    check(result.error.find("duplicate") != std::string::npos);
    check(helper.receipts().empty());
    check(!fs::exists(root / "Applications" / "tamber.editor"));
};

auto tMockHelperRejectsHashMismatch =
    test("Updater/mockHelperRejectsHashMismatch") = []
{
    auto root = testRoot("hash-mismatch");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v1");

    auto catalog = makeCatalog("not-the-real-hash");
    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});

    auto plan = Updater::planInstall(
        catalog, {}, "tamber.editor", "test-platform", artifact.string());

    auto result = helper.submit(plan);
    check(!result.ok);
    check(result.error.find("hash") != std::string::npos);
};

auto tMockHelperValidatesWholePlanBeforeMutation =
    test("Updater/mockHelperValidatesWholePlanBeforeMutation") = []
{
    auto root = testRoot("validate-before-mutate");
    auto staging = root / "staging";
    auto editorArtifact = staging / "tamber.editor.artifact";
    auto captureArtifact = staging / "tamber.capture.artifact";
    writeFile(editorArtifact, "editor-v1");
    writeFile(captureArtifact, "capture-v1");

    auto plan = Updater::InstallPlan();
    plan.operations.add({.action = Updater::PlanAction::Install,
                         .productId = "tamber.editor",
                         .name = "Example Editor",
                         .channel = "stable",
                         .version = "1.0.0",
                         .artifactPath = editorArtifact.string(),
                         .artifactSha256 =
                             eacp::Crypto::sha256File(editorArtifact.string())});
    plan.operations.add({.action = Updater::PlanAction::Install,
                         .productId = "tamber.capture",
                         .name = "Example Capture",
                         .channel = "stable",
                         .version = "1.0.0",
                         .artifactPath = captureArtifact.string(),
                         .artifactSha256 = "bad-hash"});

    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});
    auto result = helper.submit(plan);

    check(!result.ok);
    check(result.error.find("hash") != std::string::npos);
    check(helper.receipts().empty());
    check(!fs::exists(root / "Applications" / "tamber.editor"));
    check(!fs::exists(root / "Applications" / "tamber.capture"));
};

auto tMockHelperRejectsStagingEscape =
    test("Updater/mockHelperRejectsArtifactOutsideStaging") = []
{
    auto root = testRoot("staging-escape");
    auto staging = root / "staging";
    auto outside = root / "outside.artifact";
    writeFile(outside, "editor-v1");

    auto hash = eacp::Crypto::sha256File(outside.string());
    auto catalog = makeCatalog(hash);
    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});

    auto plan = Updater::planInstall(
        catalog, {}, "tamber.editor", "test-platform", outside.string());

    auto result = helper.submit(plan);
    check(!result.ok);
    check(result.error.find("staging") != std::string::npos);
};

auto tMockHelperRejectsDowngrade = test("Updater/mockHelperRejectsDowngrade") = []
{
    auto root = testRoot("downgrade");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v2");

    auto hash = eacp::Crypto::sha256File(artifact.string());
    auto catalog = makeCatalog(hash);
    catalog.products[0].latestVersion = "2.0.0";

    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});

    auto installPlan = Updater::planInstall(
        catalog, {}, "tamber.editor", "test-platform", artifact.string());
    check(helper.submit(installPlan).ok);

    writeFile(artifact, "editor-v1");
    hash = eacp::Crypto::sha256File(artifact.string());
    catalog.products[0].latestVersion = "1.0.0";
    catalog.products[0].artifacts[0].sha256 = hash;

    auto downgradePlan = Updater::planInstall(catalog,
                                             helper.receipts(),
                                             "tamber.editor",
                                             "test-platform",
                                             artifact.string());
    auto result = helper.submit(downgradePlan);

    check(!result.ok);
    check(result.error.find("downgrade") != std::string::npos);
};

auto tPlanUpdateAllFindsNewerInstalledProduct =
    test("Updater/planUpdateAllFindsNewerInstalledProduct") = []
{
    auto root = testRoot("update-plan");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v2");

    auto hash = eacp::Crypto::sha256File(artifact.string());
    auto catalog = makeCatalog(hash);
    catalog.products[0].latestVersion = "2.0.0";

    auto receipts = eacp::Vector<Updater::ProductReceipt>();
    receipts.add({.productId = "tamber.editor", .version = "1.0.0"});

    auto plan = Updater::planUpdateAll(
        catalog, receipts, "test-platform", staging.string());

    check(plan.operations.size() == 1);
    check(plan.operations[0].action == Updater::PlanAction::Update);
    check(plan.operations[0].artifactPath == artifact.string());
};

auto tMockHelperRemovesReceiptAndInstall =
    test("Updater/mockHelperRemovesReceiptAndInstall") = []
{
    auto root = testRoot("remove");
    auto staging = root / "staging";
    auto artifact = staging / "tamber.editor.artifact";
    writeFile(artifact, "editor-v1");

    auto hash = eacp::Crypto::sha256File(artifact.string());
    auto catalog = makeCatalog(hash);
    auto helper = Updater::MockPrivilegedHelper(
        {.root = root.string(), .stagingRoot = staging.string()});

    auto installPlan = Updater::planInstall(
        catalog, {}, "tamber.editor", "test-platform", artifact.string());
    check(helper.submit(installPlan).ok);

    auto removePlan = Updater::planRemove("tamber.editor");
    check(helper.submit(removePlan).ok);
    check(helper.receipts().empty());
    check(!fs::exists(root / "Applications" / "tamber.editor"));
};
