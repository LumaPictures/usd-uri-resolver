#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>

struct SQLConnection;

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
