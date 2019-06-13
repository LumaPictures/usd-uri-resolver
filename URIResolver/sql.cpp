#include "sql.h"

#include <pxr/base/tf/diagnosticLite.h>

#include <errmsg.h>
#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>

#include <time.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <unordered_map>

#include <z85/z85.hpp>

#include "debug_codes.h"
#include "memory_asset.h"

PXR_NAMESPACE_OPEN_SCOPE

// -----------------------------------------------------------------------------

namespace {

constexpr auto SQL_PREFIX = "sql://";
constexpr auto SQL_PREFIX_SHORT = "sql:";
constexpr auto HOST_ENV_VAR = "USD_SQL_DBHOST";
constexpr auto PORT_ENV_VAR = "USD_SQL_PORT";
constexpr auto DB_ENV_VAR = "USD_SQL_DB";
constexpr auto TABLE_ENV_VAR = "USD_SQL_TABLE";
constexpr auto USER_ENV_VAR = "USD_SQL_USER";
constexpr auto PASSWORD_ENV_VAR = "USD_SQL_PASSWD";

constexpr double INVALID_TIME = std::numeric_limits<double>::lowest();

struct MySQLResultDeleter {
    void operator()(MYSQL_RES* r) const { mysql_free_result(r); }
};
using MySQLResult = std::unique_ptr<MYSQL_RES, MySQLResultDeleter>;

using mutex_scoped_lock = std::lock_guard<std::mutex>;

// Included in other source file. For improving readibility it's defined here.

// -----------------------------------------------------------------------------
// If you want to print out a stacktrace everywhere SQL_WARN is called, set this
// to a value > 0 - it will print out this number of stacktrace entries
#define USD_SQL_DEBUG_STACKTRACE_SIZE 0

#if USD_SQL_DEBUG_STACKTRACE_SIZE > 0

#include <execinfo.h>

#define SQL_WARN                                                          \
    {                                                                     \
        void* backtrace_array[USD_SQL_DEBUG_STACKTRACE_SIZE];             \
        size_t stack_size =                                               \
            backtrace(backtrace_array, USD_SQL_DEBUG_STACKTRACE_SIZE);    \
        TF_WARN("\n\n====================================\n");            \
        TF_WARN("Stacktrace:\n");                                         \
        backtrace_symbols_fd(backtrace_array, stack_size, STDERR_FILENO); \
    }                                                                     \
    TF_WARN

#else // STACKTRACE_SIZE

#define SQL_WARN TF_WARN

#endif // STACKTRACE_SIZE

// -----------------------------------------------------------------------------

// If you want to control the number of seconds an idle connection is kept alive
// for, set this to something other than zero

#define SESSION_WAIT_TIMEOUT 0

#if SESSION_WAIT_TIMEOUT > 0

#define _USD_SQL_SIMPLE_QUOTE(ARG) #ARG
#define _USD_SQL_EXPAND_AND_QUOTE(ARG) _SIMPLE_QUOTE(ARG)
#define SET_SESSION_WAIT_TIMEOUT_QUERY                      \
    ("SET SESSION wait_timeout=" _USD_SQL_EXPAND_AND_QUOTE( \
        SESSION_WAIT_TIMEOUT))
#define SET_SESSION_WAIT_TIMEOUT_QUERY_STRLEN \
    (sizeof(SET_SESSION_WAIT_TIMEOUT_QUERY) - 1)

#endif // SESSION_WAIT_TIMEOUT

// Clang tidy/static analyzer complains about this.
constexpr size_t cstrlen(const char* str) {
    return *str != 0 ? 1 + cstrlen(str + 1) : 0;
}

thread_local std::once_flag thread_flag;

void sql_thread_init() {
    std::call_once(thread_flag, []() { my_thread_init(); });
}

std::string get_env_var(
    const std::string& server_name, const std::string& env_var,
    const std::string& default_value) {
    const auto env_first = getenv((server_name + "_" + env_var).c_str());
    if (env_first != nullptr) { return env_first; }
    const auto env_second = getenv(env_var.c_str());
    if (env_second != nullptr) { return env_second; }
    return default_value;
}

template <
    typename key_t, typename value_t, value_t default_value = value_t(),
    typename pair_t = std::pair<key_t, value_t>>
value_t find_in_sorted_vector(
    const std::vector<pair_t>& vec, const key_t& key) {
    const auto ret = std::lower_bound(
        vec.begin(), vec.end(), pair_t{key, default_value},
        [](const pair_t& a, const pair_t& b) { return a.first < b.first; });
    if (ret != vec.end() && ret->first == key) {
        return ret->second;
    } else {
        return default_value;
    }
}

std::string parse_path(const std::string& path) {
    constexpr auto schema_length_short = cstrlen(SQL_PREFIX_SHORT);
    constexpr auto schema_length = cstrlen(SQL_PREFIX);
    if (path.find(SQL_PREFIX) == 0) {
        return path.substr(schema_length);
    } else {
        return path.substr(schema_length_short);
    }
}

std::string clean_path(const std::string& path) {
  return path.find(SQL_PREFIX) == 0
         ? std::string(path).replace(0, cstrlen(SQL_PREFIX), SQL_PREFIX_SHORT)
         : path;
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

double convert_mysql_result_to_time(
    MYSQL_FIELD* field, MYSQL_ROW row, size_t field_i) {
    auto ret = INVALID_TIME;

    if (!row) {
        SQL_WARN("[SQLResolver] row was null");
    } else if (!field) {
        SQL_WARN(
            "[SQLResolver] could not find the field type to retrieve a "
            "timestamp");
    } else if (field->type != MYSQL_TYPE_TIMESTAMP) {
        SQL_WARN(
            "[SQLResolver] Wrong type for time field. Found %i instead of 7.",
            field->type);
    } else if (row[field_i] == nullptr) {
        SQL_WARN("[SQLResolver] Field %lu was null", field_i);
    } else if (field->max_length <= 0) {
        SQL_WARN("[SQLResolver] Field %lu had 0 length", field_i);
    } else {
        ret = convert_char_to_time(row[field_i]);
    }
    return ret;
}

double get_timestamp_raw(
    MYSQL* connection, const std::string& table_name,
    const TfToken& asset_path) {
    constexpr size_t query_max_length = 4096;
    char query[query_max_length];
    snprintf(
        query, query_max_length,
        "SELECT timestamp FROM %s WHERE path = '%s' LIMIT 1",
        table_name.c_str(), asset_path.GetText());
    unsigned long query_length = cstrlen(query);
    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg("get_timestamp_raw: query:\n%s\n", query);
    const auto query_ret = mysql_real_query(connection, query, query_length);
    // I only have to flush when there is a successful query.
    MySQLResult result;
    if (query_ret != 0) {
        SQL_WARN(
            "[SQLResolver] Error executing query: %s\nError code: %i\nError "
            "string: %s",
            query, mysql_errno(connection), mysql_error(connection));
    } else {
        result.reset(mysql_store_result(connection));
    }
    if (result == nullptr) { return INVALID_TIME; }
    if (mysql_num_rows(result.get()) != 1) { return INVALID_TIME; }

    auto row = mysql_fetch_row(result.get());
    assert(mysql_num_fields(result) == 1);
    auto field = mysql_fetch_field(result.get());
    const auto time = convert_mysql_result_to_time(field, row, 0);
    if (time == INVALID_TIME) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg("get_timestamp_raw: failed to convert timestamp\n");
    } else {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg("get_timestamp_raw: got: %f\n", time);
    }
    return time;
}

enum CacheState { CACHE_MISSING, CACHE_NEEDS_FETCHING, CACHE_FETCHED };

struct Cache {
    CacheState state;
    TfToken local_path;
    double timestamp;
    std::shared_ptr<ArAsset> asset;
};

} // namespace

