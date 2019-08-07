#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/staticTokens.h>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>

#include <z85/z85.hpp>

#include <atomic>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(_tokens,
                         (counter));

namespace {

constexpr auto HOST_ENV_VAR = "USD_SQL_DBHOST";
constexpr auto PORT_ENV_VAR = "USD_SQL_PORT";
constexpr auto DB_ENV_VAR = "USD_SQL_DB";
constexpr auto TABLE_ENV_VAR = "USD_SQL_TABLE";
constexpr auto USER_ENV_VAR = "USD_SQL_USER";
constexpr auto PASSWORD_ENV_VAR = "USD_SQL_PASSWD";

std::string get_env_var(
    const std::string& server_name, const std::string& env_var,
    const std::string& default_value) {
    const auto env_first = getenv((server_name + "_" + env_var).c_str());
    if (env_first != nullptr) { return env_first; }
    const auto env_second = getenv(env_var.c_str());
    if (env_second != nullptr) { return env_second; }
    return default_value;
}

std::string generate_stage(int i) {
    auto stage = UsdStage::CreateInMemory(TfStringPrintf("%i.usda", i));

    auto scope = UsdGeomScope::Define(stage, SdfPath("/scope"));
    UsdGeomPrimvarsAPI primvarsAPI(scope);
    primvarsAPI.CreatePrimvar(_tokens->counter, SdfValueTypeNames->Int).Set(i);
    std::string ret;
    stage->GetRootLayer()->ExportToString(&ret);
    return ret;
}

int get_counter(UsdStageRefPtr stage) {
    auto scope = UsdGeomScope::Get(stage, SdfPath("/scope"));
    UsdGeomPrimvarsAPI primvarsAPI(scope);
    int ret = -1;
    primvarsAPI.GetPrimvar(_tokens->counter).Get(&ret);
    return ret;
}

bool insert_to_database(MYSQL* connection, const std::string& table_name, const std::string& path, const std::string& data) {
    std::string tmp;
    tmp.reserve(1024);
    std::stringstream query_delete;
    query_delete << "DELETE FROM `" << table_name << "` WHERE `path`=\"" << path << "\";";
    const auto query_delete_str = query_delete.str();
    auto query_ret = mysql_real_query(connection, query_delete_str.c_str(), query_delete_str.size());
    if (query_ret != 0) {
        std::cerr << "Error executing delete query\n";
    }
    std::stringstream query(tmp);
    query << "INSERT INTO `" << table_name
          << "` (`path`, `data`) VALUES (\"" << path
          << "\", _binary 0x";
    query << std::hex << std::setfill('0') << std::uppercase;
    for (auto c : data) {
        query << std::setw(2) << static_cast<int>(c);
    }
    query << ");";

    const auto query_str = query.str();
    query_ret = mysql_real_query(connection, query_str.c_str(), query_str.size());
    return query_ret == 0;
}

}

int main() {
    my_init();

    const std::string server_name = getenv(HOST_ENV_VAR);
    const auto table_name = get_env_var(server_name, TABLE_ENV_VAR, "usd");
    const auto server_user = get_env_var(server_name, USER_ENV_VAR, "root");
    const auto compacted_default_pass =
        z85::encode_with_padding(std::string("12345678"));
    auto server_password =
        get_env_var(server_name, PASSWORD_ENV_VAR, compacted_default_pass);
    server_password = z85::decode_with_padding(server_password);
    const auto server_db = get_env_var(server_name, DB_ENV_VAR, "test");
    const auto server_port = static_cast<unsigned int>(
        atoi(get_env_var(server_name, PORT_ENV_VAR, "3306").c_str()));

    auto get_connection = [&] () -> MYSQL* {
        MYSQL* ret = mysql_init(nullptr);
        my_bool reconnect = 1;
        mysql_options(ret, MYSQL_OPT_RECONNECT, &reconnect);
        const auto* status = mysql_real_connect(
            ret, server_name.c_str(), server_user.c_str(),
            server_password.c_str(), server_db.c_str(), server_port, nullptr, 0);
        if (status == nullptr) {
            mysql_close(ret);
            return nullptr;
        }
        return ret;
    };

    constexpr size_t num_threads = 6;
    constexpr size_t num_tests = 2048;

    std::vector<MYSQL*> connections;
    connections.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        auto* conn = get_connection();
        if (conn != nullptr) {
            connections.push_back(conn);
        }
    }

    std::atomic<int> counter;
    counter.store(0);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    auto thread_fun = [&] (MYSQL* conn) {

        const auto id = counter.fetch_add(1);
        const auto path = TfStringPrintf("/path%i.usda", id);
        auto c = counter.fetch_add(1);
        if (!insert_to_database(conn, table_name, path, generate_stage(c))) {
            std::cerr << "Error inserting into the database!\n";
            return;
        }
        const auto sqlPath = TfStringPrintf("sql:///path%i.usda", id);
        auto stage = UsdStage::Open(sqlPath);
        for (size_t t = 0; t < num_tests; t += 1) {
            c = counter.fetch_add(1);
            insert_to_database(conn, table_name, path, generate_stage(c));
            stage->Reload();
            const auto c_new = get_counter(stage);
            if (c != c_new) {
                std::cerr << path << " incorrect counter " << c << " vs " << c_new << "\n";
            }
        }
    };
    for (auto* conn : connections) {
        threads.emplace_back(thread_fun, conn);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (auto* conn : connections) {
        mysql_close(conn);
    }

    return 0;
}
