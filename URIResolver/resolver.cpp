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

AR_DEFINE_RESOLVER(URIResolver, ArResolver)

URIResolver::URIResolver() : ArDefaultResolver()
{
}

URIResolver::~URIResolver()
{
    g_sql.clear();
}

std::string URIResolver::Resolve(const std::string& path)
{
    return URIResolver::ResolveWithAssetInfo(path, nullptr);
}

bool URIResolver::IsRelativePath(const std::string& path)
{
    return !g_sql.matches_schema(path) && ArDefaultResolver::IsRelativePath(path);
}

std::string URIResolver::ResolveWithAssetInfo(
    const std::string& path,
    ArAssetInfo* assetInfo)
{
    return g_sql.matches_schema(path) ?
           g_sql.resolve_name(path) :
           ArDefaultResolver::ResolveWithAssetInfo(path, assetInfo);
}

void URIResolver::UpdateAssetInfo(
    const std::string& identifier,
    const std::string& filePath,
    const std::string& fileVersion,
    ArAssetInfo* assetInfo)
{
    ArDefaultResolver::UpdateAssetInfo(identifier, filePath, fileVersion, assetInfo);
}

VtValue URIResolver::GetModificationTimestamp(
    const std::string& path,
    const std::string& resolvedPath)
{
    return g_sql.matches_schema(path) ?
           VtValue(g_sql.get_timestamp(path)) :
           ArDefaultResolver::GetModificationTimestamp(path, resolvedPath);
}

bool URIResolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    return !g_sql.matches_schema(path) || g_sql.fetch_asset(path);
}

PXR_NAMESPACE_CLOSE_SCOPE
