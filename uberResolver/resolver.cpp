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

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>


/*
 * Depending on the asset count and access frequency, it could be better to store the
 * resolver paths in a sorted vector, rather than a map. That's way faster when we are
 * doing significantly more queries inserts.
 */

namespace {
    using mutex_scoped_lock = std::lock_guard<std::mutex>;

    std::string generate_name(const std::string& extension) {
        return std::tmpnam(nullptr) + extension;
    }

    thread_local std::once_flag thread_flag;

    void sql_thread_init() {
        std::call_once(thread_flag, [](){my_thread_init();});
    }

    struct connection_data {
        std::mutex connection_mutex; // do we actually need this? the api should support multithreaded queries!
        std::map<std::string, std::string> cached_queries;
        MYSQL* connection;

        connection_data(const std::string& server_name) : connection(nullptr) {
            // TODO:
        }

        ~connection_data() {
            sql_thread_init();
            for (const auto& cache: cached_queries) {
                if (cache.second != "" || cache.second != "_") {
                    remove(cache.second.c_str());
                }
            }
            mysql_close(connection);
        }

        std::string resolve_name(const std::string& asset_path) {
            return "";
        }

        bool fetch(const std::string asset_path) {
            return false;
        }
    };

    template <typename key_t, typename value_t, value_t default_value = value_t()>
    value_t find_in_sorted_vector(const std::vector<std::pair<key_t, value_t>>& vec, const key_t& key) {
        const auto ret = std::lower_bound(vec.begin(), vec.end(), std::pair<key_t, value_t>{key, default_value},
                                          [](const std::pair<key_t, value_t>& a,
                                             const std::pair<key_t, value_t>& b) {
            return a.first < b.first;
        });
        if (ret != vec.end() && ret->first == key) {
            return ret->second;
        } else {
            return default_value;
        }
    };

    struct global_data {
        using connection_pair = std::pair<std::string, connection_data*>;

        global_data() {
            my_init();
        }

        ~global_data() {
            clear();
        }

        void clear() {
            sql_thread_init();
            mutex_scoped_lock sc(connections_mutex);
            for (const auto& connection : connections) {
                delete connection.second;
            }
            connections.clear();
        }

        std::string resolve_name(const std::string& server_name, const std::string& asset_path) {
            sql_thread_init();
            connection_data* conn = nullptr;
            {
                mutex_scoped_lock sc(connections_mutex);
                conn = find_in_sorted_vector<connection_pair::first_type,
                    connection_pair::second_type, nullptr>(connections, server_name);
                if (conn == nullptr) { // initialize new connection
                    // TODO
                    conn = new connection_data(server_name);
                    connections.push_back(connection_pair{server_name, conn});
                    sort_connections();
                }
            }

            return conn->resolve_name(asset_path);
        }

        bool fetch_asset(const std::string& server_name, const std::string& asset_path) {
            sql_thread_init();
            connection_data* conn = nullptr;
            {
                mutex_scoped_lock sc(connections_mutex);
                conn = find_in_sorted_vector<connection_pair::first_type,
                    connection_pair::second_type, nullptr>(connections, server_name);
            }
            // fetching asset will be after resolving, thus there should be a server
            if (conn == nullptr) {
                return false;
            } else {
                return conn->fetch(asset_path);
            }
        }

        bool matches_schema(const std::string& path) {
            return path.find("sql://") == 0;
        }

        void sort_connections() {
            // auto in lambdas require c++14 :(
            std::sort(connections.begin(), connections.end(), [](const connection_pair& a, const connection_pair& b) {
                return a.first < b.first;
            });
        }

        std::mutex connections_mutex;
        std::vector<connection_pair> connections;
    };

    global_data g_sql;
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
        return g_sql.resolve_name("localhost", path);
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
        return VtValue(); // TODO: can we query the entry modification times from SQL?
    } else {
        return ArDefaultResolver::GetModificationTimestamp(path, resolvedPath);
    }
}

bool uberResolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    if (g_sql.matches_schema(path)) {
        return g_sql.fetch_asset("localhost", path);
    } else {
        return true;
    }
}
