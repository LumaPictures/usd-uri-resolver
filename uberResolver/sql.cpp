#include "sql.h"

#include <pxr/base/tf/diagnosticLite.h>

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <errmsg.h>

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <locale>
#include <time.h>
#include <iostream>

#include <z85/z85.hpp>

// -------------------------------------------------------------------------------
// If you want to print out a stacktrace everywhere SQL_WARN is called, set this
// to a value > 0 - it will print out this number of stacktrace entries
#define USD_SQL_DEBUG_STACKTRACE_SIZE 0

#if USD_SQL_DEBUG_STACKTRACE_SIZE > 0

#include <execinfo.h>

#define SQL_WARN \
    { \
        void* backtrace_array[USD_SQL_DEBUG_STACKTRACE_SIZE]; \
        size_t stack_size = backtrace(backtrace_array, USD_SQL_DEBUG_STACKTRACE_SIZE); \
        TF_WARN("\n\n====================================\n"); \
        TF_WARN("Stacktrace:\n"); \
        backtrace_symbols_fd(backtrace_array, stack_size, STDERR_FILENO); \
    } \
    TF_WARN

#else // STACKTRACE_SIZE

#define SQL_WARN TF_WARN

#endif // STACKTRACE_SIZE

// -------------------------------------------------------------------------------

// If you want to control the number of seconds an idle connection is kept alive
// for, set this to something other than zero

#define SESSION_WAIT_TIMEOUT 0

#if SESSION_WAIT_TIMEOUT > 0

#define _USD_SQL_SIMPLE_QUOTE(ARG) #ARG
#define _USD_SQL_EXPAND_AND_QUOTE(ARG) _SIMPLE_QUOTE(ARG)
#define SET_SESSION_WAIT_TIMEOUT_QUERY ( "SET SESSION wait_timeout=" _USD_SQL_EXPAND_AND_QUOTE( SESSION_WAIT_TIMEOUT ) )
#define SET_SESSION_WAIT_TIMEOUT_QUERY_STRLEN ( sizeof(SET_SESSION_WAIT_TIMEOUT_QUERY) - 1 )


#endif // SESSION_WAIT_TIMEOUT

// -------------------------------------------------------------------------------

namespace {
    using mutex_scoped_lock = std::lock_guard<std::mutex>;

    std::string generate_name(const std::string& base, const std::string& extension, char* buffer) {
        std::tmpnam(buffer);
        std::string ret(buffer);
        const auto last_slash = ret.find_last_of('/');
        if (last_slash == std::string::npos) {
            return base + ret + extension;
        } else {
            return base + ret.substr(last_slash + 1) + extension;
        }
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

    std::string parse_path(const std::string& path) {
        constexpr auto schema_length = strlen(usd_sql::SQL_PREFIX);
        return path.substr(schema_length);
    }

    double convert_char_to_time(const char* raw_time) {
        // GCC 4.8 doesn't have the get_time function of C++11
        std::tm parsed_time = {};
        strptime(raw_time, "%Y-%m-%d %H:%M:%S", &parsed_time);
        parsed_time.tm_isdst = 0;
        // I have to set daylight savings to 0
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
                 "SELECT timestamp FROM %s WHERE path = '%s' LIMIT 1",
                 table_name.c_str(), asset_path.c_str());
        unsigned long query_length = strlen(query);
        const auto query_ret = mysql_real_query(connection, query, query_length);
        // I only have to flush when there is a successful query.
        if (query_ret != 0) {
            SQL_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
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
                SQL_WARN("[uberResolver] Wrong type for time field. Found %i instead of 7.", field->type);
            }
            mysql_free_result(result);
        }
        return ret;
    }
}

namespace usd_sql {
    struct SQLConnection {
        enum CacheState {
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
        std::string cache_path;
        char tmp_name_buffer[TMP_MAX];
        MYSQL* connection;

        SQLConnection(const std::string& server_name) : connection(
                mysql_init(nullptr)) {
            cache_path = get_env_var(server_name, CACHE_PATH_ENV_VAR, "/tmp/");
            if (cache_path.back() != '/') {
                cache_path += "/";
            }
            const auto server_user = get_env_var(server_name, USER_ENV_VAR,
                                                 "root");
            const auto compacted_default_pass = z85::encode_with_padding(
                    std::string("12345678"));
            auto server_password = get_env_var(server_name, PASSWORD_ENV_VAR,
                                               compacted_default_pass);
            server_password = z85::decode_with_padding(server_password);
            const auto server_db = get_env_var(server_name, DB_ENV_VAR, "usd");
            table_name = get_env_var(server_name, TABLE_ENV_VAR, "headers");
            const auto server_port = static_cast<unsigned int>(
                    atoi(get_env_var(server_name, PORT_ENV_VAR, "3306").c_str()));
            // Turn on auto-reconnect
            // Note that it IS still possible for the reconnect to fail, and we
            // don't do any explicit check for this; experimented with also adding
            // a check with mysql_ping in get_connection after retrieving
            // a cached result, but decided against this approach, as it added
            // a lot of extra spam if something DID go wrong (because a lot of
            // resolve name calls will still "work" by just using the cached
            // result
            // Also, it's good practice to add error checking after every usage
            // of mysql anyway (can fail in more ways than just connection being
            // lost), so these will catch / print error when reconnection fails
            // as well
            my_bool reconnect = 1;
            mysql_options(connection, MYSQL_OPT_RECONNECT, &reconnect);
            const auto ret = mysql_real_connect(
                    connection, server_name.c_str(),
                    server_user.c_str(), server_password.c_str(),
                    server_db.c_str(), server_port, nullptr, 0);
            if (ret == nullptr) {
                mysql_close(connection);
                SQL_WARN("[uberResolver] Failed to connect to: %s\nReason: %s",
                        server_name.c_str(), mysql_error(connection));
                connection = nullptr;
            }
#if SESSION_WAIT_TIMEOUT > 0
            else {
                const auto query_ret = mysql_real_query(connection,
                                                        SET_SESSION_WAIT_TIMEOUT_QUERY,
                                                        SET_SESSION_WAIT_TIMEOUT_QUERY_STRLEN);
                if (query_ret != 0) {
                    SQL_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                             SET_SESSION_WAIT_TIMEOUT_QUERY, mysql_errno(connection), mysql_error(connection));
                }

            }
#endif // SESSION_WAIT_TIMEOUT
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
            Cache cache{
                    CACHE_MISSING,
                    ""
            };

