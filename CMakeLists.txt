cmake_minimum_required(VERSION 3.6)
project(cmathics)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include_directories(${cmathics_SOURCE_DIR})

set(HOMEBREWHOME "$ENV{HOMEBREWHOME}")
set(PYTHONHOME "$ENV{PYTHONHOME}")

if(HOMEBREWHOME)
    message(STATUS "using homebrew at ${HOMEBREWHOME}")
    # on OS X, the easiest way to get the dependent libraries is to use homebrew. we might want to use a custom
    # path that can be specified using this env var. your homebrew needs to be set up using: brew install gmp mpfr
    include_directories("${HOMEBREWHOME}/include")
    link_directories("${HOMEBREWHOME}/lib")
elseif(APPLE)
    message(WARNING "please set HOMEBREWHOME to a homebrew installaton with gmp and mpfr.")
endif()

if(PYTHONHOME)
    # PYTHONHOME must point to a python installation with Mathics installed. note that PYTHONHOME should be the
    # same PYTHONHOME that is set (as environment variable) when we run the binary later. mismatches here will
    # produce nasty, seemingly unrelated, errors/hard aborts at startup when Py_Initialize is called.
    include_directories("${PYTHONHOME}/include/python3.5m")
    link_directories("${PYTHONHOME}/lib")
    set(PYTHON_LIBRARIES "python3.5m")
else()
    find_package(PythonLibs REQUIRED)
    include_directories(${PYTHON_INCLUDE_DIRS})
endif()

set(SOURCE_FILES
    parser/tokeniser.cpp
    parser/tokeniser.h
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
    core/leaves.h
    core/primitives.h
    core/builtin.h
    core/symbol.h
    core/matcher.h)

add_custom_target(standalone)

add_executable(cmathics ${SOURCE_FILES} mathics.cpp)
target_link_libraries(cmathics mpfr gmp ${PYTHON_LIBRARIES})

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
    tests/test_string.cpp)

include_directories("$ENV{HOME}/googletest/googletest/include")
link_directories("$ENV{HOME}/googletest/googletest/lib")

add_executable(cmathicstest ${TESTS_SOURCE_FILES})
target_link_libraries(cmathicstest mpfr gmp gtest)

