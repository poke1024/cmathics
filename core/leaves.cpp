#include "leaves.h"
#include "types.h"

const Slice empty_slice = Slice(std::shared_ptr<Extent>(), nullptr, nullptr);

Extent::~Extent() {
    delete[] _leaves;
}

Slice Slice::slice(size_t begin, size_t end) const {
    assert(begin <= end);

    if (!_storage) {
        // special case: only 1 item in _expr.
        if (begin > 0 || end < 1) {
            return empty_slice;
        } else {
            return *this;
        }
    }

    const size_t offset = _begin - _storage->_leaves;
    const size_t n = _storage->_n;
    assert(offset <= n);

    end = std::min(end, n - offset);
    begin = std::min(begin, end);
    return Slice(_storage, _begin + begin, _begin + end);
}