#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>

namespace usd_sql {
    struct SQLConnection;

    constexpr auto SQL_PREFIX = "sql://";
    constexpr auto HOST_ENV_VAR = "USD_SQL_DBHOST";
    constexpr auto PORT_ENV_VAR = "USD_SQL_PORT";
    constexpr auto DB_ENV_VAR = "USD_SQL_DB";
    constexpr auto TABLE_ENV_VAR = "USD_SQL_TABLE";
    constexpr auto USER_ENV_VAR = "USD_SQL_USER";
    constexpr auto PASSWORD_ENV_VAR = "USD_SQL_PASSWD";
    constexpr auto CACHE_PATH_ENV_VAR = "USD_SQL_CACHE_PATH";

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