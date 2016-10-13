project(luma_usd)

add_definitions(-std=c++11 -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")

cmake_minimum_required(VERSION 2.8)

add_subdirectory(usdDatabase)
add_subdirectory(databaseResolver)

install(FILES plugInfo.json
        DESTINATION ./)