// @formatter:off

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "types.h"
#include "hash.h"
#include "symbol.h"
#include "string.h"
#include "leaves.h"

#include <sstream>
#include <vector>
#include <mutex>

#include <symengine/add.h>
#include <symengine/mul.h>
#include <symengine/pow.h>

template<typename Slice>
class ExpressionImplementation : public Expression {

protected:
	virtual BaseExpressionRef materialize_leaf(size_t i) const {
		return _leaves[i];
	}

	virtual TypeMask materialize_type_mask() const {
		return type_mask();
	}

	virtual TypeMask materialize_exact_type_mask() const {
		return _leaves.exact_type_mask();
	}

public:
	virtual ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const final;

public:
	const Slice _leaves;

	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &_leaves), _leaves(slice) {
		assert(head);
	}

	inline ExpressionImplementation(const BaseExpressionRef &head) :
		Expression(head, Slice::code(), &_leaves), _leaves() {
		assert(head);
	}

	inline ExpressionImplementation(ExpressionImplementation<Slice> &&expr) :
		Expression(expr._head, Slice::code(), &_leaves), _leaves(expr.leaves) {
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FSGenerator<F> &generator) :
		Expression(head, Slice::code(), &_leaves), _leaves(generator) {
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FPGenerator<F> &generator) :
		Expression(head, Slice::code(), &_leaves), _leaves(generator) {
	}

    inline const auto &slice() const {
        return _leaves;
    }

	inline auto leaves() const {
		return _leaves.leaves();
	}

	template<typename T>
	inline auto primitives() const {
		return _leaves.template primitives<T>();
	}

	inline TypeMask type_mask() const {
		return _leaves.type_mask();
	}

	inline TypeMask exact_type_mask() const {
		return _leaves.exact_type_mask();
	}

	inline void init_type_mask(TypeMask type_mask) const {
		_leaves.init_type_mask(type_mask);
	}

	inline constexpr SliceCode slice_code() const {
		return Slice::code();
	}

	virtual inline bool same(const BaseExpression &item) const final {
		if (this == &item) {
			return true;
		}
		if (item.type() != ExpressionType) {
			return false;
		}
		const Expression *expr = item.as_expression();

		if (!_head->same(expr->head())) {
			return false;
		}

		const size_t size = _leaves.size();
		if (size != expr->size()) {
			return false;
		}

		const auto &self = _leaves;
		return expr->with_slice([&self, size] (const auto &slice) {

			for (size_t i = 0; i < size; i++) {
				if (!self[i]->same(slice[i])) {
					return false;
				}
			}

			return true;
		});
	}

    virtual inline tribool equals(const BaseExpression &item) const final {
		if (this == &item) {
			return true;
		}
		if (item.type() != ExpressionType) {
			return false;
		}
		const Expression *expr = item.as_expression();

		const size_t size = _leaves.size();
		if (size != expr->size()) {
			return false;
		}

		const tribool head = _head->equals(*expr->head());
        if (head != true) {
            return head;
        }

		const auto &self = _leaves;
		return expr->with_slice([&self, size] (const auto &slice) {

			for (size_t i = 0; i < size; i++) {
                const tribool leaf = self[i]->same(slice[i]);
				if (leaf != true) {
					return leaf;
				}
			}

			return tribool(true);
		});
	}

    /*virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final {
        switch (_head->extended_type()) {
            case SymbolFullForm:
            case SymbolStandardForm:
                if (_leaves.size() == 1) {
                    return static_leaves<1>()[0]->format(
                        SymbolRef(_head->as_symbol()), evaluation);
                }
                break;

            case SymbolHoldForm:
                if (_leaves.size() == 1) {
                    return static_leaves<1>()[0]->format(form, evaluation);
                }
                break;

            default:
                break; // ignore
        }

        std::stringstream result;

        // format head
        result << _head->format(form, evaluation);
        result << "[";

        // format leaves
        const size_t argc = _leaves.size();

        for (size_t i = 0; i < argc; i++) {
            result << _leaves[i]->format(form, evaluation);

            if (i < argc - 1) {
                result << ", ";
            }
        }
        result << "]";

        return result.str();
    }*/

	virtual BaseExpressionRef evaluate_expression_with_non_symbol_head(
		const Evaluation &evaluation) const {

		// Step 4
		// Apply SubValues
		if (_head->type() == ExpressionType) {
			const BaseExpression * const head_head =
				static_cast<const Expression*>(_head.get())->_head.get();

			if (head_head->type() == SymbolType) {
				const Symbol * const head_symbol = static_cast<const Symbol *>(head_head);

				const SymbolRules * const rules = head_symbol->state().rules();

				if (rules) {
					const BaseExpressionRef sub_form =
						rules->sub_rules.apply(this, evaluation);

					if (sub_form) {
						return sub_form;
					}
				}
			}
		}

		return BaseExpressionRef();
	}

	virtual BaseExpressionRef replace_all(const MatchRef &match, const Evaluation &evaluation) const;

	virtual BaseExpressionRef clone() const;

	virtual ExpressionRef clone(const BaseExpressionRef &head) const;

	virtual hash_t hash() const final {
		hash_t result = hash_combine(_leaves.size(), _head->hash());
		for (const auto &leaf : _leaves.leaves()) {
			result = hash_combine(result, leaf->hash());
		}
		return result;
	}

	virtual optional<hash_t> compute_match_hash() const final {
		switch (_head->extended_type()) {
			case SymbolBlank:
			case SymbolBlankSequence:
			case SymbolBlankNullSequence:
			case SymbolPattern:
			case SymbolAlternatives:
			case SymbolRepeated:
            case SymbolExcept:
				return optional<hash_t>();

			default: {
				// note that this must yield the same value as hash()
				// defined above if this Expression is not a pattern.

				const auto head_hash = _head->match_hash();
				if (!head_hash) {
					return optional<hash_t>();
				}

				hash_t result = hash_combine(_leaves.size(), *head_hash);
				for (const auto &leaf : _leaves.leaves()) {
					const auto leaf_hash = leaf->match_hash();
					if (!leaf_hash) {
						return optional<hash_t>();
					}
					result = hash_combine(result, *leaf_hash);
				}
				return result;
			}
		}
	}

	virtual MatchSize match_size() const final {
		switch (_head->extended_type()) {
			case SymbolBlank:
				return MatchSize::exactly(1);

			case SymbolBlankSequence:
				return MatchSize::at_least(1);

			case SymbolBlankNullSequence:
				return MatchSize::at_least(0);

			case SymbolPattern:
				if (size() == 2) {
					// Pattern is only valid with two arguments
					return _leaves[1]->match_size();
				} else {
					return MatchSize::exactly(1);
				}

			case SymbolAlternatives: {
				const size_t n = size();

				if (n == 0) {
					return MatchSize::exactly(1); // FIXME
				}

				const MatchSize size = _leaves[0]->match_size();
				match_size_t min_p = size.min();
				match_size_t max_p = size.max();

				for (size_t i = 1; i < n; i++) {
					const MatchSize leaf_size = _leaves[i]->match_size();
					max_p = std::max(max_p, leaf_size.max());
					min_p = std::min(min_p, leaf_size.min());
				}

				return MatchSize::between(min_p, max_p);
			}

			case SymbolRepeated:
				switch(size()) {
					case 1:
						return MatchSize::at_least(1);
					case 2:
						// TODO inspect second arg
						return MatchSize::at_least(1);
					default:
						return MatchSize::exactly(1);
				}

            case SymbolExcept:
                return MatchSize::at_least(0);

			case SymbolOptional:
				return MatchSize::at_least(0);

			default:
				return MatchSize::exactly(1);
		}
	}

	virtual MatchSize leaf_match_size() const final {
		MatchSize size = MatchSize::exactly(0);
		for (const auto &leaf : _leaves.leaves()) {
			size += leaf->match_size();
		}
		return size;
	}

	virtual DynamicExpressionRef to_dynamic_expression(const BaseExpressionRef &self) const;

	virtual const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const;

	virtual bool is_numeric() const final {
		if (head()->type() == SymbolType &&
		    head()->as_symbol()->state().has_attributes(Attributes::NumericFunction)) {

			const size_t n = size();
			for (size_t i = 0; i < n; i++) {
				if (!_leaves[i]->is_numeric())
					return false;
			}

			return true;
		} else {
			return false;
		}
	}

	virtual SortKey sort_key() const final {
		MonomialMap m(Pool::monomial_map_allocator());

		switch (_head->extended_type()) {
			case SymbolTimes: {
				const size_t n = size();

				for (size_t i = 0; i < n; i++) {
					const BaseExpressionRef leaf = _leaves[i];

					if (leaf->type() == ExpressionType &&
						leaf->as_expression()->head()->extended_type() == SymbolPower &&
						leaf->as_expression()->size() == 2) {

						const BaseExpressionRef * const power =
							leaf->as_expression()->static_leaves<2>();

						const BaseExpressionRef &var = power[0];
						const BaseExpressionRef &exp = power[1];

						if (var->type() == SymbolType) {
							// FIXME
						}
					} else if (leaf->type() == SymbolType) {
						increment_monomial(m, leaf->as_symbol(), 1);
					}
				}

				break;
			}

			default: {
				break;
			}
		}

		if (!m.empty()) {
			return SortKey(is_numeric() ? 1 : 2, 2, std::move(m), 1, SortByHead(this), SortByLeaves(this), 1);
		} else {
			return SortKey(is_numeric() ? 1 : 2, 3, SortByHead(this), SortByLeaves(this), 1);
		}
	}

	virtual SortKey pattern_key() const final {
		switch (_head->extended_type()) {
			case SymbolBlank:
				return blank_sort_key(1, size() > 0, this);
			case SymbolBlankSequence:
				return blank_sort_key(2, size() > 0, this);
			case SymbolBlankNullSequence:
				return blank_sort_key(3, size() > 0, this);
			case SymbolPatternTest:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.set_pattern_test(0);
					return key;
				}
			case SymbolCondition:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.set_condition(0);
					return key;
				}
			case SymbolPattern:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = _leaves[1]->pattern_key();
					key.set_pattern_test(0);
					return key;
				}
			case SymbolOptional:
				if (size() < 1 || size() > 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = _leaves[0]->pattern_key();
					key.set_optional(1);
					return key;
				}
			case SymbolAlternatives:
				// FIXME
			case SymbolVerbatim:
				// FIXME
			case SymbolOptionsPattern:
				// FIXME
			default: {
				return SortKey(2, 0, 1, 1, 0, SortByHead(this, true), SortByLeaves(this, true, true), 1);
			}
		}
	}

    template<typename Compute, typename Recurse>
    BaseExpressionRef do_symbolic(
        const Compute &compute,
        const Recurse &recurse,
        const Evaluation &evaluation) const;

	virtual BaseExpressionRef expand(const Evaluation &evaluation) const final {
        return do_symbolic(
            [] (const SymbolicFormRef &form) {
                const SymEngineRef new_form = SymEngine::expand(form->get());
                if (new_form.get() != form->get().get()) {
                    return Pool::SymbolicForm(new_form);
                } else {
                    return Pool::SymbolicForm(SymEngineRef());
                }
            },
            [] (const BaseExpressionRef &leaf, const Evaluation &evaluation) {
                return leaf->expand(evaluation);
            },
            evaluation);
	}

	virtual std::tuple<bool, UnsafeExpressionRef> thread(const Evaluation &evaluation) const;

	template<enum Type... Types, typename F>
	inline ExpressionRef conditional_map(
		const F &f, const Evaluation &evaluation) const;

	template<enum Type... Types, typename F>
	inline ExpressionRef conditional_map(
		const BaseExpressionRef &head, const F &f, const Evaluation &evaluation) const;

	template<typename F>
	inline ExpressionRef conditional_map_all(
		const BaseExpressionRef &head, const F &f, const Evaluation &evaluation) const;

    virtual BaseExpressionRef custom_format(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual std::string boxes_to_text(const Evaluation &evaluation) const {
        switch (head()->extended_type()) {
	        case SymbolStyleBox: {
				// FIXME parse StyleBox options like System`ShowStringCharacters
		        if (size() < 1) {
			        break;
		        }
		        return _leaves[0]->boxes_to_text(evaluation);
	        }

            case SymbolRowBox: {
                if (size() != 1) {
                    break;
                }

                const BaseExpressionRef &list = static_leaves<1>()[0];
                if (list->type() != ExpressionType) {
                    break;
                }
                if (list->as_expression()->head()->extended_type() != SymbolList) {
                    break;
                }

                std::ostringstream s;
                list->as_expression()->with_slice([&s, &evaluation] (const auto &slice) {
                    const size_t n = slice.size();
                    for (size_t i = 0; i < n; i++) {
                        s << slice[i]->boxes_to_text(evaluation);
                    }
                });

                return s.str();
            }

            case SymbolSuperscriptBox: {
                if (size() != 2) {
                    break;
                }

                const BaseExpressionRef * const leaves = static_leaves<2>();

                std::ostringstream s;
                s << leaves[0]->boxes_to_text(evaluation) << "^" << leaves[1]->boxes_to_text(evaluation);
                return s.str();
            }

            default:
                break;
        }

        throw std::runtime_error("box error");
    }

    virtual bool is_negative() const final {
        if (head()->extended_type() == SymbolTimes && size() >= 1) {
            return _leaves[0]->is_negative();
        } else {
            return false;
        }
    }

    virtual inline BaseExpressionRef negate(const Evaluation &evaluation) const final;

	virtual std::string debugform() const {
		std::ostringstream s;
		s << head()->debugform();
		s << "[";
		for (size_t i = 0; i < size(); i++) {
			if (i > 0) {
				s << ", ";
			}
			s << _leaves[i]->debugform();
		}
		s << "]";
		return s.str();
	}
};

