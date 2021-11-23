cmake_minimum_required(VERSION 3.1)

project (msemu CXX)

include(FetchContent)

FetchContent_Declare(
  mspkg
  GIT_REPOSITORY https://github.com/matgla/mspkg.git
  GIT_TAG        master
)

FetchContent_MakeAvailable(mspkg)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${mspkg_SOURCE_DIR}/cmake)
include (mspkg)

setup_mspkg(${mspkg_SOURCE_DIR})

find_package(CMakeUtils REQUIRED)

include (sanitizers)
add_sanitizers()
include (coverage) 
add_coverage()

add_library(msemu_private_flags INTERFACE)

target_compile_options(msemu_private_flags INTERFACE
    -Wall -Wpedantic -Werror -Wextra -pedantic-errors -Wconversion -Wsign-conversion
    $<$<CONFIG:DEBUG>:-Og -g>
    $<$<CONFIG:RELEASE>:-O3>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++2a>
)

target_link_libraries(msemu_private_flags INTERFACE enable_sanitizers coverage_flags)

add_subdirectory(source)

if (ENABLE_TESTS)

  add_subdirectory(tests)

endif ()