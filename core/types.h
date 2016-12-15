#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>
#include <boost/intrusive_ptr.hpp>
#include <experimental/optional>
#include <symengine/basic.h>

using std::experimental::optional;

struct nothing {
};

class BaseExpression;
typedef const BaseExpression* BaseExpressionPtr;
typedef boost::intrusive_ptr<const BaseExpression> BaseExpressionRef;

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
	StringType = 9
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
	StringExtendedType = StringType << CoreTypeShift
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

constexpr TypeMask MakeTypeMask(Type type) {
    return TypeMask(1) << type;
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

class MatchIdData { // uniquely identifies a specific match
protected:
	size_t m_ref_count;

	friend void intrusive_ptr_add_ref(MatchIdData *data);
	friend void intrusive_ptr_release(MatchIdData *data);
};

typedef boost::intrusive_ptr<MatchIdData> MatchId;

class MatchContext;

class Match;

std::ostream &operator<<(std::ostream &s, const Match &m);

class Expression;
typedef boost::intrusive_ptr<const Expression> ExpressionRef;
typedef const Expression *ExpressionPtr;

template<typename S>
class ExpressionImplementation;
class DynamicSlice;
typedef const ExpressionImplementation<DynamicSlice> DynamicExpression;
typedef const DynamicExpression* DynamicExpressionPtr;
typedef boost::intrusive_ptr<const DynamicExpression> DynamicExpressionRef;

class Symbol;

// in contrast to BaseExpressionRef, SymbolRefs are not constant. Symbols might
// change, after all, if rules are added.
typedef boost::intrusive_ptr<Symbol> SymbolRef;

class String;

typedef boost::intrusive_ptr<const String> StringRef;

class Evaluation;

class SortKey;

typedef SymEngine::RCP<const SymEngine::Basic> SymbolicForm;

class BaseExpression;
class Expression;

BaseExpressionRef from_symbolic_form(const SymbolicForm &form, const Evaluation &evaluation);

class BaseExpression {
protected:
    const ExtendedType _extended_type;

protected:
    mutable size_t _ref_count;
    mutable optional<SymbolicForm> _symbolic_form;

    virtual bool instantiate_symbolic_form() const {
        throw std::runtime_error("instantiate_symbolic_form not implemented");
    }

	virtual optional<hash_t> compute_match_hash() const {
		throw std::runtime_error("compute_match_hash not implemented");
	}

public:
    template<typename T>
    inline void set_symbolic_form(const SymEngine::RCP<const T> &form) const {
       _symbolic_form = SymEngine::rcp_static_cast<const SymEngine::Basic>(form);
    }

    inline void set_no_symbolic_form() const {
        _symbolic_form = SymbolicForm();
    }

    inline BaseExpression(ExtendedType type) : _extended_type(type), _ref_count(0) {
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

	inline bool is_symengine_simplified() const {
		// this returns true if this expression has already been converted
		// into and then rebuilt from the SymEngine representation.

		// it also returns true if no SymEngine represenation was found in
		// that process (i.e. _symbolic_form is set, but to nullptr). this
		// just means that is can not be simplified any more.

		return bool(_symbolic_form);
	}

    inline bool no_symbolic_form() const {
        // checks whether there is no SymEngine form for this expression.
        // if false is returned, it can also mean that the answer is not
        // known currently and symbolic_form() needs to be called.

        return _symbolic_form and (*_symbolic_form).is_null();
    }

	inline SymbolicForm symbolic_form() const {
		if (!_symbolic_form) {
			if (!instantiate_symbolic_form()) {
                set_no_symbolic_form();
			}
		}

		return *_symbolic_form;
	}

	virtual BaseExpressionRef expand(const Evaluation &evaluation) const {
		return BaseExpressionRef();
	}

	inline bool same(const BaseExpressionRef &expr) const {
        return same(*expr);
    }

    virtual bool same(const BaseExpression &expr) const = 0;

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

    virtual BaseExpressionRef replace_all(const Match &match) const {
		return BaseExpressionRef();
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

	friend void intrusive_ptr_add_ref(const BaseExpression *expr);
    friend void intrusive_ptr_release(const BaseExpression *expr);
};

inline BaseExpressionRef coalesce(const BaseExpressionRef &a, const BaseExpressionRef &b) {
    return a ? a : b;
}

#include "heap.h"

inline std::ostream &operator<<(std::ostream &s, const BaseExpressionRef &expr) {
    if (expr) {
        s << expr->fullform();
    } else {
        s << "IDENTITY";
    }
    return s;
}

#include "arithmetic.h"

class OperationsInterface :
	virtual public ArithmeticOperations {
};

class Slice {
public:
	const size_t _size;
	const BaseExpressionRef * const _address;

	inline Slice(const BaseExpressionRef *address, size_t size) : _address(address), _size(size) {
	}
};

typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineUnaryFunction)(
	const SymEngine::RCP<const SymEngine::Basic>&);

typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineBinaryFunction)(
	const SymEngine::RCP<const SymEngine::Basic>&,
	const SymEngine::RCP<const SymEngine::Basic>&);

typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineNAryFunction)(
	const SymEngine::vec_basic&);

