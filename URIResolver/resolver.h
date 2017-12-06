#pragma once

#include <pxr/usd/ar/defaultResolver.h>

#include <tbb/enumerable_thread_specific.h>

#include <memory>
#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// \class URIResolver
///
/// Generic resolver to handle Luma needs.
///
class URIResolver
    : public ArDefaultResolver
{
public:
    URIResolver();
    ~URIResolver() override;

    std::string Resolve(const std::string& path) override;

    bool IsRelativePath(const std::string& path) override;

    std::string ResolveWithAssetInfo(
        const std::string& path,
        ArAssetInfo* assetInfo) override;

    void UpdateAssetInfo(
        const std::string& identifier,
        const std::string& filePath,
        const std::string& fileVersion,
        ArAssetInfo* assetInfo) override;

    VtValue GetModificationTimestamp(
        const std::string& path,
        const std::string& resolvedPath) override;

    bool FetchToLocalResolvedPath(
        const std::string& path,
        const std::string& resolvedPath) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
