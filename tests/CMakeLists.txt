find_package(gtest REQUIRED)

add_executable(msemu_tests)

target_sources(msemu_tests 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu8086_for_test.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_base.hpp
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/aaa_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aad_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aam_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/aas_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/adc_tests.cpp


        ${CMAKE_CURRENT_SOURCE_DIR}/test_base.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/add_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jmp_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pop_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mov_tests.cpp
)

target_link_libraries(msemu_tests 
    PRIVATE 
        msemu_cpu8086 
        msemu_private_flags
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

