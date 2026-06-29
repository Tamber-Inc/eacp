#pragma once

#include <eacp/Updater/Updater.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace eacp::Hub
{

struct CatalogConfig
{
    std::filesystem::path stateRoot;
    std::filesystem::path manualCatalogPath;
    std::string channel = "stable";
    std::string remoteCatalogUrl;
    std::string channelReleaseBaseUrl;
    std::string channelReleaseTagPrefix = "apphub-channel-";
    std::string channelCatalogAssetName = "apphub-catalog.json";
    std::string channelCatalogUrlTemplate;
};

struct CatalogLoadOptions
{
    bool preferRemote = true;
    int remoteAttempts = 1;
};

using CatalogFallback = std::function<Updater::ProductCatalog()>;

std::filesystem::path remoteDownloadRoot(const std::filesystem::path& stateRoot);
std::filesystem::path cachedCatalogPath(const std::filesystem::path& stateRoot);
std::filesystem::path cachedCatalogPath(const std::filesystem::path& stateRoot,
                                        const std::string& channel);
std::string normalizedChannel(std::string channel);
std::string channelReleaseTag(const CatalogConfig& config);
std::string resolvedCatalogUrl(const CatalogConfig& config);

std::optional<Updater::ProductCatalog>
    loadCatalogFromPath(const std::filesystem::path& path);

std::optional<Updater::ProductCatalog> fetchRemoteCatalog(
    const CatalogConfig& config);

Updater::ProductCatalog loadCatalog(const CatalogConfig& config,
                                    const CatalogLoadOptions& options,
                                    const CatalogFallback& fallback);

Updater::ProductCatalog loadCatalogContaining(const CatalogConfig& config,
                                              const std::string& productId,
                                              const CatalogFallback& fallback);

} // namespace eacp::Hub
