#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>
#include <boost/intrusive_ptr.hpp>
#include <experimental/optional>

#include "hash.h"

class BaseExpression;
typedef const BaseExpression* BaseExpressionPtr;
typedef boost::intrusive_ptr<const BaseExpression> BaseExpressionRef;

enum Type : uint8_t { // values represented as bits in TypeMasks
	SymbolType = 0,
	MachineIntegerType = 1,
	BigIntegerType = 2,
	MachineRealType = 3,
	BigRealType = 4,
	RationalType = 5,
	ComplexType = 6,
	ExpressionType = 7,
	StringType = 8
};

// CoreTypeBits is the number of bits needed to represent every value in
// Type, i.e. SymbolType, MachineIntegerType, ..., StringType
constexpr int CoreTypeBits = 4;

// CoreTypeShift is the number of bits needed to represent the additional
// extended type information attached to a core type.
constexpr int CoreTypeShift = 5;

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
	RationalExtendedType = RationalType << CoreTypeShift,
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

constexpr int MaxStaticSliceSize = 3;
static_assert(MaxStaticSliceSize >= 3, "must be >= 3");

constexpr int MinPackedSliceSize = 16;
static_assert(MinPackedSliceSize > MaxStaticSliceSize, "MinPackedSliceSize too small");

enum SliceCode : uint8_t {
	StaticSlice0Code = 0,
	StaticSlice1Code = 1,
	StaticSlice2Code = 2,
	StaticSlice3Code = 3,
	StaticSliceNCode = StaticSlice0Code + MaxStaticSliceSize,

	DynamicSliceCode = StaticSliceNCode + 1,

	PackedSliceMachineIntegerCode = DynamicSliceCode + 1,
	PackedSliceMachineRealCode = DynamicSliceCode + 2,
	PackedSliceBigIntegerCode = DynamicSliceCode + 3,
	PackedSliceRationalCode = DynamicSliceCode + 4,
	PackedSliceStringCode = DynamicSliceCode + 5,

	NumberOfSliceCodes = PackedSliceStringCode + 1,
	Unknown = 255
};

