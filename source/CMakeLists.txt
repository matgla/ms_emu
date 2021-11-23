add_executable(msemu)

add_library(msemu_cpu8086)

target_sources(msemu_cpu8086 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_cpu.hpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/memory.hpp 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_cpu.cpp
)

target_link_libraries(msemu_cpu8086
    PUBLIC 
        msemu_private_flags
)

target_sources(msemu
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(msemu 
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/source
)

target_include_directories(msemu_cpu8086 
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/source
)

target_link_libraries(msemu 
    PRIVATE 
        msemu_private_flags
        msemu_cpu8086
)

if (PRINT_CORE_STATUS)
    target_compile_definitions(msemu PRIVATE -DDUMP_CORE_STATE)
endif ()