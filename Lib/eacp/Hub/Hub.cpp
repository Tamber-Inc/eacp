#include <eacp/Hub/Hub.h>

#include <eacp/Network/HTTP/Http.h>

#include <chrono>
#include <cctype>
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

std::string replaceAll(std::string text,
                       const std::string& token,
                       const std::string& replacement)
{
    auto pos = std::string::size_type {};
    while ((pos = text.find(token, pos)) != std::string::npos)
    {
        text.replace(pos, token.size(), replacement);
        pos += replacement.size();
    }
    return text;
}

std::string safeChannelPathName(const std::string& channel)
{
    auto out = std::string();
    auto lastWasDash = false;
    for (auto c: normalizedChannel(channel))
    {
        auto ch = static_cast<unsigned char>(c);
        auto keep = std::isalnum(ch) || c == '.' || c == '_' || c == '-';
        if (keep)
        {
            out.push_back(c);
            lastWasDash = false;
            continue;
        }

        if (!lastWasDash)
            out.push_back('-');
        lastWasDash = true;
    }

    while (!out.empty() && out.front() == '-')
        out.erase(out.begin());
    while (!out.empty() && out.back() == '-')
        out.pop_back();

    return out.empty() ? "stable" : out;
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

fs::path cachedCatalogPath(const fs::path& stateRoot, const std::string& channel)
{
    auto normalized = normalizedChannel(channel);
    if (normalized == "stable")
        return cachedCatalogPath(stateRoot);
    return remoteDownloadRoot(stateRoot) / "catalogs"
           / (safeChannelPathName(normalized) + ".json");
}

std::string normalizedChannel(std::string channel)
{
    while (!channel.empty()
           && std::isspace(static_cast<unsigned char>(channel.front())))
        channel.erase(channel.begin());
    while (!channel.empty()
           && std::isspace(static_cast<unsigned char>(channel.back())))
        channel.pop_back();
    return channel.empty() ? "stable" : channel;
}

std::string channelReleaseTag(const CatalogConfig& config)
{
    auto channel = normalizedChannel(config.channel);
    if (channel == "stable")
        return "stable";
    return config.channelReleaseTagPrefix + safeChannelPathName(channel);
}

std::string resolvedCatalogUrl(const CatalogConfig& config)
{
    auto channel = normalizedChannel(config.channel);
    if (channel == "stable")
        return config.remoteCatalogUrl;

    auto releaseTag = channelReleaseTag(config);
    if (!config.channelCatalogUrlTemplate.empty())
    {
        auto url = replaceAll(config.channelCatalogUrlTemplate,
                             "{channel}",
                             safeChannelPathName(channel));
        url = replaceAll(url, "{releaseTag}", releaseTag);
        url = replaceAll(url, "{asset}", config.channelCatalogAssetName);
        return url;
    }

    if (config.channelReleaseBaseUrl.empty())
        return {};

    auto base = config.channelReleaseBaseUrl;
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    return base + "/" + releaseTag + "/" + config.channelCatalogAssetName;
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
    auto catalogUrl = resolvedCatalogUrl(config);
    if (catalogUrl.empty())
        return std::nullopt;

    auto response = HTTP::Request(catalogUrl).perform();
    if (response.statusCode < 200 || response.statusCode >= 300)
        return std::nullopt;

    auto catalog = parseCatalog(response.content);
    if (!catalog)
        return std::nullopt;

    writeFile(cachedCatalogPath(config.stateRoot, config.channel),
              response.content);
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

    if (auto cached =
            loadCatalogFromPath(cachedCatalogPath(config.stateRoot,
                                                  config.channel)))
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