constexpr inline bool is_packed_slice(SliceCode id) {
	return id >= PackedSliceMachineIntegerCode && id <= PackedSliceStringCode;
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
constexpr match_size_t MATCH_MAX = INT64_MAX;

class MatchSize {
private:
	match_size_t _min;
	match_size_t _max;

	inline MatchSize(match_size_t min, match_size_t max) : _min(min), _max(max) {
	}

public:
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
    typedef typename BaseExpressionTuple<N - 1, BaseExpressionRef, refs...>::type type;
};

template<typename... refs>
struct BaseExpressionTuple<0, refs...> {
    typedef std::tuple<refs...> type;
};

class MatchId { // uniquely identifies a specific match
private:
    BaseExpressionRef _patt; // could be a weak pointer
    BaseExpressionRef _item; // could be a weak pointer

public:
	inline MatchId() {
	}

	inline MatchId(const MatchId& id) : _patt(id._patt), _item(id._item) {
	}

	inline MatchId(const BaseExpressionRef &patt, const BaseExpressionRef &item) :
        _patt(patt), _item(item) {
	}

	inline MatchId &operator=(const MatchId &other) {
		_patt = other._patt;
		_item = other._item;
		return *this;
	}

	inline bool operator==(const MatchId &other) const {
        return _patt == other._patt && _item == other._item;
	}

	inline void reset() {
		_patt.reset();
		_item.reset();
	}
};

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

class Evaluation;

class SortKey;

class BaseExpression {
protected:
	const ExtendedType _extended_type;

protected:
    mutable size_t _ref_count;

public:
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

    inline bool same(const BaseExpressionRef &expr) const {
        return same(*expr);
    }

    virtual bool same(const BaseExpression &expr) const = 0;

    virtual hash_t hash() const = 0;

    virtual std::string fullform() const = 0;

	inline BaseExpressionRef evaluate(const BaseExpressionRef &self, const Evaluation &evaluation) const;

    // various getters

    virtual BaseExpressionRef head() const {
        return BaseExpressionRef(); // no head available
    }

    virtual BaseExpressionPtr head_ptr() const {
        return nullptr; // no head available
    }

    virtual bool is_sequence() const {
        return false;
    }

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

	virtual bool match_leaves(MatchContext &_context, const BaseExpressionRef &patt) const {
		throw std::runtime_error("need an Expression to match leaves");
	}

	virtual MatchSize match_size() const {
        return MatchSize::exactly(1); // default
    }

    virtual BaseExpressionRef replace_all(const Match &match) const {
		return BaseExpressionRef();
    }

    virtual BaseExpressionRef clone() const {
        throw std::runtime_error("not implemented yet");
    }

	virtual BaseExpressionRef clone(const BaseExpressionRef &head) const {
		throw std::runtime_error("cannot clone with head");
	}

	virtual DynamicExpressionRef to_dynamic_expression(const BaseExpressionRef &self) const {
		throw std::runtime_error(std::string("cannot create refs expression for ") + typeid(this).name());
	}

	virtual const Symbol *lookup_name() const {
		return nullptr;
	}

	virtual SortKey sort_key() const;
	virtual SortKey pattern_key() const;

	friend void intrusive_ptr_add_ref(const BaseExpression *expr);
    friend void intrusive_ptr_release(const BaseExpression *expr);
};

#include "heap.h"

inline void intrusive_ptr_add_ref(const BaseExpression *expr) {
    ++expr->_ref_count;
}

inline void intrusive_ptr_release(const BaseExpression *expr) {
    if(--expr->_ref_count == 0) {
        Heap::release(const_cast<BaseExpression*>(expr));
    }
}

inline std::ostream &operator<<(std::ostream &s, const BaseExpressionRef &expr) {
    if (expr) {
        s << expr->fullform();
    } else {
        s << "IDENTITY";
    }
    return s;
}

#include "arithmetic.h"
#include "structure.h"

class OperationsInterface :
	virtual public ArithmeticOperations,
	virtual public StructureOperations {
};

class Slice {
public:
	const size_t _size;
	const BaseExpressionRef * const _address;

	inline Slice(const BaseExpressionRef *address, size_t size) : _address(address), _size(size) {
	}
};

class Expression : public BaseExpression, virtual public OperationsInterface {
private:
	mutable Cache *_cache;

public:
	const BaseExpressionRef _head;
	const Slice * const _slice_ptr;

	inline Expression(const BaseExpressionRef &head, SliceCode slice_id, const Slice *slice_ptr) :
		BaseExpression(build_extended_type(ExpressionType, slice_id)),
		_head(head), _slice_ptr(slice_ptr), _cache(nullptr) {
	}

	inline ~Expression() {
		if (_cache) {
			Heap::release_cache(_cache);
		}
	}

	inline SliceCode slice_code() const {
		return SliceCode(extended_type_info(_extended_type));
	}

	inline size_t size() const {
		return _slice_ptr->_size;
	}

	template<SliceCode SliceCode = SliceCode::Unknown, typename F>
	inline auto with_leaves_array(const F &f) const {
		const BaseExpressionRef *leaves = _slice_ptr->_address;

		// the check with slice_needs_no_materialize() here is really just an additional
		// optimization that allows the compiler to reduce this code to the first case
		// alone if it's statically clear that we're always dealing with a non-packed
		// slice.

		if (slice_needs_no_materialize(SliceCode) || leaves) {
			return f(leaves, size());
		} else {
			BaseExpressionRef materialized;
			leaves = materialize(materialized);
			return f(leaves, size());
		}
	}

	virtual const BaseExpressionRef *materialize(BaseExpressionRef &materialized) const = 0;

	virtual BaseExpressionRef leaf(size_t i) const = 0;

	virtual BaseExpressionRef head() const {
		return _head;
	}

	virtual BaseExpressionPtr head_ptr() const {
		return _head.get();
	}

	virtual bool is_sequence() const {
		return _head->extended_type() == SymbolSequence;
	}

	BaseExpressionRef evaluate_expression(
		const BaseExpressionRef &self, const Evaluation &evaluation) const;

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const ExpressionRef &self, const Evaluation &evaluation) const = 0;

	virtual ExpressionRef slice(index_t begin, index_t end = INDEX_MAX) const = 0;

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
};

inline std::ostream &operator<<(std::ostream &s, const ExpressionRef &expr) {
	s << boost::static_pointer_cast<const BaseExpression>(expr);
	return s;
}

#endif
