#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>
#include <symengine/basic.h>

#include <experimental/optional>
using std::experimental::optional;

#include "shared.h"
#include "concurrent/parallel.h"

struct nothing {
};

template<typename F>
class const_lambda_class {
public:
	const F &lambda;

	inline const_lambda_class(const F &l) : lambda(l) {
	}
};

template<typename F>
auto lambda(const F &l) {
	return const_lambda_class<F>(l);
}

template<typename F>
class mutable_lambda_class {
public:
	F &lambda;

	inline mutable_lambda_class(F &l) : lambda(l) {
	}
};

template<typename F>
auto lambda(F &l) {
	return mutable_lambda_class<F>(l);
}

class BaseExpression;
typedef const BaseExpression* BaseExpressionPtr;

typedef ConstSharedPtr<const BaseExpression> BaseExpressionRef;
typedef QuasiConstSharedPtr<const BaseExpression> CachedBaseExpressionRef;
typedef SharedPtr<const BaseExpression> MutableBaseExpressionRef;
typedef UnsafeSharedPtr<const BaseExpression> UnsafeBaseExpressionRef;

enum Type : uint8_t { // values represented as bits in TypeMasks
	SymbolType = 0,
	MachineIntegerType = 1,
	BigIntegerType = 2,
	MachineRealType = 3,
	BigRealType = 4,
	MachineRationalType = 5,
    BigRationalType = 6,
	ComplexType = 7,
	ExpressionType = 8,
	StringType = 9,
    MachineComplexType = 10,
    BigComplexType = 11
};

// CoreTypeBits is the number of bits needed to represent every value in
// Type, i.e. SymbolType, MachineIntegerType, ..., StringType
constexpr int CoreTypeBits = 4;

// CoreTypeShift is the number of bits needed to represent the additional
// extended type information attached to a core type.
constexpr int CoreTypeShift = 8;

enum ExtendedType : uint16_t;

constexpr inline ExtendedType build_extended_type(Type core, uint8_t extended) {
	return ExtendedType((ExtendedType(core) << CoreTypeShift) | extended);
}

constexpr inline auto extended_type_info(ExtendedType type) {
	return type & ((ExtendedType(1) << CoreTypeShift) - 1);
}

// extended type infos are not represented in TypeMasks.

enum ExtendedType : uint16_t {
	SymbolExtendedType = SymbolType << CoreTypeShift,

	#define SYMBOL(SYMBOLNAME) Symbol##SYMBOLNAME,
	#include "system_symbols.h"
	#undef SYMBOL

	MachineIntegerExtendedType = MachineIntegerType << CoreTypeShift,
	BigIntegerExtendedType = BigIntegerType << CoreTypeShift,
	MachineRealExtendedType = MachineRealType << CoreTypeShift,
	BigRealExtendedType = BigRealType << CoreTypeShift,
    MachineRationalExtendedType = MachineRationalType << CoreTypeShift,
	BigRationalExtendedType = BigRationalType << CoreTypeShift,
	ComplexExtendedType = ComplexType << CoreTypeShift,
	ExpressionExtendedType = ExpressionType << CoreTypeShift,
	StringExtendedType = StringType << CoreTypeShift,
    MachineComplexExtendedType = MachineComplexType << CoreTypeShift,
    BigComplexExtendedType = BigComplexType << CoreTypeShift
};

typedef uint32_t TypeMask;

// the presence of TypeMaskIsInexact indicates that the TypeMask may contain
// bits (types) that are not actually present (it will never miss a type though,
// i.e. not contain a bit for the type that is present).
constexpr TypeMask TypeMaskIsInexact = 1 << 31;

constexpr TypeMask UnknownTypeMask = TypeMask(-1); // inexact, all type bits set

constexpr inline bool is_exact_type_mask(TypeMask type_mask) {
	return (type_mask & TypeMaskIsInexact) == 0;
}

static_assert(TypeMaskIsInexact != 0,
	"TypeMaskIsInexact must fit into the TypeMask type");

