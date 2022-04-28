cmake_minimum_required(VERSION 3.2)

message(STATUS "CMakeLists: Zep")

# Global Options
option(BUILD_TESTS "Make the tests" ON)

# Global Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_FLAGS "-lGLEW")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-reldbg")

project(Zep
    LANGUAGES CXX C
    VERSION 0.5.0
    )

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# config_app.h checks
# This makes a config_shared.h file which can be included for system settings
#  Must be after setting up the platforms
set(ZEP_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/m3rdparty)

include(GNUInstallDirs)

# Set all compiler flags 
include(cmake/all.cmake)

enable_testing()

# The main library
add_subdirectory(src)
add_subdirectory(tests)

# Make the CMake bits that ensure find_package does the right thing
install(EXPORT zep-targets
    FILE zep-targets.cmake
    NAMESPACE Zep::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
    )

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${ZEP_ROOT}/cmake/zep-config.cmake.in
    ${CMAKE_BINARY_DIR}/cmake/zep-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cmake/zep-config-version.cmake
    VERSION 1.0
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
    ${CMAKE_BINARY_DIR}/cmake/zep-config.cmake
    ${CMAKE_BINARY_DIR}/cmake/zep-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
)
