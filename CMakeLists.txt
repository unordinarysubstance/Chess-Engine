cmake_minimum_required(VERSION 3.16)

project(HighPerformanceChessEngine VERSION 1.0.0 LANGUAGES CXX)

option(CHESS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(chess_core
    src/board.cpp
    src/engine.cpp
    src/evaluator.cpp
    src/move.cpp
    src/move_generator.cpp
    src/perft.cpp
    src/transposition_table.cpp
    src/uci.cpp
    src/zobrist.cpp
)

target_include_directories(chess_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(chess_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(chess_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(chess_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(CHESS_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(chess_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(chess_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(chess_engine src/main.cpp)
target_link_libraries(chess_engine PRIVATE chess_core)

add_executable(chess_tests tests/tests.cpp)
target_link_libraries(chess_tests PRIVATE chess_core)

add_executable(chess_bench tools/benchmark.cpp)
target_link_libraries(chess_bench PRIVATE chess_core)

enable_testing()
add_test(NAME chess_tests COMMAND chess_tests)