static_assert((TypeMaskIsInexact >> CoreTypeBits) != 0,
	"CoreTypeBits is too large as it hides TypeMaskIsInexact");

static_assert(sizeof(TypeMask) * 8 >= (1 << CoreTypeBits),
	"TypeMask is too small; needs one bit for each of the 2^TypeBits basic types");

constexpr TypeMask make_type_mask(Type type) {
    return TypeMask(1) << type;
}

template<typename... Types>
constexpr TypeMask make_type_mask(Type type, Types... types) {
	return make_type_mask(type) | make_type_mask(types...);
}

inline bool is_homogenous(TypeMask mask) {
	return __builtin_popcount(mask) <= 1; // TypeMask contains only 0 or 1 types
}

typedef int64_t index_t; // may be negative as well
constexpr index_t INDEX_MAX = INT64_MAX;

const char *type_name(Type type);

typedef int64_t machine_integer_t;
typedef double machine_real_t;

#include "hash.h"

constexpr int MaxStaticSliceSize = 4;
static_assert(MaxStaticSliceSize >= 4, "must be >= 4");

constexpr int MinPackedSliceSize = 16;
static_assert(MinPackedSliceSize > MaxStaticSliceSize, "MinPackedSliceSize too small");

enum SliceCode : uint8_t {
	StaticSlice0Code = 0,
	StaticSlice1Code = 1,
	StaticSlice2Code = 2,
	StaticSlice3Code = 3,
    StaticSlice4Code = 4,
	StaticSliceNCode = StaticSlice0Code + MaxStaticSliceSize,

	DynamicSliceCode = StaticSliceNCode + 1,

	PackedSlice0Code = DynamicSliceCode + 1,
	PackedSliceMachineIntegerCode = PackedSlice0Code,
	PackedSliceMachineRealCode,
	PackedSliceNCode = PackedSliceMachineRealCode,

	NumberOfSliceCodes = PackedSliceNCode + 1,
	Unknown = 255
};

template<SliceCode slice_code>
struct PackedSliceType {
};

template<>
struct PackedSliceType<PackedSliceMachineIntegerCode> {
	typedef machine_integer_t type;
};

template<>
struct PackedSliceType<PackedSliceMachineRealCode> {
	typedef machine_real_t type;
};

constexpr inline bool is_packed_slice(SliceCode id) {
	return id >= PackedSlice0Code && id <= PackedSliceNCode;
}

constexpr inline bool is_static_slice(SliceCode code) {
	return code >= StaticSlice0Code && code <= StaticSliceNCode;
}

constexpr inline bool slice_needs_no_materialize(SliceCode id) {
	return is_static_slice(id) || id == SliceCode::DynamicSliceCode;
}

constexpr inline SliceCode static_slice_code(size_t n) {
	const SliceCode code = SliceCode(SliceCode::StaticSlice0Code + n);
	assert(code <= StaticSliceNCode);
	return code;
}

constexpr inline size_t static_slice_size(SliceCode code) {
	return size_t(code) - size_t(StaticSlice0Code);
}

typedef int64_t match_size_t; // needs to be signed
constexpr match_size_t MATCH_MAX = INT64_MAX >> 2; // safe for subtractions

class MatchSize {
private:
	match_size_t _min;
	match_size_t _max;

	inline MatchSize(match_size_t min, match_size_t max) : _min(min), _max(max) {
	}

public:
	inline MatchSize() {
	}

	static inline MatchSize exactly(match_size_t n) {
		return MatchSize(n, n);
	}

	static inline MatchSize at_least(match_size_t n) {
		return MatchSize(n, MATCH_MAX);
	}

	static inline MatchSize between(match_size_t min, match_size_t max) {
		return MatchSize(min, max);
	}

	inline match_size_t min() const {
		return _min;
	}

	inline match_size_t max() const {
		return _max;
	}

	inline bool contains(match_size_t s) const {
		return s >= _min && s <= _max;
	}

	inline optional<size_t> fixed_size() const {
		if (_min == _max) { // i.e. a finite, fixed integer
			return _min;
		} else {
			return optional<size_t>();
		}
	}

