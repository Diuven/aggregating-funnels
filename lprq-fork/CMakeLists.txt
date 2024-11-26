cmake_policy(SET CMP0048 NEW)
project(ConcurrentQueuesResearch)
CMAKE_MINIMUM_REQUIRED(VERSION 3.13)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER /usr/bin/clang-13)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-13)

option(CAUTIOUS_DEQUEUE "Enable preliminary emptiness check in dequeue in array-based queues" OFF)
option(DISABLE_HP "Disable Hazard Pointers; this causes intentional memory leaks" OFF)
option(USE_LIBCPP "Use libc++ instead of libstdc++" OFF)

if(USE_LIBCPP)
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++ -lc++abi)
endif()

find_package(Threads REQUIRED)
include(Gtest.cmake)
include(CLI11.cmake)

# -- jemalloc
find_package(PkgConfig REQUIRED)
pkg_search_module(JEMALLOC REQUIRED jemalloc)

# jemalloc --
enable_testing()

add_library(scqueue STATIC third-party/scqueue/lfring_cas1.c)
target_include_directories(scqueue PUBLIC third-party/scqueue/)

if(CAUTIOUS_DEQUEUE)
    target_compile_definitions(scqueue PRIVATE CAUTIOUS_DEQUEUE)
endif()

function(make_bench name sources)
    add_executable("bench-${name}" ${sources})

    target_link_libraries("bench-${name}" PRIVATE scqueue Threads::Threads CLI11::CLI11 ${JEMALLOC_LIBRARIES})

    # target_link_libraries("bench-${name}" PRIVATE ccqueue scqueue Threads::Threads CLI11::CLI11 ${JEMALLOC_LIBRARIES})
    target_include_directories("bench-${name}" PRIVATE include/ ${JEMALLOC_INCLUDE_DIRS})
    target_compile_options("bench-${name}" PRIVATE -Wall -Wextra)

    if(CAUTIOUS_DEQUEUE)
        target_compile_definitions("bench-${name}" PRIVATE CAUTIOUS_DEQUEUE)
    endif()

    if(DISABLE_HP)
        target_compile_definitions("bench-${name}" PRIVATE DISABLE_HP)
    endif()
endfunction()

make_bench(enq-deq "src/pairs-benchmark.cpp;src/AdditionalWork.cpp;src/Forked.cpp")
make_bench(prod-cons "src/pc-benchmark.cpp;src/AdditionalWork.cpp;src/Forked.cpp")

add_executable(tests src/test.cpp)
target_link_libraries(tests PRIVATE scqueue Threads::Threads gtest_main)

# target_link_libraries(tests PRIVATE ccqueue scqueue Threads::Threads gtest_main)
target_include_directories(tests PRIVATE include/)
target_compile_options(tests PRIVATE -Wall -Wextra -fsanitize=address)
target_link_options(tests PRIVATE -fsanitize=address) # DISABLE_HP does not affect tests

if(CAUTIOUS_DEQUEUE)
    target_compile_definitions(tests PRIVATE CAUTIOUS_DEQUEUE)
endif()

add_test(NAME tests COMMAND tests)
