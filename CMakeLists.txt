cmake_minimum_required(VERSION 3.6)
project(cmathics)

# specifying -O2 here gives another factor 10 currently
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include_directories(${cmathics_SOURCE_DIR})

set(CUSTOM_PREFIX "$ENV{HOMEBREWHOME}")
set(PYTHONHOME "$ENV{PYTHONHOME}")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED OFF)
set(Boost_USE_STATIC_RUNTIME OFF)

set(Boost_LIBRARIES "boost_system")

#find_package(Boost 1.60 COMPONENTS pool python)
#include_directories(${Boost_INCLUDE_DIRS})

if(CUSTOM_PREFIX)
    message(STATUS "using prefix ${CUSTOM_PREFIX}")

    # on OS X, the easiest way to get the dependent libraries is to use homebrew. we might want to use a custom
    # path that can be specified using CUSTOM_PREFIX. your homebrew needs to be set up using:

    # brew install gmp mpfr boost boost-python

    # you also need symengine installed; the instructions given at the official page at
    # https://github.com/symengine/symengine/wiki/Building-SymEngine don't seem to work that
    # well under OS X and homebrew. here's what works for me (needs "brew install cmake" and
    # "brew install homebrew/science/arb"):

    # export SYMENGINE_PREFIX="$HOME/homebrew"
    # export SYMENGINE_SOURCE="$HOME/Downloads/symengine-master"
    # export CPATH="$SYMENGINE_PREFIX/include:$SYMENGINE_SOURCE"
    # cd "$SYMENGINE_SOURCE"
    # CXXFLAGS=-std=c++11 "$SYMENGINE_PREFIX/bin/cmake" -DCMAKE_INSTALL_PREFIX:PATH="$SYMENGINE_PREFIX" -DWITH_ARB:BOOL=ON .
    # make CXX_DEFINES=-std=c++11
    # make install

    include_directories("${CUSTOM_PREFIX}/include" "${CUSTOM_PREFIX}/include/flint")
    link_directories("${CUSTOM_PREFIX}/lib")
elseif(APPLE)
    message(WARNING "please set CUSTOM_PREFIX to a homebrew installaton with gmp, arb and symengine.")
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
    core/matcher.h
    core/operations.h
    core/arithmetic_implementation.h
    core/heap.cpp
    core/heap.h
    core/structure.h
    core/structure_implementation.h
    core/evaluate.h
    core/evaluate.cpp
    core/cache.h
    core/sort.h
    core/sort.cpp
    core/system_symbols.h
    core/rule.h
    core/numeric.h
    core/runtime.h
    core/python.h
    core/parser.h

    builtin/arithmetic.cpp
    builtin/arithmetic.h
    builtin/lists.cpp
    builtin/lists.h
    builtin/control.cpp
    builtin/control.h builtin/assignment.cpp builtin/assignment.h builtin/functional.cpp builtin/functional.h)

add_custom_target(standalone)

add_executable(cmathics ${SOURCE_FILES} mathics.cpp)
target_link_libraries(cmathics mpfr gmp arb flint symengine ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})

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