	inline bool matches(SliceCode code) const {
		if (is_static_slice(code)) {
			return contains(static_slice_size(code));
		} else {
			// inspect wrt minimum size of non-static slice
			const size_t min_size = MaxStaticSliceSize + 1;
			return _max >= min_size;
		}
	}

	inline MatchSize &operator+=(const MatchSize &size) {
		_min += size._min;
		if (_max == MATCH_MAX || size._max == MATCH_MAX) {
			_max = MATCH_MAX;
		} else {
			_max += size._max;
		}
		return *this;
	}
};

typedef std::experimental::optional<MatchSize> OptionalMatchSize;

class Definitions;
class Symbol;

template<int N, typename... refs>
struct BaseExpressionTuple {
    typedef typename BaseExpressionTuple<N - 1, BaseExpressionPtr, refs...>::type type;
};

template<typename... refs>
struct BaseExpressionTuple<0, refs...> {
    typedef std::tuple<refs...> type;
};

class MatchContext;

class Match;

typedef ConstSharedPtr<Match> MatchRef;
typedef UnsafeSharedPtr<Match> UnsafeMatchRef;

std::ostream &operator<<(std::ostream &s, const MatchRef &m);

class Expression;
typedef ConstSharedPtr<const Expression> ExpressionRef;
typedef UnsafeSharedPtr<const Expression> UnsafeExpressionRef;
typedef const Expression *ExpressionPtr;

template<typename S>
class ExpressionImplementation;
class DynamicSlice;
typedef const ExpressionImplementation<DynamicSlice> DynamicExpression;
typedef ConstSharedPtr<const DynamicExpression> DynamicExpressionRef;

class Symbol;

typedef ConstSharedPtr<const Symbol> SymbolRef;
typedef SharedPtr<const Symbol> MutableSymbolRef;
typedef ConstSharedPtr<const Symbol> ConstSymbolRef;
typedef UnsafeSharedPtr<const Symbol> UnsafeSymbolRef;

class String;

typedef ConstSharedPtr<const String> StringRef;
typedef UnsafeSharedPtr<const String> UnsafeStringRef;

class Evaluation;

class SortKey;

/*#if defined(WITH_SYMENGINE_THREAD_SAFE)
#else
static_assert(false, "need WITH_SYMENGINE_THREAD_SAFE");
#endif*/

typedef SymEngine::RCP<const SymEngine::Basic> SymEngineRef;

class SymbolicForm : public Shared<SymbolicForm, SharedPool> {
private:
	const SymEngineRef m_ref;
	const bool m_is_simplified; // owner BaseExpression was simplified to this SymbolicForm

public:
	SymbolicForm(const SymEngineRef &ref, bool is_simplified) :
		m_ref(ref), m_is_simplified(is_simplified) {
	}

	inline bool is_simplified() const {
		return m_is_simplified;
	}

	inline bool is_none() const {
		// checks whether there is no SymEngine form for this expression.
		return m_ref.is_null();
	}

	inline SymEngineRef get() const {
		return m_ref;
	}
};

typedef ConstSharedPtr<SymbolicForm> SymbolicFormRef;
typedef QuasiConstSharedPtr<SymbolicForm> CachedSymbolicFormRef;
typedef UnsafeSharedPtr<SymbolicForm> UnsafeSymbolicFormRef;

class BaseExpression;
class Expression;

BaseExpressionRef from_symbolic_form(const SymEngineRef &ref, const Evaluation &evaluation);

class BaseExpression : public Shared<BaseExpression, SharedPool> {
protected:
    const ExtendedType _extended_type;

protected:
	template<typename T>
	friend inline SymbolicFormRef symbolic_form(const T &item);

	friend inline SymbolicFormRef fast_symbolic_form(const Expression *expr);

	mutable CachedSymbolicFormRef m_symbolic_form;

	virtual SymbolicFormRef instantiate_symbolic_form() const {
		throw std::runtime_error("instantiate_symbolic_form not implemented");
	}

protected:
	virtual optional<hash_t> compute_match_hash() const {
		std::ostringstream s;
		s << "compute_match_hash not implemented for ";
		s << typeid(this).name();
		throw std::runtime_error(s.str());
	}

public:
    template<typename T>
    inline void set_simplified_form(const SymEngine::RCP<const T> &ref) const;

