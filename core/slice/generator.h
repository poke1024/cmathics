#pragma once

#include "vector.h"

template<typename FReference, typename R>
inline auto generate_vector_sequentially(FReference f, const R &r) {
	LeafVector v;
	r(v);
	auto store = [&v] (BaseExpressionRef &&leaf) mutable {
		v.push_back(std::move(leaf));
	};
	f(store);
	return v;
}

class Generator {
};

class FGenerator : public Generator { // [f]ixed size generator
};

class VGenerator : public Generator { // [v]ariable size generator
};

template<typename FReference>
struct FSGenerator : public FGenerator { // [f]ixed size [s]equential
	const FReference f;
	const size_t n;

	inline FSGenerator(FReference f_, size_t n_) : f(f_), n(n_) {
	}

	inline size_t size() const {
		return n;
	}

	template<size_t N>
	std::tuple<std::array<BaseExpressionRef, N>, TypeMask> array() const {
		assert(n == N);
		std::array<BaseExpressionRef, N> array;
		TypeMask mask = 0;
		size_t i = 0;
		auto store = [&mask, &array, &i] (BaseExpressionRef &&leaf) mutable {
            assert(i < N);
			mask |= leaf->type_mask();
			array[i++].unsafe_mutate(std::move(leaf));
		};
		f(store);
        assert(i == N);
		return std::make_tuple(std::move(array), mask);
	}

	LeafVector vector() const {
		return generate_vector_sequentially(f, [this] (auto &v) {
			v.reserve(n);
		});
	}
};

template<typename FReference>
struct VSGenerator : public VGenerator { // [v]ariable size [s]equential
	const FReference f;

	inline VSGenerator(FReference f_) : f(f_) {
	}

	LeafVector vector() const {
		return generate_vector_sequentially(f, [] (auto &v) {
		});
	}
};

#if FASTER_COMPILE
using GenericSGeneratorCallback = std::function<void(const std::function<void(BaseExpressionRef&&)>&)>;
#endif

template<typename F>
inline auto sequential(const F &f, size_t n) {
#if FASTER_COMPILE
	return FSGenerator<GenericSGeneratorCallback>(f, n);
#else
	return FSGenerator<const F&>(f, n);
#endif
}

template<typename F>
inline auto sequential(F &f, size_t n) {
#if FASTER_COMPILE
	return FSGenerator<GenericSGeneratorCallback>(f, n);
#else
	return FSGenerator<F&>(f, n);
#endif
}

template<typename F>
inline auto sequential(const F &f) {
#if FASTER_COMPILE
	return VSGenerator<GenericSGeneratorCallback>(f);
#else
	return VSGenerator<const F&>(f);
#endif
}

template<typename F>
inline auto sequential(F &f) {
#if FASTER_COMPILE
	return VSGenerator<GenericSGeneratorCallback>(f);
#else
	return VSGenerator<F&>(f);
#endif
}

template<typename F>
struct FPGenerator : public FGenerator { // [f]ixed size [p]arallel
private:
	const F m_generate;
	const size_t m_n;
	const Evaluation &m_evaluation;

public:
	inline FPGenerator(const F &f, size_t n, const Evaluation &evaluation) :
		m_generate(f), m_n(n), m_evaluation(evaluation) {
	}

	inline size_t size() const {
		return m_n;
	}

	template<size_t N>
	std::tuple<std::array<BaseExpressionRef, N>, TypeMask> array() const {
		std::array<BaseExpressionRef, N> array;
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		parallelize([this, &array, &mask] (size_t i) {
			BaseExpressionRef leaf = m_generate(i);
			mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
			array[i].unsafe_mutate(std::move(leaf));
		}, N, m_evaluation);
		return std::make_tuple(std::move(array), mask.load(std::memory_order_relaxed));
	}

	LeafVector vector() const {
		std::vector<BaseExpressionRef> v(m_n);
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		parallelize([this, &v, &mask] (size_t i) {
			BaseExpressionRef leaf = m_generate(i);
			mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
			v[i].unsafe_mutate(std::move(leaf));
		}, m_n, m_evaluation);
		return LeafVector(std::move(v), mask.load(std::memory_order_relaxed));
	}
};

template<typename F>
struct VPGenerator : public VGenerator { // [v]ariable size [p]arallel
private:
	const F m_generate;
	const size_t n;

public:
	inline VPGenerator(const F &f_, size_t n_) : m_generate(f_), n(n_) {
	}

	LeafVector vector() const {
		std::vector<BaseExpressionRef> v;
		v.reserve(n); // allocate for worst case
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		parallelize([this, &v, &mask] (size_t i) {
			BaseExpressionRef leaf = m_generate(i);
			if (leaf) {
				mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
				v.emplace_back(std::move(leaf)); // FIXME
			}
		}, n);
		return LeafVector(std::move(v), mask.load(std::memory_order_relaxed));
	}
};

#if FASTER_COMPILE
using GenericPGeneratorCallback = std::function<BaseExpressionRef(size_t)>;
#endif

template<typename F>
inline auto parallel(const F &f, size_t n, const Evaluation &evaluation) {
#if FASTER_COMPILE
	return FPGenerator<GenericPGeneratorCallback>(f, n, evaluation);
#else
	return FPGenerator<const F&>(f, n, evaluation);
#endif
}

class up_to {
private:
	const size_t m_n;

public:
	inline up_to(size_t n) : m_n(n) {
	}

	inline size_t operator*() const {
		return m_n;
	}
};

template<typename F>
inline auto parallel(const F &f, up_to n, const Evaluation &evaluation) {
#if FASTER_COMPILE
	return VPGenerator<GenericPGeneratorCallback>(f, *n, evaluation);
#else
	return VPGenerator<const F&>(f, *n, evaluation);
#endif
}

template<typename V>
ExpressionRef sorted(const V &vector, const BaseExpressionRef &head, const Evaluation &evaluation) {
	const size_t n = vector.size();

	SortKeyVector keys(n);
	IndexVector indices;
	indices.reserve(n);

	for (size_t i = 0; i < n; i++) {
		vector[i]->sort_key(keys[i], evaluation);
		indices.push_back(i);
	}

	std::sort(indices.begin(), indices.end(), [&keys, &evaluation] (size_t i, size_t j) {
		return keys[i].compare(keys[j], evaluation) < 0;
	});

	return expression(head, sequential([n, &vector, &indices] (auto &store) {
		for (size_t i = 0; i < n; i++) {
			store(BaseExpressionRef(vector[indices[i]]));
		}
	}, n));
}
