#include <eacp/Core/App/App.h>
#include <eacp/Graphics/Graphics.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef EACP_JSON_VIEW_TITLE
#define EACP_JSON_VIEW_TITLE "JsonView"
#endif

#ifndef EACP_JSON_VIEW_VARIANT
#define EACP_JSON_VIEW_VARIANT "JsonView"
#endif

#ifndef EACP_JSON_VIEW_ASSET_ID
#define EACP_JSON_VIEW_ASSET_ID "shared.json-view-demo"
#endif

namespace
{
namespace fs = std::filesystem;
namespace Graphics = eacp::Graphics;

fs::path appHubStateRoot()
{
#if defined(_WIN32)
    if (auto* data = std::getenv("ProgramData"))
        return fs::path(data) / "Tamber" / "AppHub";
    return fs::temp_directory_path() / "Tamber" / "AppHub";
#else
    if (auto* home = std::getenv("HOME"))
        return fs::path(home) / "Library" / "Application Support" / "Tamber"
               / "AppHub";
    return fs::temp_directory_path() / "eacp-apphub";
#endif
}

std::string readText(const fs::path& path)
{
    auto in = std::ifstream(path, std::ios::binary);
    if (!in)
        return {};

    auto out = std::ostringstream();
    out << in.rdbuf();
    return out.str();
}

fs::path jsonAssetPath()
{
    if (auto* overridePath = std::getenv("EACP_JSON_VIEW_ASSET_PATH");
        overridePath != nullptr && *overridePath != '\0')
    {
        return overridePath;
    }

    return appHubStateRoot() / "Applications" / EACP_JSON_VIEW_ASSET_ID
           / "artifact.bin";
}

std::string displayJson()
{
    auto path = jsonAssetPath();
    auto text = readText(path);
    if (!text.empty())
        return text;

    return std::string("{\n")
         + "  \"error\": \"JSON asset is not installed\",\n"
         + "  \"expectedPath\": \"" + path.string() + "\",\n"
         + "  \"sharedProductId\": \"" + EACP_JSON_VIEW_ASSET_ID + "\"\n"
         + "}";
}

struct JsonView final : Graphics::View
{
    JsonView()
        : content(displayJson())
    {
        background->setFillColor({0.08f, 0.10f, 0.12f});
        title->setColor({0.94f, 0.97f, 0.96f});
        subtitle->setColor({0.55f, 0.70f, 0.72f});
        body->setColor({0.82f, 0.89f, 0.86f});
        body->setText(content);
        addChildren({background, title, subtitle, body});
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto path = Graphics::Path();
        path.addRect(bounds);
        background->setPath(path);

        scaleToFit({background, title, subtitle, body});
        title->setPosition({24.f, bounds.h - 48.f});
        subtitle->setPosition({24.f, bounds.h - 78.f});
        body->setPosition({24.f, bounds.h - 132.f});
    }

    std::string content;
    Graphics::ShapeLayerView background;
    Graphics::TextLayerView title {EACP_JSON_VIEW_TITLE};
    Graphics::TextLayerView subtitle {
        std::string(EACP_JSON_VIEW_VARIANT) + " reads "
        + EACP_JSON_VIEW_ASSET_ID};
    Graphics::TextLayerView body;
};

struct JsonViewApp
{
    JsonViewApp()
    {
        window.setContentView(view);
        window.setVisible(true);
        window.toFront();
    }

    static Graphics::WindowOptions windowOptions()
    {
        auto options = Graphics::WindowOptions();
        options.title = EACP_JSON_VIEW_TITLE;
        options.width = 640;
        options.height = 420;
        options.minWidth = 420;
        options.minHeight = 260;
        return options;
    }

    JsonView view;
    Graphics::Window window {windowOptions()};
};
} // namespace

int main(int argc, char* argv[])
{
    eacp::Apps::run<JsonViewApp>(argc, argv);
    return 0;
}
