cmake_minimum_required(VERSION 3.5)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(clog C)
endif()

# C89 strict mode
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Select platform implementation
if(CMAKE_SYSTEM_NAME MATCHES "Retro68|RetroPPC")
    set(CLOG_PLATFORM_SRC src/clog_mac.c)
else()
    set(CLOG_PLATFORM_SRC src/clog_posix.c)
endif()

# Static library
if(NOT TARGET clog)
    add_library(clog STATIC ${CLOG_PLATFORM_SRC})
    target_include_directories(clog PUBLIC include)
endif()

# Test executable (POSIX only)
option(CLOG_BUILD_TESTS "Build clog test applications" ON)
if(CLOG_BUILD_TESTS AND NOT CMAKE_SYSTEM_NAME MATCHES "Retro68|RetroPPC")
    enable_testing()
    add_executable(test_clog tests/test_clog.c)
    target_link_libraries(test_clog clog)
    add_test(NAME test_clog COMMAND test_clog)
endif()
