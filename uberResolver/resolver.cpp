//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "resolver.h"

#include "pxr/usd/ar/assetInfo.h"
#include "pxr/usd/ar/resolverContext.h"

#include <pxr/usd/ar/defineResolver.h>

#include <map>
#include <cstdio>
#include <mutex>
#include <thread>
#include <fstream>

namespace {
    constexpr const char* placeholderURI = "sphere://";
    const char* placeholderAsset = R"usd(
#usda 1.0

def Xform "hello"
{
    def Sphere "world"
    {
    }
}
    )usd";

    // Temp solution
    std::mutex g_resolvedNamesMutex;
    using mutex_scoped_lock = std::lock_guard<std::mutex>;
    std::map<std::string, std::string> g_resolvedNames;

    std::string resolveName(const std::string path)
    {
        mutex_scoped_lock sc(g_resolvedNamesMutex);
        const auto it = g_resolvedNames.find(path);
        if (it != g_resolvedNames.end()) {
            return it->second;
        } else {
            const auto resolvedName = std::tmpnam(nullptr) + std::string(".usda");
            g_resolvedNames.insert(std::make_pair(path, resolvedName));
            return resolvedName;
        }
    }
}

AR_DEFINE_RESOLVER(uberResolver, ArResolver)

uberResolver::uberResolver() : ArDefaultResolver()
{
}

uberResolver::~uberResolver()
{
}

std::string uberResolver::Resolve(const std::string& path)
{
    return uberResolver::ResolveWithAssetInfo(path, nullptr);
}

std::string uberResolver::ResolveWithAssetInfo(
    const std::string& path,
    ArAssetInfo* assetInfo)
{
    if (path.find(placeholderURI) == 0) {
        const auto resolved_name = resolveName(path);
        return resolved_name;
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
    if (path.find(placeholderURI) == 0) {
        return VtValue(1.0);
    } else {
        return ArDefaultResolver::GetModificationTimestamp(path, resolvedPath);
    }
}

bool uberResolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    if (path.find(placeholderURI) == 0) {
        std::fstream fs(resolvedPath, std::ios::out);
        fs << placeholderAsset;
        return true;
    } else {
        return true;
    }
}
