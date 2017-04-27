#include "resolver.h"

#include <pxr/usd/ar/assetInfo.h>
#include <pxr/usd/ar/resolverContext.h>

#include <pxr/usd/ar/defineResolver.h>
#include <pxr/usd/ar/defaultResolver.h>

#include <map>
#include <cstdio>
#include <mutex>
#include <thread>
#include <fstream>
#include <memory>

#include <unistd.h>

#include "sql.h"

/*
 * Depending on the asset count and access frequency, it could be better to store the
 * resolver paths in a sorted vector, rather than a map. That's way faster when we are
 * doing significantly more queries inserts.
 */

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    usd_sql::SQL g_sql;
}

AR_DEFINE_RESOLVER(uberResolver, ArResolver)

uberResolver::uberResolver() : ArDefaultResolver()
{
}

uberResolver::~uberResolver()
{
    g_sql.clear();
}

std::string uberResolver::Resolve(const std::string& path)
{
    return uberResolver::ResolveWithAssetInfo(path, nullptr);
}

std::string uberResolver::ResolveWithAssetInfo(
    const std::string& path,
    ArAssetInfo* assetInfo)
{
    if (g_sql.matches_schema(path)) {
        return g_sql.resolve_name(path);
    } else {
        return ArDefaultResolver::ResolveWithAssetInfo(path, assetInfo);
    }
}

void uberResolver::UpdateAssetInfo(
    const std::string& identifier,
    const std::string& filePath,
    const std::string& fileVersion,
    ArAssetInfo* assetInfo)
{
    ArDefaultResolver::UpdateAssetInfo(identifier, filePath, fileVersion, assetInfo);
}

VtValue uberResolver::GetModificationTimestamp(
    const std::string& path,
    const std::string& resolvedPath)
{
    if (g_sql.matches_schema(path)) {
        return VtValue(g_sql.get_timestamp(path));
    } else {
        return ArDefaultResolver::GetModificationTimestamp(path, resolvedPath);
    }
}

bool uberResolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    if (g_sql.matches_schema(path)) {
        return g_sql.fetch_asset(path);
    } else {
        return true;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