#include "leaves.tcc"

template<size_t N>
inline const BaseExpressionRef *Expression::static_leaves() const {
    static_assert(N <= MaxStaticSliceSize, "N is too large");
    return static_cast<const StaticSlice<N>*>(_slice_ptr)->refs();
}

template<typename E, typename T>
inline std::vector<T> collect(const LeafVector &leaves) {
	std::vector<T> values;
	values.reserve(leaves.size());
	for (const auto &leaf : leaves) {
		values.push_back(static_cast<const E*>(leaf.get())->value);
	}
	return values;
}

inline ExpressionRef non_static_expression(
	const BaseExpressionRef &head,
	LeafVector &&leaves) {

	if (leaves.size() < MinPackedSliceSize) {
		return Pool::Expression(head, DynamicSlice(std::move(leaves)));
	} else {
		switch (leaves.type_mask()) {
			case make_type_mask(MachineIntegerType):
				return Pool::Expression(head, PackedSlice<machine_integer_t>(
					collect<MachineInteger, machine_integer_t>(leaves)));
			case make_type_mask(MachineRealType):
				return Pool::Expression(head, PackedSlice<machine_real_t>(
					collect<MachineReal, machine_real_t>(leaves)));
			default:
				return Pool::Expression(head, DynamicSlice(std::move(leaves)));
		}
	}
}

