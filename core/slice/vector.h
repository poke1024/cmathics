#pragma once

class TemporaryRefVector : public std::vector<UnsafeBaseExpressionRef, VectorAllocator<UnsafeBaseExpressionRef>> {
private:
	using Allocator = VectorAllocator<UnsafeBaseExpressionRef>;

	static Allocator s_allocator;

public:
	inline TemporaryRefVector() : vector<UnsafeBaseExpressionRef, Allocator>(s_allocator) {
	}

	inline ExpressionRef to_expression(const BaseExpressionRef &head) const;
};

class SortKeyVector : public std::vector<SortKey, VectorAllocator<SortKey>> {
private:
	using Allocator = VectorAllocator<SortKey>;

	static Allocator s_allocator;

public:
	inline SortKeyVector(size_t n) : vector<SortKey, Allocator>(n, s_allocator) {
	}
};

class IndexVector : public std::vector<size_t, VectorAllocator<size_t>> {
private:
	using Allocator = VectorAllocator<size_t>;

	static Allocator s_allocator;

public:
	inline IndexVector() : vector<size_t, Allocator>(s_allocator) {
	}
};

template<typename V>
ExpressionRef sorted(const V &vector, const BaseExpressionRef &head, const Evaluation &evaluation);

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

    inline ExpressionRef sorted(const BaseExpressionRef &head, const Evaluation &evaluation) const {
	    return ::sorted(m_leaves, head, evaluation);
    }
};

using LeafVector = LeafVectorBase<std::allocator<BaseExpressionRef>>;
