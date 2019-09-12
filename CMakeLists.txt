project(usd-uri-resolver)

cmake_minimum_required(VERSION 3.1)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")

find_package(USD REQUIRED)

set(CMAKE_CXX_STANDARD 11)

if (LINUX)
    set(DSO_EXT "so")
elseif (APPLE)
    set(DSO_EXT "dylib")
else ()
    set(DSO_EXT "dll")
endif ()

# TODO: Windows support.
add_compile_options(-Wall -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")
option(ENABLE_RESOLVER_BUILD "Enabling building the uri resolver." On)
option(ENABLE_STRESSTEST_BUILD "Enabling building stress test for the resolver." Off)

set(EXTERNAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

set(Z85_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/z85/z85_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/z85/z85.c)

if (ENABLE_RESOLVER_BUILD)
    add_subdirectory(URIResolver)
endif ()

if (ENABLE_STRESSTEST_BUILD)
    add_subdirectory(stressTest)
endif ()

add_subdirectory(external)

install(
    FILES plugInfo.json
    DESTINATION .)
