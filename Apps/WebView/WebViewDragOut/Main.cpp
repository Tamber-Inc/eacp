#include <eacp/WebView/WebView.h>
#include <ResEmbed/ResEmbed.h>
#include <WebResources.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace eacp;
using namespace Graphics;

namespace
{
constexpr auto category = "DragOutApp";

// A draggable file: an absolute on-disk path plus where it came from. Bundled
// entries are ResEmbed resources materialised to a temp file; the rest are real
// files scanned from ~/Downloads. Both drag out identically (public.file-url).
struct FileEntry
{
    std::string path;
    std::string name;
    std::string source;
};

constexpr std::array audioExtensions =
    {".wav", ".mp3", ".aif", ".aiff", ".flac", ".m4a", ".aac", ".ogg"};

bool isAudioFile(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return std::find(audioExtensions.begin(), audioExtensions.end(), ext)
        != audioExtensions.end();
}

std::filesystem::path downloadsDir()
{
    const auto* home = std::getenv("HOME");
    return std::filesystem::path {home != nullptr ? home : ""} / "Downloads";
}

// Materialise an embedded resource to a temp file so it has a real path the OS
// can copy when dragged out. Returns the absolute path, or empty if missing.
std::string extractBundledAsset(const std::string& name)
{
    auto asset = ResEmbed::get(name, category);

    if (! asset)
        return {};

    auto ec = std::error_code {};
    auto dir = std::filesystem::temp_directory_path() / "eacp-dragout";
    std::filesystem::create_directories(dir, ec);

    auto path = dir / name;
    auto out = std::ofstream {path, std::ios::binary};
    out.write(asset.asCharPointer(), static_cast<std::streamsize>(asset.size()));

    return path.string();
}

std::vector<FileEntry> buildFileList()
{
    auto entries = std::vector<FileEntry> {};

    // Bundled ResEmbed assets first, to show embedded resources drag out too.
    for (const auto* name: {"sample.png", "sample.mp3"})
    {
        auto path = extractBundledAsset(name);
        if (! path.empty())
            entries.push_back({path, name, "bundled"});
    }

    // Then real audio files from ~/Downloads.
    auto ec = std::error_code {};
    auto downloads = std::vector<std::string> {};

    for (const auto& entry: std::filesystem::directory_iterator(downloadsDir(), ec))
    {
        if (entry.is_regular_file(ec) && isAudioFile(entry.path()))
            downloads.push_back(entry.path().string());
    }

    std::sort(downloads.begin(), downloads.end());

    for (const auto& path: downloads)
    {
        entries.push_back({path,
                           std::filesystem::path {path}.filename().string(),
                           "~/Downloads"});
    }

    return entries;
}

std::string jsonEscape(const std::string& s)
{
    auto out = std::string {};
    out.reserve(s.size());

    for (char c: s)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (c == '\n')
        {
            out += "\\n";
        }
        else if (static_cast<unsigned char>(c) >= 0x20)
        {
            out += c;
        }
    }

    return out;
}

// window.__eacpFiles = [{ id, name, source }, ...] -- injected at document start
// so the page can render the list before its own scripts run. The page refers
// to files by id (index); the native side owns the absolute paths and never
// trusts a path from the renderer.
std::string buildFileListInjection(const std::vector<FileEntry>& files)
{
    auto js = std::string {"window.__eacpFiles = ["};

    for (std::size_t i = 0; i < files.size(); ++i)
    {
        js += "{\"id\":" + std::to_string(i) + ",\"name\":\""
            + jsonEscape(files[i].name) + "\",\"source\":\""
            + jsonEscape(files[i].source) + "\"}";

        if (i + 1 < files.size())
            js += ",";
    }

    js += "];";
    return js;
}

WebView::Options dragOutOptions()
{
    auto opts = embeddedOptions(category);
    // Defer the load so we can inject the file list before the page runs.
    opts.embedded.autoLoad = false;
    return opts;
}
} // namespace

struct DragOutApp
{
    DragOutApp()
        : files(buildFileList())
    {
        webView.onFileDragRequested = [this](const std::string& payload)
        { return resolveDragPaths(payload); };

        webView.addUserScript(buildFileListInjection(files));
        webView.loadURL("app://local/index.html");

        setApplicationMenuBar(buildDefaultWebViewMenuBar());
        window.setContentView(webView);
    }

    // payload is a comma-separated list of file ids (indices into `files`).
    std::vector<std::string> resolveDragPaths(const std::string& payload) const
    {
        auto paths = std::vector<std::string> {};
        auto start = std::size_t {0};

        while (start <= payload.size())
        {
            auto comma = payload.find(',', start);
            auto token = payload.substr(
                start, comma == std::string::npos ? std::string::npos : comma - start);

            auto allDigits = ! token.empty()
                && std::all_of(token.begin(), token.end(), [](unsigned char c)
                               { return std::isdigit(c) != 0; });

            if (allDigits)
            {
                auto idx = static_cast<std::size_t>(std::stoul(token));
                if (idx < files.size())
                    paths.push_back(files[idx].path);
            }

            if (comma == std::string::npos)
                break;

            start = comma + 1;
        }

        return paths;
    }

    std::vector<FileEntry> files;
    WebView webView {dragOutOptions()};
    Window window;
};

int main()
{
    eacp::Apps::run<DragOutApp>();
    return 0;
}
