#pragma once

#include "binary.h"

template<typename U, typename V>
struct Comparison {
	template<typename F>
	inline static auto compare(const U &u, const V &v, const F &f) {
		return f(u.value, v.value);
	}
};

template<>
struct Comparison<BigInteger, MachineInteger> {
	template<typename F>
	inline static auto compare(const BigInteger &u, const MachineInteger &v, const F &f) {
		return f(u.value, long(v.value));
	}
};

template<>
struct Comparison<MachineInteger, BigInteger> {
	template<typename F>
	inline static auto compare(const MachineInteger &u, const BigInteger &v, const F &f) {
		return f(long(u.value), v.value);
	}
};

template<>
struct Comparison<BigRational, MachineInteger> {
	template<typename F>
	inline static auto compare(const BigRational &u, const MachineInteger &v, const F &f) {
		return f(u.value, long(v.value));
	}
};

template<>
struct Comparison<MachineInteger, BigRational> {
	template<typename F>
	inline static auto compare(const MachineInteger &u, const BigRational &v, const F &f) {
		return f(long(u.value), v.value);
	}
};

template<>
struct Comparison<MachineInteger, BigReal> {
	template<typename F>
	inline static auto compare(const MachineInteger &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, MachineInteger> {
	template<typename F>
	inline static auto compare(const BigReal &u, const MachineInteger &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<MachineReal, BigReal> {
	template<typename F>
	inline static auto compare(const MachineReal &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, MachineReal> {
	template<typename F>
	inline static auto compare(const BigReal &u, const MachineReal &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigInteger, BigReal> {
	template<typename F>
	inline static auto compare(const BigInteger &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, BigInteger> {
	template<typename F>
	inline static auto compare(const BigReal &u, const BigInteger &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigRational, BigReal> {
	template<typename F>
	inline static auto compare(const BigRational &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value, v.prec), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, BigRational> {
	template<typename F>
	inline static auto compare(const BigReal &u, const BigRational &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value, u.prec));
	}
};

template<bool Less, bool Equal, bool Greater>
struct inequality {
	using Function = BinaryComparisonFunction;

	static inline tribool fallback(
		const BaseExpressionPtr a,
		const BaseExpressionPtr b,
		const Evaluation &evaluation) {

		const SymbolicFormRef sym_a = symbolic_form(a, evaluation);
		if (!sym_a->is_none()) {
			const SymbolicFormRef sym_b = symbolic_form(b, evaluation);
			if (!sym_b->is_none()) {
				if (sym_a->get()->__eq__(*sym_b->get())) {
					return Equal;
				}

				const Precision pa = precision(a);
				const Precision pb = precision(b);

				const Precision p = std::max(pa, pb);
				const Precision p0 = std::min(pa, pb);

				try {
					unsigned long prec = p.bits;
					const auto diff = SymEngine::sub(sym_a->get(), sym_b->get());

					while (true) {
						const auto ndiff = SymEngine::evalf(*diff, prec, true);

						if (ndiff->is_negative()) {
							return Less;
						} else if (ndiff->is_positive()) {
							return Greater;
						} else {
							if (p0.is_none()) {
								// we're probably dealing with an irrational as
								// in x < Pi. increase precision, try again.
								assert(!__builtin_umull_overflow(prec, 2, &prec));
							} else {
								return Equal;
							}
						}
					}
				} catch (const SymEngine::SymEngineException &e) {
					evaluation.sym_engine_exception(e);
					return undecided;
				}
			}
		}

		return undecided;
	}
};

struct less : public inequality<true, false, false> {
	template<typename U, typename V>
	static inline tribool function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
			return x < y;
		});
	}
};

struct less_equal : public inequality<true, true, false> {
	template<typename U, typename V>
	static inline tribool function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
			return x <= y;
		});
	}
};

struct greater : public inequality<false, false, true> {
	template<typename U, typename V>
	static inline tribool function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
			return x > y;
		});
	}
};

struct greater_equal : public inequality<false, true, true> {
	template<typename U, typename V>
	static inline tribool function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
			return x >= y;
		});
	}
};

template<bool Negate>
inline bool negate(bool f) {
	return Negate ? !f : f;
}

template<bool Unequal>
struct equal {
	using Function = BinaryComparisonFunction;

	template<typename U, typename V>
	static inline tribool function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(
			u, v, [] (const auto &x, const auto &y) {
				return negate<Unequal>(x == y);
			});
	}

	static inline tribool fallback(
		const BaseExpressionPtr a,
		const BaseExpressionPtr b,
		const Evaluation &evaluation) {

		switch (a->equals(*b)) {
			case true:
				return negate<Unequal>(true);
			case false:
				return negate<Unequal>(false);
			case undecided:
				return undecided;
			default:
				throw std::runtime_error("illegal equals result");
		}
	}
};

struct order {
	// note that the following methods are only ever called as part
	// of SortKey::compare, which should guarantee that there is
	// always a defined order between two elements. if there is not,
	// i.e. if we run into problems here, then the tuple ordering in
	// SortKey::compare is faulty (as it has to ensure a defined
	// ordering for all cases).

	using Function = BinaryOrderFunction;

	static inline int fallback(
		const BaseExpressionPtr a,
		const BaseExpressionPtr b,
		const Evaluation &evaluation) {

		const SymbolicFormRef sym_a = symbolic_form(a, evaluation);
		assert (!sym_a->is_none());
		const SymbolicFormRef sym_b = symbolic_form(b, evaluation);
		assert (!sym_b->is_none());

		return sym_a->get()->__cmp__(*sym_b->get());
	}

	template<typename U, typename V>
	static inline int function(const U &u, const V &v, const Evaluation &evaluation) {
		return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
			if (x < y) {
				return -1;
			} else if (x > y) {
				return 1;
			} else {
				return 0;
			}
		});
	}
};