            MYSQL_RES* result = nullptr;
            constexpr size_t query_max_length = 4096;
            char query[query_max_length];
            snprintf(query, query_max_length,
                     "SELECT EXISTS(SELECT 1 FROM %s WHERE path = '%s')",
                     table_name.c_str(), asset_path.c_str());
            auto query_length = strlen(query);
            const auto query_ret = mysql_real_query(connection, query,
                                                    query_length);
            // I only have to flush when there is a successful query.
            if (query_ret != 0) {
                SQL_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                        query, mysql_errno(connection), mysql_error(connection));
            }
            else {
                result = mysql_store_result(connection);
            }

            if (result != nullptr) {
                assert(mysql_num_rows(result) == 1);
                auto row = mysql_fetch_row(result);
                assert(mysql_num_fields(result) == 1);
                if (row[0] != nullptr && strcmp(row[0], "1") == 0) {
                    const auto last_dot = asset_path.find_last_of('.');
                    if (last_dot != std::string::npos) {
                        cache.local_path = generate_name(cache_path,
                                                         asset_path.substr(
                                                                 last_dot),
                                                         tmp_name_buffer);
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
                SQL_WARN("[uberResolver] %s was not resolved before fetching!",
                        asset_path.c_str());
                return false;
            }
            else {
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
                    const auto query_ret = mysql_real_query(connection, query,
                                                            query_length);
                    // I only have to flush when there is a successful query.
                    if (query_ret != 0) {
                        SQL_WARN("[uberResolver] Error executing query: %s\nError code: %i\nError string: %s",
                                query, mysql_errno(connection),
                                mysql_error(connection));
                    }
                    else {
                        result = mysql_store_result(connection);
                    }

                    if (result != nullptr) {
                        assert(mysql_num_rows(result) == 1);
                        auto row = mysql_fetch_row(result);
                        assert(mysql_num_fields(result) == 1);
                        auto field = mysql_fetch_field(result);
                        if (row[0] != nullptr && field->max_length > 0) {
                            std::fstream fs(cached_result->second.local_path,
                                            std::ios::out | std::ios::binary);
                            fs.write(row[0], field->max_length);
                            fs.flush();
                            cached_result->second.state = CACHE_FETCHED;
                            cached_result->second.timestamp = get_timestamp_raw(
                                    connection, table_name, asset_path);
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
            if (cached_result == cached_queries.end() ||
                cached_result->second.state == CACHE_MISSING) {
                SQL_WARN("[uberResolver] %s is missing when querying timestamps!",
                        asset_path.c_str());
                return 1.0;
            }
            else {
                const auto ret = get_timestamp_raw(connection, table_name,
                                                   asset_path);
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

    SQLConnection* SQL::get_connection(bool create) {
        sql_thread_init();
        SQLConnection* conn = nullptr;
        {
            const auto server_name = getenv(HOST_ENV_VAR);
            if (server_name == nullptr) {
                SQL_WARN("[uberResolver] Could not get host name - make sure $%s"
                                " is defined", HOST_ENV_VAR);
                return conn;
            }
            mutex_scoped_lock sc(connections_mutex);
            conn = find_in_sorted_vector<connection_pair::first_type,
                    connection_pair::second_type, nullptr>(connections,
                                                           server_name);
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
        auto conn = get_connection(true);
        if (conn == nullptr) {
            return "";
        }
        else {
            return conn->resolve_name(parsed_path);
        }
    }

    bool SQL::fetch_asset(const std::string& path) {
        const auto parsed_path = parse_path(path);
        auto conn = get_connection(false);
        // fetching asset will be after resolving, thus there should be a server
        if (conn == nullptr) {
            return false;
        }
        else {
            return conn->fetch(parsed_path);
        }
    }

    bool SQL::matches_schema(const std::string& path) {
        return path.find(SQL_PREFIX) == 0;
    }

    double SQL::get_timestamp(const std::string& path) {
        const auto parsed_path = parse_path(path);
        auto conn = get_connection(false);
        if (conn == nullptr) {
            return 1.0;
        }
        else {
            return conn->get_timestamp(parsed_path);
        }
    }

    void SQL::sort_connections() {
        // auto in lambdas require c++14 :(
        std::sort(connections.begin(), connections.end(),
                  [](const connection_pair& a, const connection_pair& b) {
                      return a.first < b.first;
                  });
    }
}