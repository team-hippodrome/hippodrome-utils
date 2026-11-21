cmake_minimum_required(VERSION 3.16)
project(hippodrome_utils VERSION 0.1.0 LANGUAGES C)

# Plain C11, no dependencies: the ports add this directory with add_subdirectory() and link hippodrome::utils.
add_library(hippodrome_utils STATIC
    src/stats.c
    src/speed_estimate.c
    src/trainer_memory.c
    src/summary_card.c
    src/file_io.c
    src/window_anim.c
    src/hue_choice.c
    src/move_description.c
    src/order_queue.c
)
add_library(hippodrome::utils ALIAS hippodrome_utils)
target_include_directories(hippodrome_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(hippodrome_utils PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hippodrome_utils PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Unit tests: on by default only when this repository is built on its own.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_hu_tests_default ON)
else()
    set(_hu_tests_default OFF)
endif()
option(HIPPODROME_UTILS_TESTS "Build the hippodrome-utils unit tests" ${_hu_tests_default})
if(HIPPODROME_UTILS_TESTS)
    enable_testing()
    foreach(t stats speed_estimate trainer_memory summary_card window_anim hue_choice move_description order_queue)
        add_executable(test_${t} tests/test_${t}.c)
        target_link_libraries(test_${t} PRIVATE hippodrome_utils)
        set_target_properties(test_${t} PROPERTIES C_STANDARD 11)
        add_test(NAME ${t} COMMAND test_${t} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
endif()
