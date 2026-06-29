// Windows privileged helper for AppHub.
//
// This is the elevated worker process that owns machine-level writes. It is the
// Windows analogue of the macOS SMJobBless XPC daemon (PrivilegedHelper/Main.mm):
// the hub stages and verifies an artifact as the signed-in user, then hands a
// narrow, already-verified install request to this process, which re-validates
// independently before touching the protected install root. It speaks the same
// JSON command protocol as the macOS helper.
//
// Usage:
//   AppHubPrivilegedHelper --version
//   AppHubPrivilegedHelper installAppBundle <requestPath> <resultPath>
//
// The request and result are exchanged through files so the unprivileged hub
// can write the request and read the reply across the elevation boundary.

#include <eacp/Updater/Updater.h>

#include <Miro/Miro.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifndef EACP_APPHUB_HELPER_LABEL
#define EACP_APPHUB_HELPER_LABEL "com.tamber.AppHub.PrivilegedHelper"
#endif

#ifndef EACP_APPHUB_HELPER_VERSION
#define EACP_APPHUB_HELPER_VERSION "1.0.0"
#endif

namespace
{
std::string readFile(const std::string& path)
{
    auto in = std::ifstream(path, std::ios::binary);
    if (!in)
        return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool writeFile(const std::string& path, const std::string& text)
{
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << text;
    return static_cast<bool>(out);
}

eacp::Updater::InstallResult handleInstallAppBundle(const std::string& payload)
{
    auto request = eacp::Updater::PrivilegedAppBundleInstallRequest();
    Miro::fromJSONString(request, payload);
    return eacp::Updater::installAppBundleArtifact(request);
}

eacp::Updater::InstallResult handleCommand(const std::string& command,
                                           const std::string& payload)
{
    if (command == "installAppBundle")
        return handleInstallAppBundle(payload);

    auto result = eacp::Updater::InstallResult();
    result.ok = false;
    result.error = "unknown privileged helper command";
    return result;
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "--version")
    {
        std::cout << EACP_APPHUB_HELPER_LABEL << " " << EACP_APPHUB_HELPER_VERSION
                  << "\n";
        return 0;
    }

    if (argc < 4)
    {
        std::cerr << "usage: AppHubPrivilegedHelper <command> <requestPath> "
                     "<resultPath>\n";
        return 2;
    }

    auto command = std::string(argv[1]);
    auto requestPath = std::string(argv[2]);
    auto resultPath = std::string(argv[3]);

    auto result = eacp::Updater::InstallResult();
    try
    {
        result = handleCommand(command, readFile(requestPath));
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error = e.what();
    }
    catch (...)
    {
        result.ok = false;
        result.error = "unknown privileged helper error";
    }

    if (!writeFile(resultPath, Miro::toJSONString(result)))
    {
        std::cerr << "failed to write privileged helper result\n";
        return 1;
    }

    return result.ok ? 0 : 1;
}
