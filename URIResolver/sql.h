#pragma once

#include <pxr/pxr.h>

#include <pxr/base/tf/token.h>

#include <pxr/usd/ar/asset.h>

#include <mutex>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct SQLConnection;
class SQLResolver {
public:
    SQLResolver();
    ~SQLResolver();
    void clear();

    bool find_asset(const std::string& path);
    bool matches_schema(const std::string& path);
    double get_timestamp(const std::string& path);
    std::shared_ptr<ArAsset> open_asset(const std::string& path);

private:
    using connection_pair = std::pair<std::string, SQLConnection*>;
    SQLConnection* get_connection(bool create);
    std::mutex connections_mutex;
    std::vector<connection_pair> connections;
};

PXR_NAMESPACE_CLOSE_SCOPE
