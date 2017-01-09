#ifndef AR_UBER_RESOLVER_H
#define AR_UBER_RESOLVER_H

#include "pxr/usd/ar/defaultResolver.h"

#include <tbb/enumerable_thread_specific.h>

#include <memory>
#include <string>
#include <vector>

/// \class uberResolver
///
/// A generic resolver, which can load up resolver plugins and assign them to
/// predefined URIs.
///
class uberResolver
    : public ArDefaultResolver
{
public:
    uberResolver();
    virtual ~uberResolver();

    virtual std::string Resolve(const std::string& path) override;

    virtual std::string ResolveWithAssetInfo(
        const std::string& path,
        ArAssetInfo* assetInfo) override;

    virtual void UpdateAssetInfo(
        const std::string& identifier,
        const std::string& filePath,
        const std::string& fileVersion,
        ArAssetInfo* assetInfo) override;

    virtual VtValue GetModificationTimestamp(
        const std::string& path,
        const std::string& resolvedPath) override;

    virtual bool FetchToLocalResolvedPath(
        const std::string& path,
        const std::string& resolvedPath) override;
};

#endif // AR_UBER_RESOLVER_H
