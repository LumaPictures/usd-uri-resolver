add_subdirectory(obfuscate_pass)

set(PLUGIN_NAME URIResolver)

if (USE_PYTHON_3)
    find_package(Python 3.0 COMPONENTS Development REQUIRED)
else()
    find_package(Python 2.7 COMPONENTS Development REQUIRED)
endif()

find_package(Boost COMPONENTS python)
if (NOT Boost_FOUND)
    set(python_version_nodot "${Python_VERSION_MAJOR}${Python_VERSION_MINOR}")
    find_package(Boost COMPONENTS python${python_version_nodot} REQUIRED)
endif()
find_package(TBB REQUIRED)
find_package(MySQL REQUIRED)

link_directories(${USD_LIBRARY_DIR})

set(SRC
    debug_codes.cpp
    memory_asset.cpp
    resolver.cpp
    sql.cpp)

add_library(${PLUGIN_NAME} SHARED ${Z85_SRC} ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
set_target_properties(${PLUGIN_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)
target_link_libraries(${PLUGIN_NAME} PRIVATE ${Boost_LIBRARIES} ${Python_LIBRARIES})
target_link_libraries(${PLUGIN_NAME} PRIVATE ${TBB_LIBRARIES})
target_link_libraries(${PLUGIN_NAME} PRIVATE arch tf plug vt ar ${MYSQL_LIBRARIES})
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Python_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${MYSQL_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${TBB_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${EXTERNAL_INCLUDE_DIR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json")

install(
    TARGETS ${PLUGIN_NAME}
    DESTINATION .)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json"
    DESTINATION ${PLUGIN_NAME}/resources/)

install(
    FILES README.md
    DESTINATION docs)
