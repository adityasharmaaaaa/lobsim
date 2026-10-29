include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(Catch2)

add_executable(lobsim_tests
    unit/book_test.cpp
    unit/lesson1_worked_example_test.cpp
)

target_link_libraries(lobsim_tests
    PRIVATE
        lobsim_core
        Catch2::Catch2WithMain
)

target_compile_options(lobsim_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
)

include(Catch)
catch_discover_tests(lobsim_tests)