class InstantiateSymbolicForm {
public:
    typedef std::function<bool(const Expression *expr)> Function;

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

class Expression : public BaseExpression, virtual public OperationsInterface {
private:
	mutable Cache *_cache;

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
		_head(head), _slice_ptr(slice_ptr), _cache(nullptr) {
	}

	inline ~Expression() {
		if (_cache) {
			Heap::release_cache(_cache);
		}
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
			BaseExpressionRef materialized;
			return f(materialize(materialized), size());
		}
	}

	template<SliceMethodOptimizeTarget Optimize = DoNotCompileToSliceType, typename F>
	inline auto with_slice(const F &f) const;

	template<typename F>
	inline auto map(const BaseExpressionRef &head, const F &f) const;

	virtual const BaseExpressionRef *materialize(BaseExpressionRef &materialized) const = 0;

	virtual inline BaseExpressionPtr head(const Evaluation &evaluation) const final {
		return _head.get();
	}

    inline BaseExpressionPtr head() const {
        return _head.get();
    }

    virtual bool is_sequence() const {
		return _head->extended_type() == SymbolSequence;
	}

	BaseExpressionRef evaluate_expression(
		const Evaluation &evaluation) const;

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const Evaluation &evaluation) const = 0;

	virtual ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const = 0;

	inline bool has_cache() const {
		return _cache;
	}

	inline Cache *cache() const {
		Cache *cache = _cache;
		if (!cache) {
			cache = Heap::new_cache();
			_cache = cache;
		}
		return cache;
	}

	virtual optional<SymEngine::vec_basic> symbolic_operands() const = 0;

    virtual bool instantiate_symbolic_form() const;

	inline bool symbolic_1(const SymEngineUnaryFunction &f) const;

	inline bool symbolic_2(const SymEngineBinaryFunction &f) const;

	inline bool symbolic_n(const SymEngineNAryFunction &f) const {
		const optional<SymEngine::vec_basic> operands = symbolic_operands();
		if (operands) {
			_symbolic_form = f(*operands);
			return true;
		} else {
			return false;
		}
	}

	virtual MatchSize leaf_match_size() const = 0;

	inline PatternMatcherRef expression_matcher() const;

	inline PatternMatcherRef string_matcher() const;
};

inline bool instantiate_symbolic_form(const Expression *expr) {
    const auto &f = InstantiateSymbolicForm::lookup(
        expr->_head->extended_type());
    if (f) {
        return f(expr);
    } else {
        return false;
    }
}

inline std::ostream &operator<<(std::ostream &s, const ExpressionRef &expr) {
	s << boost::static_pointer_cast<const BaseExpression>(expr);
	return s;
}

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
