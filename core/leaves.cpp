#include "leaves.h"
#include "types.h"

const RefsSlice empty_slice;

RefsSlice RefsSlice::slice(size_t begin, size_t end) const {
    assert(begin <= end);

    if (!_extent) {
        // special case: only 1 item in _expr.
        if (begin > 0 || end < 1) {
            return empty_slice;
        } else {
            return *this;
        }
    }

    const size_t offset = _begin - _extent->address();
    const size_t n = _extent->size();
    assert(offset <= n);

    end = std::min(end, n - offset);
    begin = std::min(begin, end);
    return RefsSlice(_extent, _begin + begin, _begin + end);
}
