add_executable(msemu)

add_library(msemu_cpu8086 STATIC)

target_sources(msemu_cpu8086 
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_cpu.hpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/memory.hpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/core_dump.hpp
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/core_dump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_registers.cpp
)

target_link_libraries(msemu_cpu8086 PRIVATE msemu_private_flags)

target_sources(msemu
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(msemu 
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/source
)

target_include_directories(msemu_cpu8086 
    INTERFACE 
        ${PROJECT_SOURCE_DIR}/source
)

target_link_libraries(msemu 
    PRIVATE 
        msemu_cpu8086
        msemu_private_flags

        # pico_stdlib
)

if (DUMP_CORE_STATE)
    target_compile_definitions(msemu PRIVATE -DDUMP_CORE_STATE)
endif ()

# pico_add_extra_outputs(msemu)