struct SQLConnection {
    SQLConnection(const std::string& server_name);

    std::mutex connection_mutex; // do we actually need this? the api should
                                 // support multithreaded queries!
    std::unordered_map<TfToken, Cache, TfToken::HashFunctor> cached_queries;
    std::string table_name;
    MYSQL* connection;

    bool find_asset(const std::string& asset_path);
    double get_timestamp(const std::string& asset_path);
    std::shared_ptr<ArAsset> open_asset(const std::string& asset_path);
};

SQLResolver::SQLResolver() { my_init(); }

SQLResolver::~SQLResolver() { clear(); }

void SQLResolver::clear() {}

SQLConnection* SQLResolver::get_connection(bool create) {
    sql_thread_init();
    SQLConnection* conn = nullptr;
    {
        const auto server_name = getenv(HOST_ENV_VAR);
        if (server_name == nullptr) {
            SQL_WARN(
                "[SQLResolver] Could not get host name - make sure $%s"
                " is defined",
                HOST_ENV_VAR);
            return conn;
        }
        mutex_scoped_lock sc(connections_mutex);
        conn = find_in_sorted_vector<
            connection_pair::first_type, connection_pair::second_type, nullptr>(
            connections, server_name);
        if (create && conn == nullptr) { // initialize new connection
            conn = new SQLConnection(server_name);
            connections.emplace_back(server_name, conn);
            std::sort(
                connections.begin(), connections.end(),
                [](const connection_pair& a, const connection_pair& b) -> bool {
                    return a.first < b.first;
                });
        }
    }
    return conn;
}

