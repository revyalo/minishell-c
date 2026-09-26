cmake_minimum_required(VERSION 3.16)
project(minishell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(minishell_parser parser.c)
target_include_directories(minishell_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(minishell_parser PUBLIC _POSIX_C_SOURCE=200809L)

add_executable(mymsh mymsh.c executor.c)
target_link_libraries(mymsh PRIVATE minishell_parser)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(minishell_parser PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(mymsh PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_parser tests/test_parser.c)
    target_link_libraries(test_parser PRIVATE minishell_parser)
    add_test(NAME parser COMMAND test_parser)
    add_test(NAME shell_integration COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shell.sh $<TARGET_FILE:mymsh>)
endif()
