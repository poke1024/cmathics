cmake_minimum_required(VERSION 3.6)
project(cmathics)

set(CMAKE_CXX_STANDARD 14)

if("$ENV{OPTIMIZE}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    add_definitions(-DNDEBUG=1)
endif()
add_definitions(-DCOMPILE_TO_SLICE=1)

include_directories(${cmathics_SOURCE_DIR})

set(CUSTOM_PREFIX "$ENV{HOMEBREWHOME}")
set(PYTHONHOME "$ENV{PYTHONHOME}")

if(CUSTOM_PREFIX)
    message(STATUS "using prefix ${CUSTOM_PREFIX}")

    # on OS X, the easiest way to get the dependent libraries is to use homebrew. we might want to use a custom
    # path that can be specified using CUSTOM_PREFIX. your homebrew needs to be set up using:

    # brew install gmp mpfr icu4c

    # you also need symengine installed; the instructions given at the official page at
    # https://github.com/symengine/symengine/wiki/Building-SymEngine don't seem to work that
    # well under OS X and homebrew. here's what works for me (needs "brew install cmake" and
    # "brew install homebrew/science/arb"):

    # NOTE: you need a thread-safe symengine, i.e. one built with WITH_SYMENGINE_THREAD_SAFE.

    # export SYMENGINE_PREFIX="$HOME/homebrew"
    # export SYMENGINE_SOURCE="$HOME/Downloads/symengine-master"
    # export CPATH="$SYMENGINE_PREFIX/include:$SYMENGINE_SOURCE"
    # cd "$SYMENGINE_SOURCE"
    # CXXFLAGS=-std=c++11 "$SYMENGINE_PREFIX/bin/cmake" -DCMAKE_INSTALL_PREFIX:PATH="$SYMENGINE_PREFIX" -DWITH_ARB:BOOL=ON -DWITH_SYMENGINE_THREAD_SAFE:BOOL=ON .
    # make CXX_DEFINES=-std=c++11
    # make install

    # it might also be useful to use -DCMAKE_BUILD_TYPE:STRING="Debug"

    include_directories("${CUSTOM_PREFIX}/include" "${CUSTOM_PREFIX}/include/flint" "${CUSTOM_PREFIX}/opt/icu4c/include")
    link_directories("${CUSTOM_PREFIX}/lib" "${CUSTOM_PREFIX}/opt/icu4c/lib")
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
    core/definitions.cpp
    core/definitions.h
    core/evaluation.cpp
    core/evaluation.h
        core/expression/implementation.cpp
        core/expression/implementation.h
        core/hash.cpp
    core/hash.h
        core/atoms/integer.cpp
        core/atoms/integer.h
    core/misc.cpp
    core/misc.h
        core/pattern/options.cpp
        core/pattern/arguments.h
        core/atoms/rational.cpp
        core/atoms/rational.h
        core/atoms/real.cpp
        core/atoms/real.h
        core/atoms/string.cpp
        core/atoms/string.h
    core/types.h
    core/types.cpp
        core/builtin.h
        core/atoms/symbol.h
        core/pattern/matcher.h
    core/heap.cpp
    core/heap.h
    core/evaluate.h
    core/evaluate.cpp
    core/cache.h
    core/sort.h
    core/sort.cpp
    core/system_symbols.h
    core/rule.h
        core/atoms/numeric.h
    core/runtime.h
    core/python.h
    core/parser.h
        core/atoms/symbol.cpp
    builtin/arithmetic.cpp
    builtin/arithmetic.h
    builtin/lists.cpp
    builtin/lists.h
    builtin/control.cpp
    builtin/control.h
    builtin/assignment.cpp
    builtin/assignment.h
    builtin/functional.cpp
    builtin/functional.h
    builtin/exptrig.cpp
    builtin/exptrig.h
    core/runtime.cpp
    core/rule.cpp
        core/pattern/matcher.cpp
    builtin/strings.cpp
    builtin/strings.h
    builtin/numbertheory.cpp
    builtin/numbertheory.h
    builtin/structure.cpp
    builtin/structure.h
        core/atoms/complex.cpp
        core/atoms/complex.h
    concurrent/parallel.h
    concurrent/parallel.cpp
        core/slice/generator.h
    builtin/inout.cpp
    builtin/inout.h
        core/slice/generator.h
    arithmetic/add.cpp
    arithmetic/add.h
    arithmetic/mul.cpp
    arithmetic/mul.h
    builtin/comparison.cpp
    builtin/comparison.h
    arithmetic/binary.h
    builtin/evaluation.cpp
    builtin/evaluation.h
    core/numberform.h
    core/numberform.cpp
    builtin/options.cpp
    builtin/options.h
    builtin/patterns.h
    builtin/patterns.cpp
    core/atoms/numeric.tcc core/slice/collection.h core/slice/slice.h core/slice/packed.h core/slice/big.h core/slice/tiny.h core/attributes.h core/slice/method.h core/slice/array.h core/slice/vcall.h core/expression/interface.h core/slice/code.h core/atoms/precision.cpp core/pattern/arguments.tcc builtin/attributes.cpp builtin/attributes.h builtin/numeric.cpp builtin/numeric.h)

add_custom_target(standalone)

add_executable(cmathics ${SOURCE_FILES} mathics.cpp)
target_link_libraries(cmathics mpfr gmp arb flint symengine icucore icuuc ${PYTHON_LIBRARIES})

add_custom_target(tests)

set(TESTS_SOURCE_FILES ${SOURCE_FILES}
    tests/test_arithmetic.cpp
    tests/test_definitions.cpp
    tests/test_expression.cpp
    tests/test_integer.cpp
    tests/test_rational.cpp
    tests/test_real.cpp
    tests/test_string.cpp
    tests/test.cpp
    tests/test_pool.cpp
    tests/test_parallel.cpp
    tests/test_matcher.cpp)

add_executable(cmathicstest ${TESTS_SOURCE_FILES})
target_compile_definitions(cmathicstest PRIVATE MAKE_UNIT_TEST=1)
target_link_libraries(cmathicstest mpfr gmp arb flint symengine icucore icuuc ${PYTHON_LIBRARIES})

