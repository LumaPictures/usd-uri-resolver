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
target_link_libraries(${PLUGIN_NAME} PRIVATE arch tf plug vt ar ${MYSQL_LIB})
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${USD_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${Python_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${MYSQL_INCLUDE_DIR}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${TBB_INCLUDE_DIRS}")
target_include_directories(${PLUGIN_NAME} SYSTEM PRIVATE "${EXTERNAL_INCLUDE_DIR}")

if (MSVC)
    # Make sure WinDef.h doesn't define min and max macros which
    # will conflict with std::min() and std::max()
    target_compile_definitions(${PLUGIN_NAME} PRIVATE NOMINMAX)

    # The /Zc:inline option strips out the "arch_ctor_<name>" symbols used for
    # library initialization by ARCH_CONSTRUCTOR starting in Visual Studio 2019,
    # causing release builds to fail. Disable the option for this and later
    # versions.
    #
    # For more details, see:
    # https://developercommunity.visualstudio.com/content/problem/914943/zcinline-removes-extern-symbols-inside-anonymous-n.html
    if (MSVC_VERSION GREATER_EQUAL 1920)
        target_compile_options(${PLUGIN_NAME} PRIVATE "/Zc:inline-")
    else()
        target_compile_options(${PLUGIN_NAME} PRIVATE "/Zc:inline")
    endif()
endif()

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
