add_executable(msemu)

target_sources(msemu
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_cpu.hpp 
        ${CMAKE_CURRENT_SOURCE_DIR}/memory.hpp 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/8086_cpu.cpp
)

target_include_directories(msemu 
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/source
)

target_link_libraries(msemu 
    PRIVATE 
        msemu_private_flags
)

if (PRINT_CORE_STATUS)
    target_compile_definitions(msemu PRIVATE -DDUMP_CORE_STATE)
endif ()