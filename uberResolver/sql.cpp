#include "sql.h"

#include <pxr/base/tf/diagnosticLite.h>

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <errmsg.h>

#include <fstream>
#include <iomanip>
#include <algorithm>

#include <z85/z85.hpp>

namespace {
    using mutex_scoped_lock = std::lock_guard<std::mutex>;

    std::string generate_name(const std::string& extension) {
        return std::tmpnam(nullptr) + extension;
    }

    thread_local std::once_flag thread_flag;

    void sql_thread_init() {
        std::call_once(thread_flag, [](){my_thread_init();});
    }

    std::string get_env_var(const std::string& server_name, const std::string& env_var, const std::string& default_value) {
        const auto env_first = getenv((server_name + "_" + env_var).c_str());
        if (env_first != nullptr) {
            return env_first;
        }
        const auto env_second = getenv(env_var.c_str());
        if (env_second != nullptr) {
            return env_second;
        }
        return default_value;
    }

    template <typename key_t, typename value_t,
        value_t default_value = value_t(), typename pair_t = std::pair<key_t, value_t>>
    value_t find_in_sorted_vector(const std::vector<pair_t>& vec, const key_t& key) {
        const auto ret = std::lower_bound(vec.begin(), vec.end(), pair_t{key, default_value},
                                          [](const pair_t& a,
                                             const pair_t& b) {
                                              return a.first < b.first;
                                          });
        if (ret != vec.end() && ret->first == key) {
            return ret->second;
        } else {
            return default_value;
        }
    }

    std::tuple<std::string, std::string> parse_path(const std::string& path) {
        constexpr auto schema_length = strlen("sql://");
        const auto query_path = path.substr(schema_length);
        const auto first_slash = query_path.find("/");
        if (first_slash == std::string::npos || first_slash == 0) {
            return std::tuple<std::string, std::string>{"", ""};
        } else {
            const auto server_name = query_path.substr(0, first_slash);
            // the lib has issues when using localhost instead of the ip address
            return std::tuple<std::string, std::string>{server_name == "localhost" ? "127.0.0.1" : server_name,
                                                        query_path.substr(first_slash)};
        }
    }

    double convert_char_to_time(const char* raw_time) {
        std::tm parsed_time = {};
        std::istringstream is(raw_time);
        is >> std::get_time(&parsed_time, "%Y-%m-%d %H:%M:%S");
        parsed_time.tm_isdst = 0; // I have to set daylight savings to 0
        // for the asctime function to match the actual time
        // even without that, the parsed times will be consistent, so
        // probably it won't cause any issues
        return mktime(&parsed_time);
    }

    double get_timestamp_raw(MYSQL* connection, const std::string& table_name, const std::string& asset_path) {
        MYSQL_RES* result = nullptr;
        constexpr size_t query_max_length = 4096;
        char query[query_max_length];
        snprintf(query, query_max_length,
                 "SELECT time FROM %s WHERE path = '%s' LIMIT 1",
                 table_name.c_str(), asset_path.c_str());
        unsigned long query_length = strlen(query);
        const auto query_ret = mysql_real_query(connection, query, query_length);
        // I only have to flush when there is a successful query.
        if (query_ret != 0) {
            TF_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                    query, mysql_errno(connection), mysql_error(connection));
        } else {
            result = mysql_store_result(connection);
        }

        auto ret = 1.0;

        if (result != nullptr) {
            assert(mysql_num_rows(result) == 1);
            auto row = mysql_fetch_row(result);
            assert(mysql_num_fields(result) == 1);
            auto field = mysql_fetch_field(result);
            if (field->type == MYSQL_TYPE_TIMESTAMP) {
                if (row[0] != nullptr && field->max_length > 0) {
                    ret = convert_char_to_time(row[0]);
                }
            } else {
                TF_WARN("[uberResolver] Wrong type for time field. Found %i instead of 7.", field->type);
            }
            mysql_free_result(result);
        }

        return ret;
    }

}

