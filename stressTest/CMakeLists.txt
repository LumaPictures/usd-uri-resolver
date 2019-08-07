find_package(Boost REQUIRED COMPONENTS python)
find_package(PythonLibs 2.7 REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(TBB REQUIRED)
find_package(MySQL REQUIRED)

link_directories(${USD_LIBRARY_DIR})

set(SRC main.cxx)

add_executable(stress ${SRC} ${Z85_SRC})
set_target_properties(stress PROPERTIES PREFIX "")
set_target_properties(stress PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)
target_link_libraries(stress PRIVATE
    ${Boost_LIBRARIES}
    ${PYTHON_LIBRARIES}
    ${MYSQL_LIBRARIES})
target_link_libraries(stress PRIVATE arch tf plug vt ar sdf usd usdGeom)
target_include_directories(stress SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(stress SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(stress SYSTEM PRIVATE "${PYTHON_INCLUDE_DIRS}")
target_include_directories(stress SYSTEM PRIVATE "${MYSQL_INCLUDE_DIR}")
target_include_directories(stress SYSTEM PRIVATE "${OPENEXR_INCLUDE_DIRS}")
target_include_directories(stress SYSTEM PRIVATE "${TBB_INCLUDE_DIRS}")
target_include_directories(stress SYSTEM PRIVATE "${EXTERNAL_INCLUDE_DIR}")

install(
    TARGETS stress
    DESTINATION bin)
