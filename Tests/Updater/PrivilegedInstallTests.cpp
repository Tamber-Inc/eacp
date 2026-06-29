// End-to-end coverage for the privileged install path that writes app bundles
// into the machine-level protected root. The logic in installAppBundleArtifact
// is the same code the Windows privileged helper executes; pointing the install
// root at a temp directory (EACP_APPHUB_INSTALL_ROOT) lets CI exercise the real
// validate -> extract -> atomic-swap -> rollback behaviour without elevation.
//
// Gated to Windows: the macOS branch additionally requires a codesigned bundle
// and team identifier, which a unit test cannot synthesise.

#if defined(_WIN32)

#include <eacp/Updater/Updater.h>

#include <eacp/Core/Process/Process.h>
#include <eacp/Core/Utils/SHA256.h>

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
fs::path testRoot(const std::string& name)
{
    auto root = fs::temp_directory_path() / ("eacp-privileged-install-" + name);
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    return root;
}

void writeFile(const fs::path& path, const std::string& text)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    out << text;
}

void setInstallRoot(const fs::path& root)
{
    _putenv_s("EACP_APPHUB_INSTALL_ROOT", root.string().c_str());
}

// Builds a zip artifact that contains a bundle directory at top level, the same
// shape the hub stages for an install request.
fs::path makeBundleArtifact(const fs::path& workspace,
                            const std::string& bundleName,
                            const std::string& executableName,
                            const std::string& payload)
{
    auto bundle = workspace / "build" / bundleName;
    writeFile(bundle / executableName, payload);

    auto artifact = workspace / "staging" / "bundle.app.zip";
    std::error_code ec;
    fs::create_directories(artifact.parent_path(), ec);
    fs::remove(artifact, ec);

    auto result = eacp::Processes::run("tar",
                                       {"-a",
                                        "-c",
                                        "-f",
                                        artifact.string(),
                                        "-C",
                                        bundle.parent_path().string(),
                                        bundleName});
    check(result.exited && result.exitCode == 0);
    return artifact;
}

Updater::PrivilegedAppBundleInstallRequest makeRequest(const std::string& bundleName,
                                                       const std::string& version,
                                                       const fs::path& artifact)
{
    auto request = Updater::PrivilegedAppBundleInstallRequest();
    request.productId = "com.eacp.maze";
    request.name = "Maze";
    request.version = version;
    request.bundleName = bundleName;
    request.artifactPath = artifact.string();
    request.artifactSha256 = eacp::Crypto::sha256File(artifact.string());
    return request;
}
} // namespace

auto tPrivilegedInstallWritesBundleToProtectedRoot =
    test("Updater/privilegedInstallWritesBundleToProtectedRoot") = []
{
    auto root = testRoot("happy");
    auto installRoot = root / "Program Files" / "Tamber" / "Apps";
    setInstallRoot(installRoot);

    auto artifact = makeBundleArtifact(root, "Maze.app", "Maze.exe", "maze-v1");
    auto result =
        Updater::installAppBundleArtifact(makeRequest("Maze.app", "1.0.0", artifact));

    check(result.ok);
    check(fs::exists(installRoot / "Maze.app" / "Maze.exe"));

    auto in = std::ifstream(installRoot / "Maze.app" / "Maze.exe", std::ios::binary);
    auto content = std::string(std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>());
    check(content == "maze-v1");
};

auto tPrivilegedInstallReplacesAndKeepsRollback =
    test("Updater/privilegedInstallReplacesAndKeepsRollback") = []
{
    auto root = testRoot("rollback");
    auto installRoot = root / "Apps";
    setInstallRoot(installRoot);

    auto first = makeBundleArtifact(root, "Maze.app", "Maze.exe", "maze-v1");
    check(Updater::installAppBundleArtifact(makeRequest("Maze.app", "1.0.0", first)).ok);

    auto second =
        makeBundleArtifact(root / "v2", "Maze.app", "Maze.exe", "maze-v2");
    check(
        Updater::installAppBundleArtifact(makeRequest("Maze.app", "2.0.0", second))
            .ok);

    auto in = std::ifstream(installRoot / "Maze.app" / "Maze.exe", std::ios::binary);
    auto content = std::string(std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>());
    check(content == "maze-v2");
    check(fs::exists(installRoot / "Maze.app.rollback"));
};

auto tPrivilegedInstallRejectsHashMismatch =
    test("Updater/privilegedInstallRejectsHashMismatch") = []
{
    auto root = testRoot("hash");
    setInstallRoot(root / "Apps");

    auto artifact = makeBundleArtifact(root, "Maze.app", "Maze.exe", "maze-v1");
    auto request = makeRequest("Maze.app", "1.0.0", artifact);
    request.artifactSha256 = "0000000000000000000000000000000000000000000000000000000000000000";

    auto result = Updater::installAppBundleArtifact(request);
    check(!result.ok);
    check(result.error.find("hash") != std::string::npos);
    check(!fs::exists((root / "Apps") / "Maze.app"));
};

auto tPrivilegedInstallRejectsInvalidBundleName =
    test("Updater/privilegedInstallRejectsInvalidBundleName") = []
{
    auto root = testRoot("badname");
    setInstallRoot(root / "Apps");

    auto artifact = makeBundleArtifact(root, "Maze.app", "Maze.exe", "maze-v1");
    auto request = makeRequest("../escape", "1.0.0", artifact);

    auto result = Updater::installAppBundleArtifact(request);
    check(!result.ok);
    check(!fs::exists((root / "Apps") / "Maze.app"));
};

auto tProtectedRootHonoursOverride =
    test("Updater/protectedApplicationsRootHonoursOverride") = []
{
    auto root = testRoot("override");
    setInstallRoot(root);
    check(Updater::protectedApplicationsRoot() == root);

    _putenv_s("EACP_APPHUB_INSTALL_ROOT", "");
    // With the override cleared the default Program Files location is used.
    check(Updater::protectedApplicationsRoot().string().find("Tamber")
          != std::string::npos);
};

#endif // defined(_WIN32)