    inline BaseExpression(ExtendedType type) : _extended_type(type) {
    }

    virtual ~BaseExpression() {
    }

    inline Type type() const {
	    // basic type, e.g. MachineIntegerType, SymbolType, ...
	    return Type(_extended_type >> CoreTypeShift);
    }

	inline ExtendedType extended_type() const {
		return _extended_type; // extended type, e.g. SymbolBlank
	}

	inline TypeMask base_type_mask() const {
		return ((TypeMask)1) << type();
	}

	inline optional<hash_t> match_hash() const {
		if (type() == ExpressionType) {
			return compute_match_hash();
		} else {
			return hash();
		}
	}

	virtual BaseExpressionRef expand(const Evaluation &evaluation) const {
		return BaseExpressionRef();
	}

	inline bool same(const BaseExpressionRef &expr) const {
        return same(*expr); // redirect to same(const BaseExpression &expr)
    }

    virtual bool same(const BaseExpression &expr) const = 0;

    virtual bool equals(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual hash_t hash() const = 0;

    virtual std::string fullform() const = 0;

	inline BaseExpressionRef evaluate(const Evaluation &evaluation) const;

	inline BaseExpressionRef evaluate_or_copy(const Evaluation &evaluation) const;

    // various getters

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const = 0;

    virtual bool is_sequence() const {
        return false;
    }

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

	virtual MatchSize match_size() const {
        return MatchSize::exactly(1); // default
    }

    virtual BaseExpressionRef replace_all(const MatchRef &match) const {
		return BaseExpressionRef();
    }

	inline BaseExpressionRef replace_all_or_copy(const MatchRef &match) const {
		const BaseExpressionRef result = replace_all(match);
		if (result) {
			return result;
		} else {
			return BaseExpressionRef(this);
		}
	}

    virtual BaseExpressionRef clone() const {
        throw std::runtime_error("not implemented yet");
    }

	virtual ExpressionRef clone(const BaseExpressionRef &head) const {
		throw std::runtime_error("cannot clone with head");
	}

	virtual DynamicExpressionRef to_dynamic_expression(const BaseExpressionRef &self) const {
		throw std::runtime_error(std::string("cannot create refs expression for ") + typeid(this).name());
	}

    virtual double round_to_float() const {
        throw std::runtime_error("not implemented");
    }

	virtual const Symbol *lookup_name() const {
		return nullptr;
	}

    inline bool is_true() const {
        return extended_type() == SymbolTrue;
    }

	inline bool is_zero() const;

	inline bool is_one() const;

	inline Symbol *as_symbol() const;

	inline const Expression *as_expression() const;

	inline const String *as_string() const;

	template<ExtendedType HeadType, int NLeaves>
	inline bool has_form(const Evaluation &evaluation) const;

	virtual SortKey sort_key() const;
	virtual SortKey pattern_key() const;
};

inline BaseExpressionRef coalesce(const BaseExpressionRef &a, const BaseExpressionRef &b) {
    return a ? a : b;
}

#include "heap.h"

template<typename T>
inline void SharedPool::release(const T *obj) {
    ::Pool::release(const_cast<T*>(obj));
}

template<typename T>
inline void BaseExpression::set_simplified_form(const SymEngine::RCP<const T> &ref) const {
	m_symbolic_form.ensure([&ref] () {
        return Pool::SymbolicForm(SymEngine::rcp_static_cast<const SymEngine::Basic>(ref), true);
    });
}

inline std::ostream &operator<<(std::ostream &s, const BaseExpressionRef &expr) {
    if (expr) {
        s << expr->fullform();
    } else {
        s << "IDENTITY";
    }
    return s;
}

class Slice {
public:
	const size_t _size;
	const BaseExpressionRef * const _address;

