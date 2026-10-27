cmake_minimum_required(VERSION 3.25)

project(
    lobsim
    VERSION 0.1.0
    DESCRIPTION "Deterministic limit order book simulator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOBSIM_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

add_library(lobsim_core
    src/book/book.cpp
)

target_include_directories(lobsim_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(lobsim_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
)

if(LOBSIM_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(lobsim_core PUBLIC
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(lobsim_core PUBLIC
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "Requested sanitizers are unsupported by this compiler")
    endif()
endif()

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()