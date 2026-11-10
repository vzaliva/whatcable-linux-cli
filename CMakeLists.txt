cmake_minimum_required(VERSION 3.20)
project(whatcable-linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)

add_subdirectory(src/core)
add_subdirectory(src/cli)

option(BUILD_TESTING "Build the test suite (run with ctest)" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