std::string SQLResolver::find_asset(const std::string& path) {
    auto conn = get_connection(true);
    if (conn == nullptr) {
        return {};
    }
    const auto cleaned_path = clean_path(path);
    return conn->find_asset(cleaned_path) ? cleaned_path : "";
}

bool SQLResolver::matches_schema(const std::string& path) {
    constexpr auto schema_length_short = cstrlen(SQL_PREFIX_SHORT);
    return path.compare(0, schema_length_short, SQL_PREFIX_SHORT) == 0;
}

double SQLResolver::get_timestamp(const std::string& path) {
    auto conn = get_connection(false);
    return conn == nullptr ? 1.0 : conn->get_timestamp(clean_path(path));
}

std::shared_ptr<ArAsset> SQLResolver::open_asset(const std::string& path) {
    auto conn = get_connection(false);
    return conn == nullptr ? nullptr : conn->open_asset(clean_path(path));
}

SQLConnection::SQLConnection(const std::string& server_name)
    : connection(mysql_init(nullptr)) {
    const auto server_user = get_env_var(server_name, USER_ENV_VAR, "root");
    const auto compacted_default_pass =
        z85::encode_with_padding(std::string("12345678"));
    auto server_password =
        get_env_var(server_name, PASSWORD_ENV_VAR, compacted_default_pass);
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
        connection, server_name.c_str(), server_user.c_str(),
        server_password.c_str(), server_db.c_str(), server_port, nullptr, 0);
    if (ret == nullptr) {
        mysql_close(connection);
        SQL_WARN(
            "[SQLResolver] Failed to connect to: %s\nReason: %s",
            server_name.c_str(), mysql_error(connection));
        connection = nullptr;
    }
#if SESSION_WAIT_TIMEOUT > 0
    else {
        const auto query_ret = mysql_real_query(
            connection, SET_SESSION_WAIT_TIMEOUT_QUERY,
            SET_SESSION_WAIT_TIMEOUT_QUERY_STRLEN);
        if (query_ret != 0) {
            SQL_WARN(
                "[SQLResolver] Error executing query: %s\nError code: "
                "%i\nError string: %s",
                SET_SESSION_WAIT_TIMEOUT_QUERY, mysql_errno(connection),
                mysql_error(connection));
        }
    }
