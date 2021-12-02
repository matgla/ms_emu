find_package(gtest REQUIRED)

add_executable(msemu_tests)

target_sources(msemu_tests 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu8086_for_test.hpp
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/mov_tests.cpp
)

target_link_libraries(msemu_tests 
    PRIVATE 
        msemu_cpu8086 
        gtest_main 
        gmock
        gtest
)

target_compile_options(msemu_tests
    PRIVATE
    -Wno-missing-field-initializers
)

add_custom_target(ut
    COMMAND 
        $<TARGET_FILE:msemu_tests> --gtest_color=yes
    DEPENDS 
        msemu_tests
)

