cmake_minimum_required(VERSION 3.13)

include(FetchContent)

if(DATA_STREAM_TEST)
set(CMAKE_C_COMPILER gcc)
endif()

project(data_stream C ASM)

add_library(data_stream INTERFACE)

target_sources(data_stream INTERFACE
	src/data_stream.c
)

target_include_directories(data_stream INTERFACE
	src
)

# Option to build standalone executable for testing
option(DATA_STREAM_TEST "Build standalone executable for data stream" OFF)

if(DATA_STREAM_TEST)


    FetchContent_Declare(
    c_buffer
    GIT_REPOSITORY https://github.com/Helienzo/c_buffer.git
    GIT_TAG        main
    )
    FetchContent_MakeAvailable(c_buffer)

    # Add standalone executable for testing data stream
    add_executable(test_data_stream test/test_data_stream.c)

    # Link the data_stream library to the standalone executable
    target_link_libraries(test_data_stream PRIVATE c_buffer data_stream)

    # Optionally, add any specific compiler options for testing
    target_compile_options(test_data_stream PRIVATE -Wall -Wextra -pedantic)
endif()