	inline Slice(const BaseExpressionRef *address, size_t size) : _address(address), _size(size) {
	}
};

typedef SymEngineRef (*SymEngineUnaryFunction)(
	const SymEngineRef&);

typedef SymEngineRef (*SymEngineBinaryFunction)(
	const SymEngineRef&,
	const SymEngineRef&);

typedef SymEngineRef (*SymEngineNAryFunction)(
	const SymEngine::vec_basic&);

class InstantiateSymbolicForm {
public:
    typedef std::function<SymbolicFormRef(const Expression *expr)> Function;

private:
    static Function s_functions[256];

    static void add(ExtendedType type, const Function &f);

    static inline constexpr uint8_t index(ExtendedType type) {
        const size_t index = type & ((1LL << CoreTypeShift) - 1);
        assert(index < 256);
        return uint8_t(index);
    }

public:
    static void init();

    static inline const Function &lookup(ExtendedType type) {
        return s_functions[index(type)];
    }
};

enum SliceMethodOptimizeTarget {
	DoNotCompileToSliceType,
	CompileToSliceType,
	CompileToPackedSliceType
};

template<SliceMethodOptimizeTarget Optimize, typename R, typename F>
class SliceMethod {
};

#include "pattern.h"
#include "cache.h"

inline CacheRef Pool::new_cache() {
	return CacheRef(_s_instance->_caches.construct());
}

inline void Pool::release(Cache *cache) {
	_s_instance->_caches.destroy(cache);
}

class Expression : public BaseExpression {
private:
	mutable CachedCacheRef m_cache;

protected:
	template<SliceMethodOptimizeTarget Optimize, typename R, typename F>
	friend class SliceMethod;

	friend class ArraySlice;
	friend class VCallSlice;

	friend class SlowLeafSequence;

	const Slice * const _slice_ptr;

	virtual BaseExpressionRef materialize_leaf(size_t i) const = 0;

	virtual TypeMask materialize_type_mask() const = 0;

public:
    static constexpr Type Type = ExpressionType;

	const BaseExpressionRef _head;

	inline Expression(const BaseExpressionRef &head, SliceCode slice_id, const Slice *slice_ptr) :
		BaseExpression(build_extended_type(ExpressionType, slice_id)),
		_head(head),
		_slice_ptr(slice_ptr) {
	}

	template<size_t N>
	inline const BaseExpressionRef *static_leaves() const;

	inline SliceCode slice_code() const {
		return SliceCode(extended_type_info(_extended_type));
	}

	inline size_t size() const {
		return _slice_ptr->_size;
	}

	inline BaseExpressionRef leaf(size_t i) const;

	template<SliceCode StaticSliceCode = SliceCode::Unknown, typename F>
	inline auto with_leaves_array(const F &f) const {
		const BaseExpressionRef * const leaves = _slice_ptr->_address;

		// the check with slice_needs_no_materialize() here is really just an additional
		// optimization that allows the compiler to reduce this code to the first case
		// alone if it's statically clear that we're always dealing with a non-packed
		// slice.

		if (slice_needs_no_materialize(StaticSliceCode) || leaves) {
			return f(leaves, size());
		} else {
			UnsafeBaseExpressionRef materialized;
			return f(materialize(materialized), size());
		}
	}

	template<SliceMethodOptimizeTarget Optimize = DoNotCompileToSliceType, typename F>
	inline auto with_slice(const F &f) const;

	template<SliceMethodOptimizeTarget Optimize = DoNotCompileToSliceType, typename F>
	inline auto with_slice(F &f) const;

	template<typename F>
	inline auto map(const BaseExpressionRef &head, const F &f) const;

	template<typename F>
	inline auto parallel_map(const BaseExpressionRef &head, const F &f) const;

	virtual const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const = 0;

	virtual inline BaseExpressionPtr head(const Evaluation &evaluation) const final {
		return _head.get();
	}

    inline BaseExpressionPtr head() const {
        return _head.get();
    }

    virtual inline bool is_sequence() const final {
		return _head->extended_type() == SymbolSequence;
	}

	BaseExpressionRef evaluate_expression(
		const Evaluation &evaluation) const;

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const Evaluation &evaluation) const = 0;

