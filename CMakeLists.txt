cmake_minimum_required(VERSION 3.6)
project(cmathics)

# find_package(Boost REQUIRED)

# include_directories(${Boost_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include_directories(${cmathics_SOURCE_DIR})

if(TRUE)
    include_directories("$ENV{HOME}/homebrew/include")
    link_directories("$ENV{HOME}/homebrew/lib")

    include_directories("$ENV{HOME}/Projekte/j5/include/python3.5m")
    link_directories("$ENV{HOME}/Projekte/j5/lib")
endif()

set(SOURCE_FILES
    core/arithmetic.cpp
    core/arithmetic.h
    core/definitions.cpp
    core/definitions.h
    core/evaluation.cpp
    core/evaluation.h
    core/expression.cpp
    core/expression.h
    core/formatter.cpp
    core/formatter.h
    core/hash.cpp
    core/hash.h
    core/integer.cpp
    core/integer.h
    core/misc.cpp
    core/misc.h
    core/pattern.cpp
    core/pattern.h
    core/rational.cpp
    core/rational.h
    core/real.cpp
    core/real.h
    core/string.cpp
    core/string.h
    core/types.h
    core/types.cpp
    core/leaves.cpp
    core/leaves.h)

add_custom_target(standalone)

add_executable(cmathics ${SOURCE_FILES} mathics.cpp)
target_link_libraries(cmathics mpfr gmp python3.5m)

add_custom_target(tests)

set(TESTS_SOURCE_FILES ${SOURCE_FILES}
    tests/test_all.cpp
    tests/test_arithmetic.cpp
    tests/test_datastructures.cpp
    tests/test_definitions.cpp
    tests/test_expression.cpp
    tests/test_integer.cpp
    tests/test_rational.cpp
    tests/test_real.cpp
    tests/test_string.cpp parser/tokeniser.cpp parser/tokeniser.h)

include_directories("$ENV{HOME}/googletest/googletest/include")
link_directories("$ENV{HOME}/googletest/googletest/lib")

add_executable(cmathicstest ${TESTS_SOURCE_FILES})
target_link_libraries(cmathicstest mpfr gmp gtest)

