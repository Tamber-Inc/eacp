#include <eacp/Hub/Hub.h>

#include <eacp/Network/HTTP/Http.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace eacp::Hub
{
namespace fs = std::filesystem;
namespace HTTP = eacp::HTTP;

namespace
{

void writeFile(const fs::path& path, const std::string& text)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

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

std::optional<Updater::ProductCatalog> parseCatalog(const std::string& raw)
{
    if (raw.empty())
        return std::nullopt;

    try
    {
        return Updater::parseCatalogJson(raw);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

} // namespace

fs::path remoteDownloadRoot(const fs::path& stateRoot)
{
    return stateRoot / "remote-downloads";
}

fs::path cachedCatalogPath(const fs::path& stateRoot)
{
    return remoteDownloadRoot(stateRoot) / "apphub-catalog.json";
}

std::optional<Updater::ProductCatalog> loadCatalogFromPath(const fs::path& path)
{
    if (path.empty())
        return std::nullopt;

    return parseCatalog(readFile(path));
}

std::optional<Updater::ProductCatalog> fetchRemoteCatalog(
    const CatalogConfig& config)
{
    if (config.remoteCatalogUrl.empty())
        return std::nullopt;

    auto response = HTTP::Request(config.remoteCatalogUrl).perform();
    if (response.statusCode < 200 || response.statusCode >= 300)
        return std::nullopt;

    auto catalog = parseCatalog(response.content);
    if (!catalog)
        return std::nullopt;

    writeFile(cachedCatalogPath(config.stateRoot), response.content);
    return catalog;
}

Updater::ProductCatalog loadCatalog(const CatalogConfig& config,
                                    const CatalogLoadOptions& options,
                                    const CatalogFallback& fallback)
{
    if (auto manual = loadCatalogFromPath(config.manualCatalogPath))
        return *manual;

    if (options.preferRemote)
    {
        for (auto attempt = 0; attempt < options.remoteAttempts; ++attempt)
        {
            if (auto remote = fetchRemoteCatalog(config))
                return *remote;
        }
    }

    if (auto cached = loadCatalogFromPath(cachedCatalogPath(config.stateRoot)))
        return *cached;

    return fallback();
}

Updater::ProductCatalog loadCatalogContaining(const CatalogConfig& config,
                                              const std::string& productId,
                                              const CatalogFallback& fallback)
{
    auto local = loadCatalog(config, {.preferRemote = false}, fallback);
    if (Updater::findProduct(local, productId) != nullptr)
        return local;

    if (!config.manualCatalogPath.empty())
        return local;

    for (auto attempt = 0; attempt < 12; ++attempt)
    {
        if (auto remote = fetchRemoteCatalog(config))
        {
            if (Updater::findProduct(*remote, productId) != nullptr)
                return *remote;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return loadCatalog(config, {.preferRemote = false}, fallback);
}

} // namespace eacp::Hub
