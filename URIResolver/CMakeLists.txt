include_directories(SYSTEM ../external)
file(GLOB Z85_SRC ../external/z85/*.c*)

add_subdirectory(obfuscate_pass)

set(PLUGIN_NAME URIResolver)

find_package(Boost REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(TBB REQUIRED)

link_directories(${USD_LIBRARY_DIR})

find_library(MYSQL_CONNECTOR_LIBRARY
             name libmysqlclient.a
             HINTS ${MYSQL_CONNECTOR_ROOT}/lib)

file(GLOB SRC *.cpp)

message(STATUS ${Z85_SRC})

add_library(${PLUGIN_NAME} SHARED ${Z85_SRC} ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} arch tf plug vt ar ${MYSQL_CONNECTOR_LIBRARY})
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${PYTHON_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${MYSQL_CONNECTOR_ROOT}/include")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${OPENEXR_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${TBB_INCLUDE_DIRS}")

install(TARGETS ${PLUGIN_NAME}
        DESTINATION .)

install(FILES plugInfo.json
        DESTINATION ${PLUGIN_NAME}/resources/)

install(FILES README.md
        DESTINATION docs)
