set(PLUGIN_NAME databaseResolver)

find_package(Boost)
find_package(PythonLibs)

include_directories(SYSTEM $ENV{USD_ROOT}/include)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})
link_directories($ENV{USD_ROOT}/lib)

add_library(${PLUGIN_NAME} SHARED resolver.cpp)
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} arch tf plug vt ar)

# does it need to be in the usd subfolder?
install(TARGETS ${PLUGIN_NAME}
        DESTINATION .)

install(FILES plugInfo.json
        DESTINATION ${PLUGIN_NAME}/resources/)