template<typename G>
typename std::enable_if<std::is_base_of<FGenerator, G>::value, ExpressionRef>::type
expression(
	const BaseExpressionRef &head,
	const G &generator) {

	if (generator.size() <= MaxStaticSliceSize) {
		return Pool::StaticExpression(head, generator);
	} else {
		return non_static_expression(head, generator.vector());
	}
}

inline ExpressionRef
expression(
	const BaseExpressionRef &head,
	LeafVector &&leaves) {

	const size_t n = leaves.size();
	if (n <= MaxStaticSliceSize) {
		return Pool::StaticExpression(head, sequential([&leaves, n] (auto &store) {
			for (size_t i = 0; i < n; i++) {
				store(leaves.unsafe_grab_leaf(i));
			}
		}, n));
	} else {
		return non_static_expression(head, std::move(leaves));
	}}


template<typename G>
typename std::enable_if<std::is_base_of<VGenerator, G>::value, ExpressionRef>::type
expression(
	const BaseExpressionRef &head,
	const G &generator) {

	return expression(head, generator.vector());
}

inline ExpressionRef expression(
	const BaseExpressionRef &head) {

	return Pool::EmptyExpression(head);
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a) {

    return Pool::StaticExpression<1>(head, StaticSlice<1>({a}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b) {

    return Pool::StaticExpression<2>(head, StaticSlice<2>({a, b}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b,
    const BaseExpressionRef &c) {

    return Pool::StaticExpression<3>(head, StaticSlice<3>({a, b, c}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b,
    const BaseExpressionRef &c,
    const BaseExpressionRef &d) {

    return Pool::StaticExpression<4>(head, StaticSlice<4>({a, b, c, d}));
}

inline ExpressionRef expression(
	const BaseExpressionRef &head,
	const std::initializer_list<BaseExpressionRef> &leaves) {

	if (leaves.size() <= MaxStaticSliceSize) {
		return Pool::StaticExpression(head, sequential([&leaves] (auto &store) {
			for (const BaseExpressionRef &leaf : leaves) {
				store(BaseExpressionRef(leaf));
			}
		}, leaves.size()));
	} else {
		return Pool::Expression(head, DynamicSlice(leaves, UnknownTypeMask));
	}
}

template<typename U>
inline PackedExpressionRef<U> expression(const BaseExpressionRef &head, PackedSlice<U> &&slice) {
	return Pool::Expression(head, slice);
}

inline DynamicExpressionRef expression(const BaseExpressionRef &head, DynamicSlice &&slice) {
	return Pool::Expression(head, slice);
}

template<int N>
inline StaticExpressionRef<N> expression(const BaseExpressionRef &head, StaticSlice<N> &&slice) {
    return Pool::StaticExpression<N>(head, std::move(slice));
}

inline ExpressionRef expression(const BaseExpressionRef &head, const ArraySlice &slice) {
	return slice.clone(head);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const VCallSlice &slice) {
	return slice.clone(head);
}

template<typename Slice>
inline ExpressionRef ExpressionImplementation<Slice>::slice(
	const BaseExpressionRef &head, index_t begin, index_t end) const {

	const index_t size = index_t(_leaves.size());

	if (begin < 0) {
		begin = size - (-begin % size);
	}
	if (end < 0) {
		end = size - (-end % size);
	}

	end = std::min(end, size);
	begin = std::min(begin, end);

	const size_t new_size = end - begin;
	constexpr SliceCode slice_code = Slice::code();

    if (is_packed_slice(slice_code) && new_size >= MinPackedSliceSize) {
        return expression(head, _leaves.slice(begin, end));
    } else if (slice_code == DynamicSliceCode && new_size > MaxStaticSliceSize) {
		return expression(head, _leaves.slice(begin, end));
	} else {
		const Slice &slice = _leaves;

		const auto generate_leaves = [begin, end, &head, &slice] (auto &store) {
			for (size_t i = begin; i < end; i++) {
				store(BaseExpressionRef(slice[i]));
			}
		};

		return expression(head, sequential(generate_leaves, new_size));
	}
}

template<typename Slice>
DynamicExpressionRef ExpressionImplementation<Slice>::to_dynamic_expression(const BaseExpressionRef &self) const {
	if (std::is_same<Slice, DynamicSlice>()) {
		return static_pointer_cast<DynamicExpression>(self);
	} else {
		LeafVector leaves;
		leaves.reserve(_leaves.size());
		for (auto leaf : _leaves.leaves()) {
			leaves.push_back(BaseExpressionRef(leaf));
		}
		return Pool::Expression(_head, DynamicSlice(std::move(leaves)));

	}
}

#include "definitions.h"
#include "evaluation.h"

#include "evaluate.h"

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::replace_all(
	const MatchRef &match, const Evaluation &evaluation) const {

	const BaseExpressionRef &old_head = _head;
	const BaseExpressionRef new_head = old_head->replace_all(match);

	return conditional_map<ExpressionType, SymbolType>(
		new_head ? new_head : old_head,
		[&match] (const BaseExpressionRef &leaf) {
			return leaf->replace_all(match);
		},
		evaluation);
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::clone() const {
	return expression(_head, Slice(_leaves));
}

template<typename Slice>
ExpressionRef ExpressionImplementation<Slice>::clone(const BaseExpressionRef &head) const {
	return expression(head, Slice(_leaves));
}

template<typename Slice>
const BaseExpressionRef *ExpressionImplementation<Slice>::materialize(UnsafeBaseExpressionRef &materialized) const {
	auto expr = expression(_head, _leaves.unpack());
	materialized = expr;
	return expr->_leaves.refs();
}

inline std::string message_text(
    const Evaluation &evaluation,
    std::string &&text,
    size_t index) {

    return text;
}

template<typename... Args>
std::string message_text(
    const Evaluation &evaluation,
    std::string &&text,
    size_t index,
    const BaseExpressionRef &arg,
    Args... args) {

    std::string new_text(text);
    const std::string placeholder(message_placeholder(index));

    const auto pos = new_text.find(placeholder);
    if (pos != std::string::npos) {
        new_text = new_text.replace(pos, placeholder.length(), evaluation.format_output(arg));
    }

    return message_text(evaluation, std::move(new_text), index + 1, args...);
}

template<typename... Args>
void Evaluation::message(const SymbolRef &name, const char *tag, const Args&... args) const {
	const auto &symbols = definitions.symbols();

	const BaseExpressionRef tag_str = Pool::String(std::string(tag));

	const ExpressionRef message = expression(
		symbols.MessageName, name, tag_str);
	UnsafeStringRef text_template = static_cast<const Symbol*>(
		name.get())->lookup_message(message.get(), *this);

	if (!text_template) {
		const ExpressionRef general_message = expression(
			symbols.MessageName, symbols.General, tag_str);

		text_template = symbols.General->lookup_message(general_message.get(), *this);
	}

	if (text_template) {
		std::string text(text_template->utf8());
        std::unique_lock<std::mutex> lock(m_output_mutex);
        m_output->write(name->short_name(), tag, message_text(*this, std::move(text), 1, args...));
	}
}

template<typename Slice>
template<typename Compute, typename Recurse>
BaseExpressionRef ExpressionImplementation<Slice>::do_symbolic(
	const Compute &compute,
	const Recurse &recurse,
	const Evaluation &evaluation) const {

	const SymbolicFormRef form = unsafe_symbolic_form(this);

	if (form && !form->is_none()) {
		const SymbolicFormRef new_form = compute(form);
		if (new_form && !new_form->is_none()) {
			return from_symbolic_form(new_form->get(), evaluation);
		} else {
			return BaseExpressionRef();
		}
	} else {
		return conditional_map<ExpressionType>(
			[&recurse, &evaluation] (const BaseExpressionRef &leaf) {
				return recurse(leaf, evaluation);
			},
			evaluation);
	}
}

class RuleForm {
private:
	const BaseExpressionRef *m_leaves;

public:
	// note that the scope of "item" must contain that of
	// RuleForm, i.e. item must be referenced at least as
	// long as RuleForm lives.
	inline RuleForm(BaseExpressionPtr item) {
		if (item->type() != ExpressionType) {
			m_leaves = nullptr;
		} else {
			const auto expr = item->as_expression();

			if (expr->size() != 2) {
				m_leaves = nullptr;
			} else {
				switch (expr->head()->extended_type()) {
					case SymbolRule:
					case SymbolRuleDelayed:
						m_leaves = expr->static_leaves<2>();
						break;
					default:
						m_leaves = nullptr;
						break;
				}
			}
		}
	}

	inline bool is_rule() const {
		return m_leaves;
	}

	inline const BaseExpressionRef &left_side() const {
		return m_leaves[0];
	}

	inline const BaseExpressionRef &right_side() const {
		return m_leaves[1];
	}
};

template<typename Arguments>
std::vector<const RewriteBaseExpression> RewriteExpression::nodes(
	Arguments &arguments,
	const Expression *body) {

	return body->with_slice([&arguments] (const auto &slice) {
		std::vector<const RewriteBaseExpression> refs;
		const size_t n = slice.size();
		refs.reserve(n);
		for (size_t i = 0; i < n; i++) {
			refs.emplace_back(RewriteBaseExpression::construct(arguments, slice[i]));
		}
		return refs;
	});
}

template<typename Arguments>
RewriteExpression::RewriteExpression(
	Arguments &arguments,
	const Expression *body) :

	m_head(RewriteBaseExpression::construct(arguments, body->head())),
	m_leaves(nodes(arguments, body)) {
}

template<typename Arguments>
inline BaseExpressionRef RewriteExpression::rewrite_or_copy(
	const Expression *body,
	const Arguments &args) const {

	const auto &head = m_head;
	const auto &leaves = m_leaves;

	return body->with_slice<CompileToSliceType>(
		[body, &head, &leaves, &args] (const auto &slice) {
			const auto generate = [&slice, &leaves, &args] (auto &store) {
				const size_t n = slice.size();
				for (size_t i = 0; i < n; i++) {
					store(leaves[i].rewrite_or_copy(slice[i], args));
				}
			};

			return ExpressionRef(expression(
				head.rewrite_or_copy(body->head(), args),
				slice.create(generate, slice.size())));
		});
}

template<typename Slice>
std::tuple<bool, UnsafeExpressionRef> ExpressionImplementation<Slice>::thread(const Evaluation &evaluation) const {
	const size_t size = this->size();
	const auto &leaves = this->leaves();

    const auto is_threadable = [] (const BaseExpressionRef &leaf) {
		return (leaf->type() == ExpressionType &&
            leaf->as_expression()->head()->extended_type() == SymbolList);
    };

    // preflight.

    bool can_thread = false;
    for (size_t i = 0; i < size; i++) {
		if (is_threadable(leaves[i])) {
            can_thread = true;
            break;
        }
    }

    if (!can_thread) {
		return std::make_tuple(false, UnsafeExpressionRef(this));
    }

    // actual threading.

	index_t dim = -1;

	LeafVector items;
	std::vector<LeafVector> dim_items;

	for (size_t i = 0; i < size; i++) {
		const BaseExpressionRef &leaf = leaves[i];
		if (is_threadable(leaf)) {
			const Expression * const expr = leaf->as_expression();

			if (dim < 0) {
				dim = expr->size();

				expr->with_slice([&items, &dim_items] (const auto &slice) {
					const size_t size = slice.size();
					for (index_t j = 0; j < size; j++) {
						LeafVector element;
						element.reserve(items.size() + 1);
						for (const auto &item : items) {
							element.push_back_copy(item);
						}
						element.push_back_copy(slice[j]);
						dim_items.push_back(std::move(element));
					}
				});

			} else if (dim != expr->size()) {
                // evaluation.message("Thread", "tdlen");
                return std::make_tuple(true, UnsafeExpressionRef(this));
            } else {
                expr->with_slice([&dim_items] (const auto &slice) {
                    const size_t size = slice.size();
                    for (index_t j = 0; j < size; j++) {
                        dim_items[j].push_back_copy(slice[j]);
                    }
                });
            }
		} else {
			if (dim < 0) {
				items.push_back_copy(leaf);
			} else {
				for (auto &item : dim_items) {
					item.push_back_copy(leaf);
				}
			}
		}
	}

	if (dim < 0) {
		return std::make_tuple(false, UnsafeExpressionRef(this));
	} else {
		const BaseExpressionRef &head = _head;

		return std::make_tuple(true, expression(
			evaluation.List, sequential([&head, &dim_items] (auto &store) {
				for (auto &items : dim_items) {
					store(expression(head, std::move(items)));
				}
			})));
	}
}

inline BaseExpressionRef apply_symengine(
	const SymEngineUnaryFunction &f,
	const BaseExpressionPtr a,
	const Evaluation &evaluation) {

	const SymbolicFormRef symbolic_a = symbolic_form(a, evaluation);
	if (!symbolic_a->is_none()) {
		return from_symbolic_form(f(symbolic_a->get()), evaluation);
	}

	return BaseExpressionRef();
}

inline BaseExpressionRef apply_symengine(
	const SymEngineBinaryFunction &f,
	const BaseExpressionPtr a,
	const BaseExpressionPtr b,
	const Evaluation &evaluation) {

	const SymbolicFormRef symbolic_a = symbolic_form(a, evaluation);
	if (!symbolic_a->is_none()) {
		const SymbolicFormRef symbolic_b = symbolic_form(b, evaluation);
		if (!symbolic_b->is_none()) {
			return from_symbolic_form(f(symbolic_a->get(), symbolic_b->get()), evaluation);
		}
	}

	return BaseExpressionRef();
}

template<typename Slice>
inline BaseExpressionRef ExpressionImplementation<Slice>::negate(const Evaluation &evaluation) const {
    if (head()->extended_type() == SymbolTimes && size() >= 1) {
        const BaseExpressionRef &leaf = _leaves[0];
        if (leaf->is_number()) {
            const BaseExpressionRef negated = leaf->negate(evaluation);
            if (negated->is_one()) {
                if (size() == 1) {
                    return negated;
                } else {
                    return slice(evaluation.Times, 1);
                }
            } else {
                return expression(evaluation.Times, sequential(
                    [this, &negated] (auto &store) {
                        store(BaseExpressionRef(negated));
                        const size_t n = size();
                        for (size_t i = 0; i < n; i++) {
                            store(BaseExpressionRef(_leaves[i]));
                        }
                    }, 1 + size()));
            }
        } else {
            return expression(evaluation.Times, evaluation.minus_one, leaf);
        }
    } else {
        return BaseExpression::negate(evaluation);
    }
}

inline ExpressionRef TempVector::to_list(const Evaluation &evaluation) const {
    const auto &v = *this;
    return expression(evaluation.List, sequential([&v] (auto &store) {
        const size_t n = v.size();
        for (size_t i = 0; i < n; i++) {
            store(BaseExpressionRef(v[i]));
        }
    }, size()));
}

inline ExpressionRef Expression::flatten_sequence() const {
	return with_slice([this] (const auto &slice) -> ExpressionRef {
		if ((slice.type_mask() & make_type_mask(ExpressionType)) == 0) {
			return ExpressionRef();
		}

		index_t first = -1;

		const size_t n = slice.size();
		for (size_t i = 0; i < n; i++) {
			if (slice[i]->is_sequence()) {
				first = i;
				break;
			}
		}

		if (first >= 0) {
			LeafVector v;

			for (size_t i = 0; i < first; i++) {
				v.push_back_copy(slice[i]);
			}

			for (size_t i = first; i < n; i++) {
				BaseExpressionRef leaf = slice[i];
				if (leaf->is_sequence()) {
					leaf->as_expression()->with_slice([&v] (const auto &sequence) {
						const size_t n = sequence.size();
						for (size_t i = 0; i < n; i++) {
							v.push_back_copy(sequence[i]);
						}
					});
				} else {
					v.push_back(std::move(leaf));
				}
			}

			return expression(head(), std::move(v));
		}

		return ExpressionRef();
	});
}

inline ExpressionRef BaseExpression::flatten_sequence() const {
	if (type() == ExpressionType) {
		return as_expression()->flatten_sequence();
	} else {
		return ExpressionRef();
	}
}

#endif
