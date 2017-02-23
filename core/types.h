#ifndef TYPES_H
#define TYPES_H

#define DEBUG_SYMBOLIC 0

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>
#include <cassert>

#include <experimental/optional>
using std::experimental::optional;

#include "shared.h"
#include "concurrent/parallel.h"

// NDEBUG will remove asserts and
// trigger optimized slice generators.

#ifdef NDEBUG
#define FASTER_COMPILE 0
#else
#define FASTER_COMPILE 1
#endif

struct nothing {
};

enum {
    undecided = 2
};

typedef int tribool;

template<typename A, typename B>
inline auto coalesce(const A &a, const B &b) {
    if (a) {
        return a;
    } else {
        return A(b);
    }
}

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
	MachineComplexType = 7,
    BigComplexType = 8,
	ExpressionType = 9,
	StringType = 10,
    TypeCount
};

// CoreTypeBits is the number of bits needed to represent every value in
// Type, i.e. SymbolType, MachineIntegerType, ..., StringType
constexpr int CoreTypeBits = 4;

static_assert(TypeCount < (1 << CoreTypeBits), "CoreTypeBits too small");

// CoreTypeShift is the number of bits needed to represent the additional
// extended type information attached to a core type.
constexpr int CoreTypeShift = 8;

// extended type infos are not represented in TypeMasks.
enum ExtendedType {
	SymbolExtendedType = SymbolType << CoreTypeShift,
	MachineIntegerExtendedType = MachineIntegerType << CoreTypeShift,
	BigIntegerExtendedType = BigIntegerType << CoreTypeShift,
	MachineRealExtendedType = MachineRealType << CoreTypeShift,
	BigRealExtendedType = BigRealType << CoreTypeShift,
	MachineRationalExtendedType = MachineRationalType << CoreTypeShift,
	BigRationalExtendedType = BigRationalType << CoreTypeShift,
	MachineComplexExtendedType = MachineComplexType << CoreTypeShift,
	BigComplexExtendedType = BigComplexType << CoreTypeShift,
	ExpressionExtendedType = ExpressionType << CoreTypeShift,
	StringExtendedType = StringType << CoreTypeShift
};

constexpr inline ExtendedType build_extended_type(Type core, uint8_t extended) {
	return ExtendedType((ExtendedType(core) << CoreTypeShift) | extended);
}

constexpr inline auto extended_type_info(ExtendedType type) {
	return type & ((ExtendedType(1) << CoreTypeShift) - 1);
}

namespace S {
	enum _Name {
		GENERIC = SymbolExtendedType,
		#define SYMBOL(SYMBOLNAME) SYMBOLNAME,
		#include "system_symbols.h"
		#undef SYMBOL
	};
}

using SymbolName = S::_Name;

typedef uint32_t TypeMask;

// the presence of TypeMaskIsInexact indicates that the TypeMask may contain
// bits (types) that are not actually present (it will never miss a type though,
// i.e. not contain a bit for the type that is present).
constexpr TypeMask TypeMaskIsInexact = 1 << 31;

// TypeMaskSequence indicates that there might be at least one Sequence[...] element
// in the corresponding slice. If it's not set, it's safe to say that there is no
// such element in the slice.
constexpr TypeMask TypeMaskSequence = 1 << 30;

static_assert(TypeCount < 24, "too many bits needed to represent type in type mask");

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
#include "slice/code.h"

#include "core/pattern/size.h"

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
class BigSlice;
typedef const ExpressionImplementation<BigSlice> BigExpression;
typedef ConstSharedPtr<const BigExpression> BigExpressionRef;

class Symbol;

typedef ConstSharedPtr<const Symbol> SymbolRef;
typedef SharedPtr<const Symbol> MutableSymbolRef;
typedef ConstSharedPtr<const Symbol> ConstSymbolRef;
typedef UnsafeSharedPtr<const Symbol> UnsafeSymbolRef;

using SymbolPtr = const Symbol*;

class String;

typedef const String *StringPtr;
typedef ConstSharedPtr<const String> StringRef;
typedef UnsafeSharedPtr<const String> UnsafeStringRef;

class Evaluation;
class Symbols;

