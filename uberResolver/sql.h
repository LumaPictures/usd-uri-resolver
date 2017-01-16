#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>

struct SQLConnection;

class SQLInstance {
public:
    SQLInstance();
    ~SQLInstance();
    void clear();

    std::string resolve_name(const std::string& server_name, const std::string& asset_path);
    bool fetch_asset(const std::string& server_name, const std::string& asset_path);
    bool matches_schema(const std::string& path);
    double get_timestamp(const std::string& server_name, const std::string& asset_path);
    std::tuple<std::string, std::string> parse_path(const std::string& path);

private:
    using connection_pair = std::pair<std::string, SQLConnection*>;
    SQLConnection* get_connection(const std::string& server_name, bool create);
    void sort_connections();
    std::mutex connections_mutex;
    std::vector<connection_pair> connections;
};
