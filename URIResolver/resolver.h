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
class URIResolver : public ArDefaultResolver {
public:
    URIResolver();
    ~URIResolver() override;

    std::string Resolve(const std::string& path) override;

    bool IsRelativePath(const std::string& path) override;

    std::string ResolveWithAssetInfo(
        const std::string& path, ArAssetInfo* assetInfo) override;

    // Quick workaround for a bug in USD 0.8.4.
    std::string AnchorRelativePath(
        const std::string& anchorPath, const std::string& path) override;

    VtValue GetModificationTimestamp(
        const std::string& path, const std::string& resolvedPath) override;

    bool FetchToLocalResolvedPath(
        const std::string& path, const std::string& resolvedPath) override;

    std::shared_ptr<ArAsset> OpenAsset(
        const std::string& resolvedPath) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
