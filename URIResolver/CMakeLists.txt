add_subdirectory(obfuscate_pass)

set(PLUGIN_NAME URIResolver)

find_package(Boost REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(TBB REQUIRED)
set(MYSQL_USE_STATIC_LIB ON)
find_package(MySQL REQUIRED)

link_directories(${USD_LIBRARY_DIR})

set(SRC
    debugCodes.cpp
    resolver.cpp
    sql.cpp)

message(STATUS ${Z85_SRC})

add_library(${PLUGIN_NAME} SHARED ${Z85_SRC} ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
set_target_properties(${PLUGIN_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)
target_link_libraries(${PLUGIN_NAME} arch tf plug vt ar ${MYSQL_LIBRARIES})
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${PYTHON_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${MYSQL_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${OPENEXR_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${TBB_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${EXTERNAL_INCLUDE_DIR}")

install(
    TARGETS ${PLUGIN_NAME}
    DESTINATION .)

install(
    FILES plugInfo.json
    DESTINATION ${PLUGIN_NAME}/resources/)

install(
    FILES README.md
    DESTINATION docs)