class SortKey;
class PatternSortKey;

#include "symbolic.h"

class BaseExpression;
class Expression;

using SExp = std::tuple<StringRef /* s */, machine_integer_t /* n */, int /* non_negative */, bool /* integer? */>;

#include "core/atoms/numeric.h"

struct StyleBoxOptions {
    inline StyleBoxOptions() :
        ShowStringCharacters(false),
        ImageSizeMultipliers{1., 1.} {
    }

    inline StyleBoxOptions(const StyleBoxOptions &options) :
        ShowStringCharacters(options.ShowStringCharacters),
        ImageSizeMultipliers{options.ImageSizeMultipliers[0], options.ImageSizeMultipliers[1]} {
    }

    bool ShowStringCharacters;
    machine_real_t ImageSizeMultipliers[2];
};

class BaseExpression : public Shared<BaseExpression, SharedPool> {
protected:
    const ExtendedType _extended_type;

protected:
	template<typename T>
	friend inline SymbolicFormRef unsafe_symbolic_form(const T &item);

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

	inline ExtendedType extended_type() const {
		return _extended_type; // extended type, e.g. SymbolBlank
	}

public:
    template<typename T>
    inline void set_symbolic_form(const SymEngine::RCP<const T> &ref) const;

	inline void set_no_symbolic_form() const;

	inline bool is_symbolic_form_evaluated() const {
		return m_symbolic_form; // might be active or passive or "no symbolic form"
	}

    inline BaseExpression(ExtendedType type) : _extended_type(type) {
    }

    virtual ~BaseExpression() {
    }

    std::string debug(const Evaluation &evaluation) const;

	virtual std::string debugform() const = 0;

	inline Type type() const {
		// basic type, e.g. MachineIntegerType, SymbolType, ...
		return Type(_extended_type >> CoreTypeShift);
	}

	inline TypeMask type_mask() const;

	inline SymbolName symbol() const {
		return SymbolName(extended_type());
	}

	inline bool is_symbol() const {
		return type() == SymbolType;
	}

	inline bool is_expression() const {
		return type() == ExpressionType;
	}

	inline bool is_machine_integer() const {
		return type() == MachineIntegerType;
	}

	inline bool is_big_integer() const {
		return type() == BigIntegerType;
	}

	inline bool is_machine_real() const {
		return type() == MachineRealType;
	}

	inline bool is_big_real() const {
		return type() == BigRealType;
	}

	inline bool is_machine_complex() const {
		return type() == MachineComplexType;
	}

	inline bool is_big_complex() const {
		return type() == BigComplexType;
	}

	inline bool is_machine_rational() const {
		return type() == MachineRationalType;
	}

	inline bool is_big_rational() const {
		return type() == BigRationalType;
	}

	inline bool is_string() const {
		return type() == StringType;
	}

    inline bool is_non_complex_number() const {
        switch (type()) {
            case MachineIntegerType:
            case BigIntegerType:
            case MachineRealType:
            case BigRealType:
            case MachineRationalType:
            case BigRationalType:
                return true;
            default:
                return false;
        }
    }

	inline bool is_number() const {
		switch (type()) {
			case MachineIntegerType:
			case BigIntegerType:
			case MachineRealType:
			case BigRealType:
			case MachineRationalType:
			case BigRationalType:
			case MachineComplexType:
			case BigComplexType:
				return true;
			default:
				return false;
		}
	}

    virtual bool is_inexact() const {
        return false;
    }