struct SQLConnection {
    enum CacheState{
        CACHE_MISSING,
        CACHE_NEEDS_FETCHING,
        CACHE_FETCHED
    };
    struct Cache {
        CacheState state;
        std::string local_path;
        double timestamp;
    };
    std::mutex connection_mutex; // do we actually need this? the api should support multithreaded queries!
    std::map<std::string, Cache> cached_queries;
    std::string table_name;
    MYSQL* connection;

    SQLConnection(const std::string& server_name) : connection(mysql_init(nullptr)) {
        const auto server_user = get_env_var(server_name, "USD_SQL_USER", "root");
        const std::string compacted_default_pass = z85::encode_with_padding(std::string("12345678"));
        auto server_password = get_env_var(server_name, "USD_SQL_PASSWD", compacted_default_pass);
        server_password = z85::decode_with_padding(server_password);
        const auto server_db = get_env_var(server_name, "USD_SQL_DB", "usd");
        table_name = get_env_var(server_name, "USD_SQL_TABLE", "headers");
        const auto server_port = static_cast<unsigned int>(
            atoi(get_env_var(server_name, "USD_SQL_PORT", "3306").c_str()));
        const auto ret = mysql_real_connect(
            connection, server_name.c_str(),
            server_user.c_str(), server_password.c_str(),
            server_db.c_str(), server_port, nullptr, 0);
        if (ret == nullptr) {
            mysql_close(connection);
            TF_WARN("[uberResolver] Failed to connect to: %s\nReason: %s",
                    server_name.c_str(), mysql_error(connection));
            connection = nullptr;
        }
    }

    ~SQLConnection() {
        for (const auto& cache: cached_queries) {
            if (cache.second.state == CACHE_FETCHED) {
                remove(cache.second.local_path.c_str());
            }
        }
        mysql_close(connection);
    }

    std::string resolve_name(const std::string& asset_path) {
        if (connection == nullptr) {
            return "";
        }
        mutex_scoped_lock sc(connection_mutex);

        const auto cached_result = cached_queries.find(asset_path);
        if (cached_result != cached_queries.end()) {
            return cached_result->second.local_path;
        }
        Cache cache {
            CACHE_MISSING,
            ""
        };

        MYSQL_RES* result = nullptr;
        constexpr size_t query_max_length = 4096;
        char query[query_max_length];
        snprintf(query, query_max_length,
                 "SELECT EXISTS(SELECT 1 FROM %s WHERE path = '%s')",
                 table_name.c_str(), asset_path.c_str());
        unsigned long query_length = strlen(query);
        const auto query_ret = mysql_real_query(connection, query, query_length);
        // I only have to flush when there is a successful query.
        if (query_ret != 0) {
            TF_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                    query, mysql_errno(connection), mysql_error(connection));
        } else {
            result = mysql_store_result(connection);
        }

        if (result != nullptr) {
            assert(mysql_num_rows(result) == 1);
            auto row = mysql_fetch_row(result);
            assert(mysql_num_fields(result) == 1);
            if (row[0] != nullptr && strcmp(row[0], "1") == 0) {
                const auto last_dot = asset_path.find_last_of('.');
                if (last_dot != std::string::npos) {
                    cache.local_path = generate_name(asset_path.substr(last_dot + 1));
                    cache.state = CACHE_NEEDS_FETCHING;
                    cache.timestamp = 1.0;
                }
            }
            mysql_free_result(result);
        }

