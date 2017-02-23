#pragma once

#include <static_if/static_if.hpp>

#include "array.h"
#include "vcall.h"

enum SliceMethodOptimizeTarget {
    DoNotCompileToSliceType,
#if COMPILE_TO_SLICE
    CompileToSliceType,
    CompileToPackedSliceType
#else
    CompileToSliceType = DoNotCompileToSliceType,
    CompileToPackedSliceType = DoNotCompileToSliceType
#endif
};

template<SliceMethodOptimizeTarget Optimize, typename R, typename F>
class SliceMethod {
};
