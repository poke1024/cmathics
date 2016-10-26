#include "leaves.h"
#include "types.h"

const Slice empty_slice = Slice(std::shared_ptr<Extent>(), nullptr, nullptr);

Extent::~Extent() {
    delete[] _leaves;
}

Slice Slice::apply(
    size_t begin,
    size_t end,
    const std::function<BaseExpressionRef(const BaseExpressionRef&)> &f) const {

    assert(_expr == nullptr);
    if (_storage.use_count() == 1) {
        // FIXME. optimize this case. we do not need to copy here.
    }

    auto leaves = _storage->_leaves;
    for (size_t i = begin; i < end; i++) {
        auto new_leaf = f(leaves[i]);

        if (new_leaf) { // copy is needed now
            std::vector<BaseExpressionRef> new_leaves;
            new_leaves.reserve(_end - _begin);

            try {
                new_leaves.insert(new_leaves.end(), &leaves[0], &leaves[i]);

                new_leaves.push_back(new_leaf); // leaves[i]

                for (size_t j = i + 1; j < end; j++) {
                    auto old_leaf = new_leaves[j];
                    new_leaf = f(old_leaf);
                    if (new_leaf) {
                        new_leaves.push_back(new_leaf);
                    } else {
                        new_leaves.push_back(old_leaf);
                    }
                }

                new_leaves.insert(new_leaves.end(), &leaves[end], &leaves[_end - _begin]);
            } catch(...) {
                // FIXME delete new Expressions in new_leaves
            }

            ;
            return Slice(std::make_shared<Extent>(&new_leaves[0], new_leaves.size()));
        }
    }

    return *this;
}