        cached_queries.insert(std::make_pair(asset_path, cache));
        return cache.local_path;
    }

    bool fetch(const std::string& asset_path) {
        if (connection == nullptr) {
            return false;
        }

        mutex_scoped_lock sc(connection_mutex);
        const auto cached_result = cached_queries.find(asset_path);
        if (cached_result == cached_queries.end()) {
            TF_WARN("[uberResolver] %s was not resolved before fetching!", asset_path.c_str());
            return false;
        } else {
            if (cached_result->second.state == CACHE_MISSING) {
                return false;
            }
            else if (cached_result->second.state == CACHE_NEEDS_FETCHING) {
                cached_result->second.state = CACHE_MISSING; // we'll set this up if fetching is successful

                MYSQL_RES* result = nullptr;
                constexpr size_t query_max_length = 4096;
                char query[query_max_length];
                snprintf(query, query_max_length,
                         "SELECT data FROM %s WHERE path = '%s' LIMIT 1",
                         table_name.c_str(), asset_path.c_str());
                unsigned long query_length = strlen(query);
                const auto query_ret = mysql_real_query(connection, query, query_length);
                // I only have to flush when there is a successful query.
                if (query_ret != 0) {
                    TF_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                            query, mysql_errno(connection), mysql_error(connection));
                } else {
                    result = mysql_store_result(connection);
                }

                if (result != nullptr) {
                    assert(mysql_num_rows(result) == 1);
                    auto row = mysql_fetch_row(result);
                    assert(mysql_num_fields(result) == 1);
                    auto field = mysql_fetch_field(result);
                    if (row[0] != nullptr && field->max_length > 0) {
                        std::fstream fs(cached_result->second.local_path, std::ios::out | std::ios::binary);
                        fs.write(row[0], field->max_length);
                        fs.flush();
                        cached_result->second.state = CACHE_FETCHED;
                        cached_result->second.timestamp = get_timestamp_raw(connection, table_name, asset_path);
                    }
                    mysql_free_result(result);
                }
            }

            return true;
        }
    }

    double get_timestamp(const std::string& asset_path) {
        if (connection == nullptr) {
            return 1.0;
        }

        mutex_scoped_lock sc(connection_mutex);
        const auto cached_result = cached_queries.find(asset_path);
        if (cached_result == cached_queries.end() || cached_result->second.state == CACHE_MISSING) {
            TF_WARN("[uberResolver] %s is missing when querying timestamps!", asset_path.c_str());
            return 1.0;
        } else {
            const auto ret = get_timestamp_raw(connection, table_name, asset_path);
            if (ret > cached_result->second.timestamp) {
                cached_result->second.state = CACHE_NEEDS_FETCHING;
            }
            return ret;
        }
    }
};

SQL::SQL() {
    my_init();
}

SQL::~SQL() {
    clear();
}

void SQL::clear() {
    sql_thread_init();
    mutex_scoped_lock sc(connections_mutex);
    for (const auto& connection : connections) {
        delete connection.second;
    }
    connections.clear();
}

SQLConnection* SQL::get_connection(const std::string& server_name, bool create) {
    sql_thread_init();
    SQLConnection* conn = nullptr;
    {
        mutex_scoped_lock sc(connections_mutex);
        conn = find_in_sorted_vector<connection_pair::first_type,
            connection_pair::second_type, nullptr>(connections, server_name);
        if (create && conn == nullptr) { // initialize new connection
            // TODO
            conn = new SQLConnection(server_name);
            connections.push_back(connection_pair{server_name, conn});
            sort_connections();
        }
    }
    return conn;
}

std::string SQL::resolve_name(const std::string& path) {
    const auto parsed_path = parse_path(path);
    auto conn = get_connection(std::get<0>(parsed_path), true);
    return conn->resolve_name(std::get<1>(parsed_path));
}

bool SQL::fetch_asset(const std::string& path) {
    const auto parsed_path = parse_path(path);
    auto conn = get_connection(std::get<0>(parsed_path), false);
    // fetching asset will be after resolving, thus there should be a server
    if (conn == nullptr) {
        return false;
    } else {
        return conn->fetch(std::get<1>(parsed_path));
    }
}

bool SQL::matches_schema(const std::string& path) {
    return path.find("sql://") == 0;
}

double SQL::get_timestamp(const std::string& path) {
    const auto parsed_path = parse_path(path);
    auto conn = get_connection(std::get<0>(parsed_path), false);
    if (conn == nullptr) {
        return 1.0;
    } else {
        return conn->get_timestamp(std::get<1>(parsed_path));
    }
}

void SQL::sort_connections() {
    // auto in lambdas require c++14 :(
    std::sort(connections.begin(), connections.end(), [](const connection_pair& a, const connection_pair& b) {
        return a.first < b.first;
    });
}
