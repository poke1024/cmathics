#pragma once

class SwappableBaseExpressionRef {
private:
    BaseExpressionRef *m_ref;

public:
    inline SwappableBaseExpressionRef() : m_ref(nullptr) {
    }

    inline SwappableBaseExpressionRef(BaseExpressionRef &ref) : m_ref(&ref) {
    }

    inline SwappableBaseExpressionRef &operator=(const SwappableBaseExpressionRef &ref) {
        m_ref = ref.m_ref;
        return *this;
    }

    inline BaseExpressionPtr operator->() const {
        return m_ref->get();
    }

    friend void swap(SwappableBaseExpressionRef a, SwappableBaseExpressionRef b) {
        a.m_ref->unsafe_swap(*b.m_ref);
    }
};

class MutableIterator {
private:
    std::vector<BaseExpressionRef>::iterator m_iterator;

public:
    using iterator_type = std::vector<BaseExpressionRef>::iterator;

    inline MutableIterator() {
    }

    inline MutableIterator(const std::vector<BaseExpressionRef>::iterator &iterator) :
        m_iterator(iterator) {
    }

    inline MutableIterator &operator=(const MutableIterator &i) {
        m_iterator = i.m_iterator;
        return *this;
    }

    inline bool operator==(const MutableIterator &i) const {
        return m_iterator == i.m_iterator;
    }

    inline bool operator>=(const MutableIterator &i) const {
        return m_iterator >= i.m_iterator;
    }

    inline bool operator>(const MutableIterator &i) const {
        return m_iterator > i.m_iterator;
    }

    inline bool operator<=(const MutableIterator &i) const {
        return m_iterator <= i.m_iterator;
    }

    inline bool operator<(const MutableIterator &i) const {
        return m_iterator < i.m_iterator;
    }

    inline bool operator!=(const MutableIterator &i) const {
        return m_iterator != i.m_iterator;
    }

    inline SwappableBaseExpressionRef operator[](size_t i) const {
        return SwappableBaseExpressionRef(m_iterator[i]);
    }

    inline SwappableBaseExpressionRef operator*() const {
        return SwappableBaseExpressionRef(*m_iterator);
    }

    inline auto operator-(const MutableIterator &i) const {
        return m_iterator - i.m_iterator;
    }

    inline MutableIterator operator--(int) {
        MutableIterator previous(*this);
        --m_iterator;
        return previous;
    }

    inline MutableIterator &operator--() {
        --m_iterator;
        return *this;
    }

    inline MutableIterator operator++(int) {
        MutableIterator previous(*this);
        ++m_iterator;
        return previous;
    }

    inline MutableIterator &operator++() {
        ++m_iterator;
        return *this;
    }

    inline MutableIterator operator+(size_t i) const {
        return MutableIterator(m_iterator + i);
    }

    inline MutableIterator &operator+=(size_t i) {
        m_iterator += i;
        return *this;
    }

    inline MutableIterator operator-(size_t i) const {
        return MutableIterator(m_iterator - i);
    }

    inline MutableIterator &operator-=(size_t i) {
        m_iterator -= i;
        return *this;
    }
};

namespace std {
    template<>
    struct iterator_traits<MutableIterator> {
        using difference_type = MutableIterator::iterator_type::difference_type;
        using value_type = SwappableBaseExpressionRef;
    };
}

template<typename Allocator>
class LeafVectorBase {
private:
    std::vector<BaseExpressionRef, Allocator> m_leaves;
    TypeMask m_mask;

public:
    inline LeafVectorBase() : m_mask(0) {
    }

    inline LeafVectorBase(std::vector<BaseExpressionRef, Allocator> &&leaves, TypeMask mask) :
        m_leaves(leaves), m_mask(mask) {
    }

    inline LeafVectorBase(std::vector<BaseExpressionRef, Allocator> &&leaves) :
        m_leaves(leaves), m_mask(0) {

        for (size_t i = 0; i < m_leaves.size(); i++) {
            m_mask |= m_leaves[i]->type_mask();
        }
    }

    inline LeafVectorBase(LeafVectorBase<Allocator> &&v) :
        m_leaves(std::move(v.m_leaves)), m_mask(v.m_mask) {
    }

    inline void push_back(BaseExpressionRef &&leaf) {
        m_mask |= leaf->type_mask();
        m_leaves.emplace_back(std::move(leaf));
    }

    inline void push_back_copy(const BaseExpressionRef &leaf) {
        push_back(BaseExpressionRef(leaf));
    }

    inline TypeMask type_mask() const {
        return m_mask;
    }

    inline bool empty() const {
        return m_leaves.empty();
    }

    inline size_t size() const {
        return m_leaves.size();
    }

    inline void reserve(size_t n) {
        m_leaves.reserve(n);
    }

    inline auto begin() const {
        return m_leaves.begin();
    }

    inline auto end() const {
        return m_leaves.end();
    }

    inline std::vector<BaseExpressionRef> &&unsafe_grab_internal_vector() {
        return std::move(m_leaves);
    }

    inline BaseExpressionRef &&unsafe_grab_leaf(size_t i) {
        return std::move(m_leaves[i]);
    }

    inline void sort() {
        std::sort(
            MutableIterator(m_leaves.begin()),
            MutableIterator(m_leaves.end()),
            [] (const SwappableBaseExpressionRef &x, const SwappableBaseExpressionRef &y) {
                return x->sort_key().compare(y->sort_key()) < 0;
            });
    }
};

using LeafVector = LeafVectorBase<std::allocator<BaseExpressionRef>>;

class TempVector : public std::vector<UnsafeBaseExpressionRef, VectorAllocator<UnsafeBaseExpressionRef>> {
public:
    inline TempVector() : vector<UnsafeBaseExpressionRef, VectorAllocator<UnsafeBaseExpressionRef>>(
        Pool::unsafe_ref_vector_allocator()) {
    }

    inline ExpressionRef to_list(const Evaluation &evaluation) const;
};
