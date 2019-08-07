add_subdirectory(obfuscate_pass)

set(PLUGIN_NAME URIResolver)

find_package(Boost REQUIRED COMPONENTS python)
find_package(PythonLibs 2.7 REQUIRED)
find_package(OpenEXR REQUIRED)
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
target_link_libraries(${PLUGIN_NAME} PRIVATE ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})
target_link_libraries(${PLUGIN_NAME} PRIVATE arch tf plug vt ar ${MYSQL_LIBRARIES})
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${PYTHON_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${MYSQL_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${OPENEXR_INCLUDE_DIRS}")
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
