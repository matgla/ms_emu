cmake_minimum_required(VERSION 3.13)

project (msemu NONE)

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

# find_package(pico-sdk REQUIRED)
# include (cmake/preload/toolchains/pico_arm_gcc)
# include (pico_sdk_init)
# pico_sdk_init()
enable_language(ASM C CXX) 

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

find_package(CMakeUtils REQUIRED)

include (sanitizers)
add_sanitizers()
include (coverage) 
add_coverage()

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(msemu_private_flags INTERFACE)

target_compile_options(msemu_private_flags INTERFACE
    -fdiagnostics-color=always -Wall -Wpedantic -Werror -Wextra -pedantic-errors -Wconversion -Wsign-conversion 
    $<$<CONFIG:DEBUG>:-Og -g -save-temps>
    $<$<CONFIG:RELEASE>:-O3>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++2a>
)

# target_link_libraries(msemu_private_flags INTERFACE enable_sanitizers coverage_flags)

add_subdirectory(source)

if (ENABLE_TESTS)

  add_subdirectory(tests)
  message (STATUS "Tests are enabled")

endif ()