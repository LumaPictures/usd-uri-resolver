#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>

namespace usd_sql {
    struct SQLConnection;

    constexpr const char SQL_PREFIX[] = "sql://";
    constexpr const char HOST_ENV_VAR[] = "USD_SQL_DBHOST";
    constexpr const char PORT_ENV_VAR[] = "USD_SQL_PORT";
    constexpr const char DB_ENV_VAR[] = "USD_SQL_DB";
    constexpr const char TABLE_ENV_VAR[] = "USD_SQL_TABLE";
    constexpr const char USER_ENV_VAR[] = "USD_SQL_USER";
    constexpr const char PASSWORD_ENV_VAR[] = "USD_SQL_PASSWD";
    constexpr const char CACHE_PATH_ENV_VAR[] = "USD_SQL_CACHE_PATH";

    class SQL {
    public:
        SQL();
        ~SQL();
        void clear();

        std::string resolve_name(const std::string& path);
        bool fetch_asset(const std::string& path);
        bool matches_schema(const std::string& path);
        double get_timestamp(const std::string& path);

    private:
        using connection_pair = std::pair<std::string, SQLConnection*>;
        SQLConnection* get_connection(bool create);
        void sort_connections();
        std::mutex connections_mutex;
        std::vector<connection_pair> connections;
    };
}