    virtual bool is_negative() const {
        return false;
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const;

	inline const Symbol *lookup_name() const;

	inline optional<hash_t> match_hash() const {
		if (is_expression()) {
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

    virtual tribool equals(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual hash_t hash() const = 0;

	inline BaseExpressionRef evaluate(const Evaluation &evaluation) const;

	inline BaseExpressionRef evaluate_or_copy(const Evaluation &evaluation) const;

    // various getters

    virtual BaseExpressionPtr head(const Symbols &symbols) const = 0;


	inline bool is_sequence() const;

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

	virtual MatchSize match_size() const {
        return MatchSize::exactly(1); // default
    }

    virtual MatchSize string_match_size() const {
        return MatchSize::exactly(0);
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

    virtual double round_to_float() const {
        throw std::runtime_error("not implemented");
    }

    inline bool is_true() const {
        return symbol() == S::True;
    }

	inline bool is_zero() const;

	inline bool is_one() const;

    inline bool is_minus_one() const;

    inline optional<machine_integer_t> get_machine_int_value() const;

    inline optional<Numeric::Z> get_int_value() const;

    inline Symbol *as_symbol() const;

	inline const Expression *as_expression() const;

	inline const String *as_string() const;

	inline bool has_form(
		SymbolName head,
		size_t n_leaves,
		const Evaluation &evaluation) const;

	virtual bool is_numeric() const;

	virtual SortKey sort_key() const;

	virtual SortKey pattern_key() const;

	BaseExpressionRef format(const BaseExpressionRef &form, const Evaluation &evaluation) const;

	virtual BaseExpressionRef custom_format(const BaseExpressionRef &form, const Evaluation &evaluation) const {
		return BaseExpressionRef(this);
	}

    inline BaseExpressionRef custom_format_or_copy(const BaseExpressionRef &form, const Evaluation &evaluation) const {
        return coalesce(custom_format(form, evaluation), BaseExpressionRef(this));
    }

    virtual BaseExpressionRef custom_format_traverse(const BaseExpressionRef &form, const Evaluation &evaluation) const {
        return custom_format(form, evaluation);
    }

	virtual std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
		throw std::runtime_error("boxes_to_text not implemented");
	}

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const {

	    return format(form, evaluation);
    }

	virtual optional<SExp> to_s_exp(optional<machine_integer_t> &n) const {
		return optional<SExp>();
	}

	inline ExpressionRef flatten_sequence() const;
};

#include "heap.h"

template<typename T>
inline void SharedPool::release(const T *obj) {
    ::Pool::release(const_cast<T*>(obj));
}

template<typename T>
inline void BaseExpression::set_symbolic_form(const SymEngine::RCP<const T> &ref) const {
	m_symbolic_form.ensure([&ref] () {
        return Pool::SymbolicForm(
		    SymEngine::rcp_static_cast<const SymEngine::Basic>(ref));
    });
}

inline void BaseExpression::set_no_symbolic_form() const {
	m_symbolic_form.ensure([] () {
		return Pool::NoSymbolicForm();
	});
}

#include "core/slice/slice.h"
#include "core/pattern/arguments.h"
#include "cache.h"

inline CacheRef Pool::new_cache() {
	return CacheRef(_s_instance->_caches.construct());
}

inline void Pool::release(Cache *cache) {
	_s_instance->_caches.destroy(cache);
}

#include "core/expression/interface.h"

template<typename T>
inline SymbolicFormRef unsafe_symbolic_form(const T &item) {
    // caller must handle SymEngine::SymEngineException; non-core code should always
    // call symbolic_form(item, evaluation).

    return SymbolicFormRef(item->m_symbolic_form.ensure([&item] () {
        const auto form = item->instantiate_symbolic_form();
#if DEBUG_SYMBOLIC
	    if (form && !form->is_none()) {
		    std::cout << "sym form " << form->get()->__str__() << std::endl;
	    }
#endif
	    return form;
    }));
}

template<>
inline SymbolicFormRef unsafe_symbolic_form<const ExpressionPtr&>(const ExpressionPtr& expr) {
    // caller must handle SymEngine::SymEngineException; non-core code should always
    // call symbolic_form(item, evaluation).

    return SymbolicFormRef(expr->m_symbolic_form.ensure([] () {
        return Pool::NoSymbolicForm();
    }));
}

#include "rule.h"

inline const Expression *BaseExpression::as_expression() const {
	return static_cast<const Expression*>(this);
}

inline bool BaseExpression::is_sequence() const {
	return type() == ExpressionType && as_expression()->head()->symbol() == S::Sequence;
}

inline TypeMask BaseExpression::type_mask() const {
    const Type t = type();
    TypeMask mask = ((TypeMask)1) << t;
    if (t == ExpressionType &&
        as_expression()->head()->symbol() == S::Sequence) {
        mask |= TypeMaskSequence;
    }
    return mask;
}

#endif
