#include "resolver.h"

#include "pxr/usd/ar/assetInfo.h"
#include "pxr/usd/ar/resolverContext.h"

#include <pxr/usd/ar/defineResolver.h>

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

namespace {
    SQLInstance g_sql;
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
        const auto parsed_request = g_sql.parse_path(path);
        return g_sql.resolve_name(std::get<0>(parsed_request), std::get<1>(parsed_request));
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
        const auto parsed_request = g_sql.parse_path(path);
        return VtValue(g_sql.get_timestamp(std::get<0>(parsed_request), std::get<1>(parsed_request)));
    } else {
        return ArDefaultResolver::GetModificationTimestamp(path, resolvedPath);
    }
}

bool uberResolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    if (g_sql.matches_schema(path)) {
        const auto parsed_request = g_sql.parse_path(path);
        return g_sql.fetch_asset(std::get<0>(parsed_request), std::get<1>(parsed_request));
    } else {
        return true;
    }
}
