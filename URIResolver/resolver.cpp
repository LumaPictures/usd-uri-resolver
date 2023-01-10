#include "resolver.h"

#include <pxr/base/tf/pathUtils.h>

#include <pxr/usd/ar/assetInfo.h>
#include <pxr/usd/ar/resolverContext.h>

#include <pxr/usd/ar/defaultResolver.h>
#include <pxr/usd/ar/defineResolver.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "debug_codes.h"
#include "sql.h"

/*
 * Depending on the asset count and access frequency, it could be better to
 * store the resolver paths in a sorted vector, rather than a map. That's way
 * faster when we are doing significantly more queries inserts.
 */

PXR_NAMESPACE_OPEN_SCOPE

namespace {
SQLResolver SQL;
}

AR_DEFINE_RESOLVER(URIResolver, ArResolver)

URIResolver::URIResolver() : ArDefaultResolver() {}

URIResolver::~URIResolver() { /*g_sql.clear();*/
}

bool URIResolver::_ResolveSql(
    const std::string& assetPath, std::string& resolvedPath) const {
    if (SQL.matches_schema(assetPath)) {
        resolvedPath = SQL.find_asset(assetPath);
        return true;
    }
    return false;
}

bool URIResolver::_GetTimestampSql(
    const std::string& assetPath, double& timestamp) const {
    if (SQL.matches_schema(assetPath)) {
        timestamp = SQL.get_timestamp(assetPath);
        return true;
    }
    return false;
}

bool URIResolver::_OpenSqlAsset(
    const std::string& resolvedPath, std::shared_ptr<ArAsset>& asset) const {
    if (SQL.matches_schema(resolvedPath)) {
        asset = SQL.open_asset(resolvedPath);
        return true;
    }
    return false;
}

#if AR_VERSION == 2
ArResolvedPath URIResolver::_Resolve(const std::string& assetPath) const {
    TF_DEBUG(USD_URI_RESOLVER).Msg("_Resolve('%s')\n", assetPath.c_str());
    std::string resolvedPath;
    if (_ResolveSql(assetPath, resolvedPath)) {
        return ArResolvedPath(resolvedPath);
    }
    return ArDefaultResolver::_Resolve(assetPath);
}

ArResolvedPath URIResolver::_ResolveForNewAsset(
    const std::string& assetPath) const {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg("_ResolveForNewAsset('%s')\n", assetPath.c_str());
    std::string resolvedPath;
    if (_ResolveSql(assetPath, resolvedPath)) {
        return ArResolvedPath(resolvedPath);
    }
    return ArDefaultResolver::_ResolveForNewAsset(assetPath);
}

std::string URIResolver::_CreateIdentifierForNewAsset(
    const std::string& assetPath,
    const ArResolvedPath& anchorAssetPath) const {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "_CreateIdentifierForNewAsset('%s', '%s')\n", assetPath.c_str(),
            anchorAssetPath.GetPathString().c_str());
    return SQL.matches_schema(assetPath)
               ? assetPath
               : ArDefaultResolver::_CreateIdentifierForNewAsset(
                   assetPath, anchorAssetPath);
}

ArTimestamp URIResolver::_GetModificationTimestamp(
    const std::string& assetPath, const ArResolvedPath& resolvedPath) const {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "GetModificationTimestamp('%s', '%s')\n", assetPath.c_str(),
            resolvedPath.GetPathString().c_str());
    double timestamp;
    if (_GetTimestampSql(assetPath, timestamp)) {
        return ArTimestamp(timestamp);
    }
    return ArDefaultResolver::_GetModificationTimestamp(
            assetPath, resolvedPath);
}

std::shared_ptr<ArAsset> URIResolver::_OpenAsset(
    const ArResolvedPath& resolvedPath) const {
    TF_DEBUG(USD_URI_RESOLVER).Msg(
            "OpenAsset('%s')\n", resolvedPath.GetPathString());
    std::shared_ptr<ArAsset> asset;
    if (_OpenSqlAsset(resolvedPath.GetPathString(), asset)) {
        return asset;
    }
    return ArDefaultResolver::_OpenAsset(resolvedPath);
}

#else
std::string URIResolver::Resolve(const std::string& path) {
    TF_DEBUG(USD_URI_RESOLVER).Msg("Resolve('%s')\n", path.c_str());
    return URIResolver::ResolveWithAssetInfo(path, nullptr);
}

bool URIResolver::IsRelativePath(const std::string& path) {
    TF_DEBUG(USD_URI_RESOLVER).Msg("IsRelativePath('%s')\n", path.c_str());
    return !SQL.matches_schema(path) && ArDefaultResolver::IsRelativePath(path);
}

std::string URIResolver::ResolveWithAssetInfo(
    const std::string& path, ArAssetInfo* assetInfo) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg("ResolveWithAssetInfo('%s')\n", path.c_str());
    std::string resolvedPath;
    if (_ResolveSql(path, resolvedPath)) {
        return resolvedPath;
    }
    return ArDefaultResolver::ResolveWithAssetInfo(path, assetInfo);
}

std::string URIResolver::AnchorRelativePath(
    const std::string& anchorPath, const std::string& path) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "AnchorRelativePath('%s', '%s')\n", anchorPath.c_str(),
            path.c_str());
    if (TfIsRelativePath(anchorPath) || !IsRelativePath(path)) { return path; }

    // Ensure we are using forward slashes and not back slashes.
    std::string forwardPath = anchorPath;
    std::replace(forwardPath.begin(), forwardPath.end(), '\\', '/');

    // If anchorPath does not end with a '/', we assume it is specifying
    // a file, strip off the last component, and anchor the path to that
    // directory.
    const std::string anchoredPath =
        TfStringCatPaths(TfStringGetBeforeSuffix(forwardPath, '/'), path);
    return TfNormPath(anchoredPath);
}

VtValue URIResolver::GetModificationTimestamp(
    const std::string& path, const std::string& resolvedPath) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "GetModificationTimestamp('%s', '%s')\n", path.c_str(),
            resolvedPath.c_str());
    double timestamp;
    if (_GetTimestampSql(path, timestamp)) {
        return VtValue(timestamp);
    }
    return ArDefaultResolver::GetModificationTimestamp(
            path, resolvedPath);
}

bool URIResolver::FetchToLocalResolvedPath(
    const std::string& path, const std::string& resolvedPath) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "FetchToLocalResolvedPath('%s', '%s')\n", path.c_str(),
            resolvedPath.c_str());
    // We load the asset in OpenAsset.
    return !SQL.matches_schema(path) ||
           ArDefaultResolver::FetchToLocalResolvedPath(path, resolvedPath);
}

std::shared_ptr<ArAsset> URIResolver::OpenAsset(
    const std::string& resolvedPath) {
    TF_DEBUG(USD_URI_RESOLVER).Msg("OpenAsset('%s')\n", resolvedPath.c_str());
    std::shared_ptr<ArAsset> asset;
    if (_OpenSqlAsset(resolvedPath, asset)) {
        return asset;
    }
    return ArDefaultResolver::OpenAsset(resolvedPath);
}
#endif

PXR_NAMESPACE_CLOSE_SCOPE
