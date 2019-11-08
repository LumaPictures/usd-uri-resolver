#pragma once

#include <pxr/pxr.h>

#include <pxr/base/tf/token.h>

#include <pxr/usd/ar/asset.h>

#include <iostream>
#include <mutex>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct SQLConnection;
class SQLResolver {
public:
    SQLResolver() {}
    ~SQLResolver() {}
    void clear() {}

    std::string find_asset(const std::string& path) {return path;}
    bool matches_schema(const std::string& path) {
        std::cout << "USDURIResolver is active..." << std::endl;
        return false;
    }
    double get_timestamp(const std::string& path) {return 0.0;}
    std::shared_ptr<ArAsset> open_asset(const std::string& path) {return nullptr;}

private:
    // using connection_pair = std::pair<std::string, SQLConnection*>;
    // SQLConnection* get_connection(bool create);
    // std::mutex connections_mutex;
    // std::vector<connection_pair> connections;
};

PXR_NAMESPACE_CLOSE_SCOPE