	virtual ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const = 0;

	inline CacheRef get_cache() const { // concurrent.
		return m_cache;
	}

	inline CacheRef ensure_cache() const { // concurrent.
        return CacheRef(m_cache.ensure([] () {
            return Pool::new_cache();
        }));
	}

	virtual optional<SymEngine::vec_basic> symbolic_operands() const = 0;

    virtual SymbolicFormRef instantiate_symbolic_form() const;

	inline SymbolicFormRef symbolic_1(const SymEngineUnaryFunction &f) const;

	inline SymbolicFormRef symbolic_2(const SymEngineBinaryFunction &f) const;

	inline SymbolicFormRef symbolic_n(const SymEngineNAryFunction &f) const {
		const optional<SymEngine::vec_basic> operands = symbolic_operands();
		if (operands) {
			return Pool::SymbolicForm(f(*operands));
		} else {
			return Pool::NoSymbolicForm();
		}
	}

	virtual MatchSize leaf_match_size() const = 0;

	inline PatternMatcherRef expression_matcher() const;

	inline PatternMatcherRef string_matcher() const;

	virtual std::tuple<bool, UnsafeExpressionRef> thread(const Evaluation &evaluation) const = 0;
};

template<typename T>
inline SymbolicFormRef symbolic_form(const T &item) {
    return SymbolicFormRef(item->m_symbolic_form.ensure([&item] () {
        return item->instantiate_symbolic_form();
    }));
}

inline SymbolicFormRef fast_symbolic_form(const Expression *expr) {
	return SymbolicFormRef(expr->m_symbolic_form.ensure([expr] () {
        const auto &f = InstantiateSymbolicForm::lookup(
            expr->_head->extended_type());
        if (f) {
            try {
                return f(expr);
            } catch(const SymEngine::SymEngineException &e) {
                std::cerr << "SymEngine error: " << e.what() << std::endl;
                return Pool::NoSymbolicForm();
            }
        } else {
            return Pool::NoSymbolicForm();
        }
    }));
}

template<>
inline SymbolicFormRef symbolic_form<const ExpressionPtr&>(const ExpressionPtr& expr) {
	return fast_symbolic_form(expr);
}

inline std::ostream &operator<<(std::ostream &s, const ExpressionRef &expr) {
	s << static_pointer_cast<const BaseExpression>(expr);
	return s;
}

#include "rule.h"

template<bool CheckMatchSize>
inline BaseExpressionRef Rules::do_try_and_apply(
	const std::vector<Entry> &entries,
	const Expression *expr,
	const Evaluation &evaluation) const {

	hash_t match_hash;
	bool match_hash_valid = false;

	const size_t size = CheckMatchSize ? expr->size() : 0;

	for (const Entry &entry : entries) {
		if (CheckMatchSize && !entry.match_size.contains(size)) {
			continue;
		}

		if (entry.match_hash) {
			if (!match_hash_valid) {
				match_hash = expr->hash();
				match_hash_valid = true;
			}
			if (match_hash != *entry.match_hash) {
				continue;
			}
		}

		const BaseExpressionRef result =
			entry.rule->try_apply(expr, evaluation);
		if (result) {
			return result;
		}
	}

	return BaseExpressionRef();
}

template<typename F>
inline auto with_slices(const Expression *a, const Expression *b, const F &f) {
	return a->with_slice([b, &f] (const auto &slice_a) {
		return b->with_slice([&f, &slice_a] (const auto &slice_b) {
			return f(slice_a, slice_b);
		});
	});
};

inline const Expression *BaseExpression::as_expression() const {
	return static_cast<const Expression*>(this);
}

template<ExtendedType HeadType, int NLeaves>
inline bool BaseExpression::has_form(const Evaluation &evaluation) const {
	if (type() != ExpressionType) {
		return false;
	}
	const Expression * const expr = as_expression();
	if (expr->head(evaluation)->extended_type() == HeadType && expr->size() == NLeaves) {
		return true;
	} else {
		return false;
	}
}

#endif