#endif // SESSION_WAIT_TIMEOUT
}

bool SQLConnection::find_asset(const std::string& asset_path) {
    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg("SQLConnection::find_asset: '%s'\n", asset_path.c_str());
    if (connection == nullptr) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::find_asset: aborting due to null "
                "connection pointer\n");
        return false;
    }

    const auto last_dot = asset_path.find_last_of('.');
    if (last_dot == std::string::npos) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::find_asset: asset path missing extension "
                "('%s')\n",
                asset_path.c_str());
        return false;
    }
    mutex_scoped_lock sc(connection_mutex);

    const TfToken asset_path_token(asset_path);
    auto cached_result = cached_queries.find(asset_path_token);

    auto fill_cache_data = [&](Cache& cache) -> bool {
        const TfToken parsed_path(parse_path(asset_path));
        constexpr size_t query_max_length = 4096;
        char query[query_max_length];
        snprintf(
            query, query_max_length,
            "SELECT EXISTS(SELECT 1 FROM %s WHERE path = '%s')",
            table_name.c_str(), parsed_path.GetText());
        auto query_length = strlen(query);
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg("SQLConnection::find_asset: query:\n%s\n", query);
        const auto query_ret =
            mysql_real_query(connection, query, query_length);
        // I only have to flush when there is a successful query.
        MySQLResult result;
        if (query_ret != 0) {
            SQL_WARN(
                "[SQLResolver] Error executing query: %s\nError code: "
                "%i\nError string: %s",
                query, mysql_errno(connection), mysql_error(connection));
            return false;
        } else {
            result.reset(mysql_store_result(connection));
        }

        if (result == nullptr) { return false; }

        assert(mysql_num_rows(result.get()) == 1);
        auto row = mysql_fetch_row(result.get());
        assert(mysql_num_fields(result.get()) == 1);

        if (row[0] == nullptr || strcmp(row[0], "1") != 0) { return false; }
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg("SQLConnection::find_asset: found: %s\n", asset_path.c_str());
        cache.local_path = parsed_path;
        cache.state = CACHE_NEEDS_FETCHING;
        cache.timestamp = 1.0;
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::find_asset: local path: %s\n",
                cache.local_path.GetText());
        return true;
    };

    if (cached_result != cached_queries.end()) {
        if (cached_result->second.state != CACHE_MISSING) {
            TF_DEBUG(USD_URI_SQL_RESOLVER)
                .Msg(
                    "SQLConnection::find_asset: using cached result: "
                    "'%s'\n",
                    cached_result->second.local_path.GetText());
            return !cached_result->second.local_path.IsEmpty();
        }
        return fill_cache_data(cached_result->second);
    } else {
        Cache cache{CACHE_MISSING, TfToken()};
        const auto result = fill_cache_data(cache);
        cached_queries.emplace(asset_path_token, cache);
        return result;
    }
}

double SQLConnection::get_timestamp(const std::string& asset_path) {
    if (connection == nullptr) { return 1.0; }

    mutex_scoped_lock sc(connection_mutex);
    const auto cached_result = cached_queries.find(TfToken(asset_path));
    if (cached_result == cached_queries.end() ||
        cached_result->second.state == CACHE_MISSING) {
        SQL_WARN(
            "[SQLResolver] %s is missing when querying timestamps!",
            asset_path.c_str());
        return 1.0;
    }
    const auto stamp = get_timestamp_raw(
        connection, table_name, cached_result->second.local_path);
    if (stamp == INVALID_TIME) {
        cached_result->second.state = CACHE_MISSING;
        SQL_WARN(
            "[SQLResolver] Failed to parse timestamp for %s, returning the"
            "existing value.",
            asset_path.c_str());
        return cached_result->second.timestamp;
    } else if (stamp > cached_result->second.timestamp) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::get_timestamp: %s timestamp has changed from "
                "%f to %f\n",
                asset_path.c_str(), cached_result->second.timestamp, stamp);
        cached_result->second.state = CACHE_NEEDS_FETCHING;
    }
    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg(
            "SQLConnection::get_timestamp: timestamp of %f for %s", stamp,
            asset_path.c_str());
    return stamp;
}

