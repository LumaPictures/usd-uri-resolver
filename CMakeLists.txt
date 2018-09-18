project(luma_usd)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(USD REQUIRED)

set (CMAKE_CXX_STANDARD 11)

add_compile_options(-Wall -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")
option(ENABLE_RESOLVER_BUILD "Enabling building the uri resolver." On)

cmake_minimum_required(VERSION 2.8)

if (ENABLE_RESOLVER_BUILD)
    add_subdirectory(URIResolver)
endif ()

add_subdirectory(external)

install(FILES plugInfo.json
        DESTINATION .)
