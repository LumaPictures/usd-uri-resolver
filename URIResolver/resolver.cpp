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

#include <unistd.h>

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

std::string URIResolver::Resolve(const std::string& path) {
    TF_DEBUG(USD_URI_RESOLVER).Msg("Resolve('%s'\n)", path.c_str());
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
    return SQL.matches_schema(path)
               ? (SQL.find_asset(path) ? path : "")
               : ArDefaultResolver::ResolveWithAssetInfo(path, assetInfo);
}

std::string URIResolver::AnchorRelativePath(
    const std::string& anchorPath, const std::string& path) {
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

void URIResolver::UpdateAssetInfo(
    const std::string& identifier, const std::string& filePath,
    const std::string& fileVersion, ArAssetInfo* assetInfo) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "UpdateAssetInfo('%s', '%s', '%s', ...)\n", identifier.c_str(),
            filePath.c_str(), fileVersion.c_str());
    if (!SQL.matches_schema(identifier)) {
        ArDefaultResolver::UpdateAssetInfo(
            identifier, filePath, fileVersion, assetInfo);
    }
}

VtValue URIResolver::GetModificationTimestamp(
    const std::string& path, const std::string& resolvedPath) {
    TF_DEBUG(USD_URI_RESOLVER)
        .Msg(
            "GetModificationTimestamp('%s', '%s')\n", path.c_str(),
            resolvedPath.c_str());
    return SQL.matches_schema(path)
               ? VtValue(SQL.get_timestamp(path))
               : ArDefaultResolver::GetModificationTimestamp(
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
    return SQL.matches_schema(resolvedPath)
               ? SQL.open_asset(resolvedPath)
               : ArDefaultResolver::OpenAsset(resolvedPath);
}

PXR_NAMESPACE_CLOSE_SCOPE
