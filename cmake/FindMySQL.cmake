# - Try to find MySQL.
# Once done this will define:
# MYSQL_FOUND          - If false, do not try to use MySQL.
# MYSQL_INCLUDE_DIR    - Where to find mysql.h, etc.
# MYSQL_LIBRARIES      - The libraries to link against.
# MYSQL_VERSION_STRING - Version in a string of MySQL.
#
# Created by RenatoUtsch based on eAthena implementation.
#
# Please note that this module only supports Windows and Linux officially, but
# should work on all UNIX-like operational systems too.
#

#=============================================================================
# Copyright 2012 RenatoUtsch
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# Define search paths based on user input and environment variables
set(MYSQL_SEARCH_DIR ${MYSQL_ROOT_DIR} $ENV{MYSQL_INSTALL_DIR} $ENV{MYSQL_ROOT})

option(MYSQL_USE_STATIC_LIB "Set to ON to force the use of the static library. Default is OFF")

if( WIN32 )
  set(MYENV "PROGRAMFILES(X86)")
  find_path( MYSQL_INCLUDE_DIR
    NAMES "mysql.h"
    PATHS "$ENV{PROGRAMFILES}/MySQL/*/include"
        "$ENV{${MYENV}}/MySQL/*/include"
        "$ENV{SYSTEMDRIVE}/MySQL/*/include" )

  find_library( MYSQL_LIBRARY
    NAMES "mysqlclient" "mysqlclient_r" "libmysql"
    PATHS "$ENV{PROGRAMFILES}/MySQL/*/lib"
        "$ENV{${MYENV}}/MySQL/*/lib"
        "$ENV{SYSTEMDRIVE}/MySQL/*/lib" )
else()
  find_path( MYSQL_INCLUDE_DIR
    NAMES "mysql.h"
    HINTS ${MYSQL_INCLUDE_DIR} ${MYSQL_SEARCH_DIR}/include
    PATHS "/usr/include/mysql"
        "/usr/local/include/mysql"
        "/usr/mysql/include/mysql" )

  if(MYSQL_USE_STATIC_LIB)
    find_library( MYSQL_LIBRARY
      NAME "libmysqlclient.a"
      HINTS ${MYSQL_LIBRARY_DIR} ${MYSQL_SEARCH_DIR}/lib )
  else()
    find_library( MYSQL_LIBRARY
      NAMES "mysqlclient" "mysqlclient_r"
      HINTS ${MYSQL_LIBRARY_DIR} ${MYSQL_SEARCH_DIR}/lib
      PATHS
          "/lib/mysql"
          "/lib64/mysql"
          "/usr/lib/mysql"
          "/usr/lib64/mysql"
          "/usr/local/lib/mysql"
          "/usr/local/lib64/mysql"
          "/usr/mysql/lib/mysql"
          "/usr/mysql/lib64/mysql" )
  endif()
endif()


if (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  set(MYSQL_FOUND true)
  set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
else (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  set(MYSQL_FOUND FALSE)
  set(MYSQL_LIBRARIES)
endif (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

mark_as_advanced(
  MYSQL_LIBRARY
  MYSQL_INCLUDE_DIR
)

if(MYSQL_INCLUDE_DIR AND EXISTS "${MYSQL_INCLUDE_DIR}/mysql_version.h")
    file(STRINGS
        "${MYSQL_INCLUDE_DIR}/mysql_version.h"
        _mysql_tmp
        REGEX "#define LIBMYSQL_VERSION .*$")
    string(REGEX MATCHALL "([0-9]+)\\.([0-9]+)\\.([0-9]+)" MYSQL_VERSION ${_mysql_tmp})
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MySQL
                                  REQUIRED_VARS MYSQL_INCLUDE_DIR MYSQL_LIBRARIES
                                  HANDLE_COMPONENTS
                                  VERSION_VAR MYSQL_VERSION)
