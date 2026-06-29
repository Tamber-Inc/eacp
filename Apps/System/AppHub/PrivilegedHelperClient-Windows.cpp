#include "PrivilegedHelperClient.h"

#include <eacp/Core/Process/Process.h>

#include <Miro/Miro.h>

#include <windows.h>
#include <shellapi.h>

#if defined(_MSC_VER)
#pragma comment(lib, "shell32.lib")
#endif

#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace AppHub
{
namespace fs = std::filesystem;
namespace Processes = eacp::Processes;

namespace
{
std::optional<fs::path> currentModuleDirectory()
{
    auto buffer = std::array<wchar_t, MAX_PATH> {};
    auto size =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size())
        return std::nullopt;

    return fs::path(buffer.data()).parent_path();
}

// The helper executable ships next to the hub. EACP_APPHUB_HELPER_PATH lets
// tests and CI point at a freshly built helper without an install step.
std::optional<fs::path> helperExecutablePath()
{
    if (auto* helperOverride = std::getenv("EACP_APPHUB_HELPER_PATH");
        helperOverride != nullptr && *helperOverride != '\0')
    {
        auto path = fs::path(helperOverride);
        std::error_code ec;
        if (fs::exists(path, ec))
            return path;
        return std::nullopt;
    }

    auto directory = currentModuleDirectory();
    if (!directory)
        return std::nullopt;

    auto path = *directory / "AppHubPrivilegedHelper.exe";
    std::error_code ec;
    if (fs::exists(path, ec))
        return path;
    return std::nullopt;
}

fs::path helperIoDirectory()
{
    static std::atomic<unsigned long long> counter {0};
    auto base = fs::temp_directory_path() / "Tamber-AppHub-helper";
    auto directory = base / std::to_string(counter.fetch_add(1));
    std::error_code ec;
    fs::create_directories(directory, ec);
    return directory;
}

std::string readFile(const fs::path& path)
{
    auto in = std::ifstream(path, std::ios::binary);
    if (!in)
        return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool writeFile(const fs::path& path, const std::string& text)
{
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << text;
    return static_cast<bool>(out);
}

bool installRootIsWritable()
{
    auto root = eacp::Updater::protectedApplicationsRoot();
    std::error_code ec;
    fs::create_directories(root, ec);

    static std::atomic<unsigned long long> counter {0};
    auto probe = root / (".eacp-write-probe-" + std::to_string(counter.fetch_add(1)));
    auto out = std::ofstream(probe, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << "probe";
    out.close();

    fs::remove(probe, ec);
    return true;
}

std::wstring widen(const std::string& value)
{
    if (value.empty())
        return {};
    auto size = MultiByteToWideChar(CP_UTF8,
                                    0,
                                    value.c_str(),
                                    static_cast<int>(value.size()),
                                    nullptr,
                                    0);
    auto result = std::wstring(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        value.c_str(),
                        static_cast<int>(value.size()),
                        result.data(),
                        size);
    return result;
}

std::wstring quote(const fs::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

// Launches the helper with the runas verb so Windows elevates it through UAC,
// then blocks until it exits. Used when the install root is not writable as the
// current user (the normal, unelevated case).
bool runHelperElevated(const fs::path& helper,
                       const std::string& command,
                       const fs::path& requestPath,
                       const fs::path& resultPath,
                       std::string& error)
{
    auto parameters = widen(command) + L" " + quote(requestPath) + L" "
                    + quote(resultPath);

    auto info = SHELLEXECUTEINFOW {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    info.lpVerb = L"runas";
    auto helperPath = helper.wstring();
    info.lpFile = helperPath.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info) || info.hProcess == nullptr)
    {
        error = "failed to launch elevated privileged helper";
        return false;
    }

    auto waitResult = WaitForSingleObject(info.hProcess, 120 * 1000);
    if (waitResult != WAIT_OBJECT_0)
    {
        CloseHandle(info.hProcess);
        error = "privileged helper timed out";
        return false;
    }

    CloseHandle(info.hProcess);
    return true;
}

eacp::Updater::InstallResult invokeHelper(
    const eacp::Updater::PrivilegedAppBundleInstallRequest& request)
{
    auto result = eacp::Updater::InstallResult();

    // Fast path for tests/unit runs: skip the process boundary entirely.
    if (auto* inProcess = std::getenv("EACP_APPHUB_HELPER_INPROCESS");
        inProcess != nullptr && std::string_view(inProcess) == "1")
        return eacp::Updater::installAppBundleArtifact(request);

    auto helper = helperExecutablePath();
    if (!helper)
    {
        result.ok = false;
        result.error = "privileged helper executable was not found";
        return result;
    }

    auto io = helperIoDirectory();
    auto requestPath = io / "request.json";
    auto resultPath = io / "result.json";

    if (!writeFile(requestPath, Miro::toJSONString(request)))
    {
        result.ok = false;
        result.error = "failed to stage privileged helper request";
        return result;
    }

    auto cleanup = [&]
    {
        std::error_code ec;
        fs::remove_all(io, ec);
    };

    if (installRootIsWritable())
    {
        // Already elevated (or a writable dev/test root): run the helper
        // directly so its output is captured synchronously.
        auto run = Processes::run(
            helper->string(),
            {"installAppBundle", requestPath.string(), resultPath.string()});
        if (!run.launched)
        {
            cleanup();
            result.ok = false;
            result.error = "failed to launch privileged helper";
            return result;
        }
    }
    else
    {
        auto error = std::string();
        if (!runHelperElevated(*helper,
                               "installAppBundle",
                               requestPath,
                               resultPath,
                               error))
        {
            cleanup();
            result.ok = false;
            result.error = error;
            return result;
        }
    }

    auto reply = readFile(resultPath);
    cleanup();

    if (reply.empty())
    {
        result.ok = false;
        result.error = "privileged helper did not produce a result";
        return result;
    }

    try
    {
        Miro::fromJSONString(result, reply);
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error = std::string("invalid privileged helper reply: ") + e.what();
    }

    return result;
}
} // namespace

PrivilegedHelperInstallResult installPrivilegedHelper()
{
    auto result = PrivilegedHelperInstallResult();

    // On Windows the helper ships alongside the hub and is elevated on demand,
    // so "installing" it means confirming the executable is present and runnable.
    auto helper = helperExecutablePath();
    if (!helper)
    {
        result.ok = false;
        result.error = "privileged helper executable was not found";
        return result;
    }

    auto run = Processes::run(helper->string(), {"--version"});
    if (!run.exited || run.exitCode != 0)
    {
        result.ok = false;
        result.error = "privileged helper failed to report its version";
        return result;
    }

    result.ok = true;
    return result;
}

eacp::Updater::InstallResult installAppBundleWithPrivilegedHelper(
    const eacp::Updater::PrivilegedAppBundleInstallRequest& request)
{
    return invokeHelper(request);
}

} // namespace AppHub
