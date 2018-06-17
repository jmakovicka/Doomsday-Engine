# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

if (POLICY CMP0068)
    cmake_policy (SET CMP0068 OLD)  # macOS: RPATH affects install_name
endif ()
if (POLICY CMP0058)
    cmake_policy (SET CMP0058 OLD)  # Resource file generation / dependencies.
endif ()

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

macro (set_path var path)
    get_filename_component (${var} "${path}" REALPATH)
endmacro (set_path)

# Project Options & Paths ----------------------------------------------------

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message (FATAL_ERROR "In-source builds are not allowed. Please create a build directory and run CMake from there.")
endif ()

set_path (DE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set (DE_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
if (APPLE OR WIN32)
    set_path (DE_DISTRIB_DIR "${DE_SOURCE_DIR}/../products")
else ()
    set_path (DE_DISTRIB_DIR /usr)
endif ()
set (DE_EXTERNAL_SOURCE_DIR "${DE_SOURCE_DIR}/external")
set (DE_API_DIR "${DE_SOURCE_DIR}/apps/api")
set (CMAKE_MODULE_PATH "${DE_CMAKE_DIR}")
set (DE_SDK_DIR "" CACHE PATH "Location of the Doomsday SDK to use for compiling")

if (CMAKE_CONFIGURATION_TYPES)
    set (DE_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES})
else ()
    set (DE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
endif ()

if (UNIX AND NOT APPLE)
    set (UNIX_LINUX YES)
    include (GNUInstallDirs)
endif()

include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

# Install directories.
set (DE_INSTALL_DATA_DIR "share/doomsday")
set (DE_INSTALL_DOC_DIR "share/doc")
set (DE_INSTALL_MAN_DIR "share/man/man6")
if (DEFINED CMAKE_INSTALL_LIBDIR)
    set (DE_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
else ()
    set (DE_INSTALL_LIB_DIR "lib")
endif ()
set (DE_INSTALL_CMAKE_DIR "${DE_INSTALL_LIB_DIR}/cmake")
set (DE_INSTALL_PLUGIN_DIR "${DE_INSTALL_LIB_DIR}/doomsday")

set (DE_BUILD_STAGING_DIR "${CMAKE_BINARY_DIR}/bundle-staging")
set (DE_VS_STAGING_DIR "${CMAKE_BINARY_DIR}/products") # for Visual Studio

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Prefix path is used for finding CMake config packages.
if (NOT DE_SDK_DIR STREQUAL "")
    list (APPEND CMAKE_PREFIX_PATH "${DE_SDK_DIR}/${DE_INSTALL_LIB_DIR}")
endif ()

find_package (CPlus REQUIRED)

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC OFF)
set (CMAKE_AUTORCC OFF)
set_property (GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Generated)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# This will ensure automoc works in all projects.
if (QT_MODULE STREQUAL Qt5)
    find_package (Qt5 COMPONENTS Core Network)
else ()
    find_package (Qt4 REQUIRED)
endif ()

# Check for mobile platforms.
if (NOT QMAKE_XSPEC_CHECKED)
    qmake_query (xspec "QMAKE_XSPEC")
    set (QMAKE_XSPEC ${xspec} CACHE STRING "Value of QMAKE_XSPEC")
    set (QMAKE_XSPEC_CHECKED YES CACHE BOOL "QMAKE_XSPEC has been checked")
    mark_as_advanced (QMAKE_XSPEC)
    mark_as_advanced (QMAKE_XSPEC_CHECKED)
    if (QMAKE_XSPEC STREQUAL "macx-ios-clang")
        set (IOS YES CACHE BOOL "Building for iOS platform")
    endif ()
endif ()

# Platform-Specific Configuration --------------------------------------------

if (IOS)
    include (PlatformiOS)
elseif (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

set (Python_ADDITIONAL_VERSIONS 2.7)
find_package (PythonInterp REQUIRED)

if (DE_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
