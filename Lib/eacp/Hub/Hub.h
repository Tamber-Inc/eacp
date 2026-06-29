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
    std::string remoteCatalogUrl;
};

struct CatalogLoadOptions
{
    bool preferRemote = true;
    int remoteAttempts = 1;
};

using CatalogFallback = std::function<Updater::ProductCatalog()>;

std::filesystem::path remoteDownloadRoot(const std::filesystem::path& stateRoot);
std::filesystem::path cachedCatalogPath(const std::filesystem::path& stateRoot);

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