std::shared_ptr<ArAsset> SQLConnection::open_asset(
    const std::string& asset_path) {
    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg("SQLConnection::open_asset: '%s'\n", asset_path.c_str());
    if (connection == nullptr) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::open_asset: aborting due to null connection "
                "pointer\n");
        return nullptr;
    }

    mutex_scoped_lock sc(connection_mutex);
    const auto cached_result = cached_queries.find(TfToken(asset_path));
    if (cached_result == cached_queries.end()) {
        SQL_WARN(
            "[SQLResolver] %s was not resolved before fetching!",
            asset_path.c_str());
        return nullptr;
    }

    if (cached_result->second.state == CACHE_MISSING) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::open_asset: missing from database, no fetch\n");
        return nullptr;
    }

    if (cached_result->second.state == CACHE_FETCHED) {
        // Ensure cached state is up to date before deciding not to fetch
        // (there is no guarantee that get_timestamp was called prior to
        // fetch)
        auto current_timestamp = get_timestamp_raw(
            connection, table_name, cached_result->second.local_path);
        // So we can fail faster next time.
        if (current_timestamp == INVALID_TIME ||
            current_timestamp <= cached_result->second.timestamp) {
            return cached_result->second.asset;
        } else {
            TF_DEBUG(USD_URI_SQL_RESOLVER)
                .Msg(
                    "SQLConnection::open_asset: local path data is out of "
                    "date.\n");
        }
    }

    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg("SQLConnection::fetch: Cache needed fetching\n");
    cached_result->second.state =
        CACHE_MISSING; // we'll set this up if fetching is successful

    MySQLResult result;
    constexpr size_t query_max_length = 4096;
    char query[query_max_length];
    snprintf(
        query, query_max_length,
        "SELECT data, timestamp FROM %s WHERE path = '%s' LIMIT 1",
        table_name.c_str(), cached_result->second.local_path.GetText());
    unsigned long query_length = strlen(query);
    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg("SQLConnection::open_asset: query:\n%s\n", query);
    const auto query_ret = mysql_real_query(connection, query, query_length);
    // I only have to flush when there is a successful query.
    if (query_ret != 0) {
        SQL_WARN(
            "[SQLResolver] Error executing query: %s\nError code: "
            "%i\nError string: %s",
            query, mysql_errno(connection), mysql_error(connection));
    } else {
        result.reset(mysql_store_result(connection));
    }

    if (result == nullptr) { return nullptr; }

    if (mysql_num_rows(result.get()) != 1) { return nullptr; }

    auto row = mysql_fetch_row(result.get());
    assert(mysql_num_fields(result) == 2);
    auto field = mysql_fetch_field(result.get());
    if (row[0] == nullptr && field->max_length == 0) { return nullptr; }

    TF_DEBUG(USD_URI_SQL_RESOLVER)
        .Msg(
            "SQLConnection::open_asset: successfully fetched "
            "data\n");
    cached_result->second.asset.reset(
        new MemoryAsset(row[0], field->max_length));
    cached_result->second.state = CACHE_FETCHED;

    field = mysql_fetch_field(result.get());
    cached_result->second.timestamp =
        convert_mysql_result_to_time(field, row, 1);
    if (cached_result->second.timestamp == INVALID_TIME) {
        TF_DEBUG(USD_URI_SQL_RESOLVER)
            .Msg(
                "SQLConnection::open_asset: failed parsing "
                "timestamp\n");
    }
    return cached_result->second.asset;
}

PXR_NAMESPACE_CLOSE_SCOPE
