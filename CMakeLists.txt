project(luma_usd)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

add_definitions(-std=c++11 -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")

cmake_minimum_required(VERSION 2.8)

add_subdirectory(databaseResolver)

install(FILES plugInfo.json
        DESTINATION ./)