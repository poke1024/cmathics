// @formatter:off

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "core/types.h"
#include "core/hash.h"
#include "core/atoms/symbol.h"
#include "core/atoms/string.h"

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
		return m_slice[i];
	}

	virtual TypeMask materialize_type_mask() const {
		return type_mask();
	}

	virtual TypeMask materialize_exact_type_mask() const {
		return m_slice.exact_type_mask();
	}

public:
	virtual ExpressionRef slice(const BaseExpressionRef &head, index_t begin, index_t end = INDEX_MAX) const final;

public:
	const Slice m_slice;

	inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &m_slice), m_slice(slice) {
		assert(head);
	}

	inline ExpressionImplementation(const BaseExpressionRef &head) :
		Expression(head, Slice::code(), &m_slice), m_slice() {
		assert(head);
	}

	inline ExpressionImplementation(ExpressionImplementation<Slice> &&expr) :
		Expression(expr._head, Slice::code(), &m_slice), m_slice(expr.leaves) {
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FSGenerator<F> &generator) :
		Expression(head, Slice::code(), &m_slice), m_slice(generator) {
	}

	template<typename F>
	inline ExpressionImplementation(const BaseExpressionRef &head, const FPGenerator<F> &generator) :
		Expression(head, Slice::code(), &m_slice), m_slice(generator) {
	}

    inline const auto &slice() const {
        return m_slice;
    }

	inline auto leaves() const {
		return m_slice.leaves();
	}

	template<typename T>
	inline auto primitives() const {
		return m_slice.template primitives<T>();
	}

	inline TypeMask type_mask() const {
		return m_slice.type_mask();
	}

	inline TypeMask exact_type_mask() const {
		return m_slice.exact_type_mask();
	}

	inline void init_type_mask(TypeMask type_mask) const {
		m_slice.init_type_mask(type_mask);
	}

	inline constexpr SliceCode slice_code() const {
		return Slice::code();
	}

	virtual inline bool same(const BaseExpression &item) const final {
		if (this == &item) {
			return true;
		}
		if (!item.is_expression()) {
			return false;
		}
		const Expression *expr = item.as_expression();

		if (!_head->same(expr->head())) {
			return false;
		}

		const size_t size = m_slice.size();
		if (size != expr->size()) {
			return false;
		}

		const auto &self = m_slice;
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
		if (!item.is_expression()) {
			return false;
		}
		const Expression *expr = item.as_expression();

		const size_t size = m_slice.size();
		if (size != expr->size()) {
			return false;
		}

		const tribool head = _head->equals(*expr->head());
        if (head != true) {
            return head;
        }

		const auto &self = m_slice;
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
                if (m_slice.size() == 1) {
                    return n_leaves<1>()[0]->format(
                        SymbolRef(_head->as_symbol()), evaluation);
                }
                break;

            case SymbolHoldForm:
                if (m_slice.size() == 1) {
                    return n_leaves<1>()[0]->format(form, evaluation);
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
        const size_t argc = m_slice.size();

        for (size_t i = 0; i < argc; i++) {
            result << m_slice[i]->format(form, evaluation);

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
		if (_head->is_expression()) {
			const BaseExpression * const head_head =
				_head->as_expression()->_head.get();

			if (head_head->is_symbol()) {
				const Symbol * const head_symbol = head_head->as_symbol();

				const SymbolRules * const rules = head_symbol->state().rules();

				if (rules) {
					const optional<BaseExpressionRef> sub_form =
						rules->sub_rules.apply(this, evaluation);

					if (sub_form) {
						return *sub_form;
					}
				}
			}
		}

		return BaseExpressionRef();
	}

	virtual BaseExpressionRef replace_all(const MatchRef &match, const Evaluation &evaluation) const;

	virtual BaseExpressionRef replace_all(const ArgumentsMap &replacement, const Evaluation &evaluation) const;

	virtual BaseExpressionRef clone() const;

	virtual ExpressionRef clone(const BaseExpressionRef &head) const;

	virtual hash_t hash() const final {
		hash_t result = hash_combine(m_slice.size(), _head->hash());
		for (const auto &leaf : m_slice.leaves()) {
			result = hash_combine(result, leaf->hash());
		}
		return result;
	}

	virtual optional<hash_t> compute_match_hash() const final {
		switch (_head->symbol()) {
			case S::Blank:
			case S::BlankSequence:
			case S::BlankNullSequence:
			case S::Pattern:
			case S::Alternatives:
			case S::Repeated:
            case S::Except:
            case S::OptionsPattern:
				return optional<hash_t>();

			default: {
				// note that this must yield the same value as hash()
				// defined above if this Expression is not a pattern.

				const auto head_hash = _head->match_hash();
				if (!head_hash) {
					return optional<hash_t>();
				}

				hash_t result = hash_combine(m_slice.size(), *head_hash);
				for (const auto &leaf : m_slice.leaves()) {
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
		switch (_head->symbol()) {
			case S::Blank:
				return MatchSize::exactly(1);

			case S::BlankSequence:
				return MatchSize::at_least(1);

			case S::BlankNullSequence:
				return MatchSize::at_least(0);

            case S::OptionsPattern:
				return MatchSize::at_least(0);

			case S::Pattern:
				if (size() == 2) {
					// Pattern is only valid with two arguments
					return m_slice[1]->match_size();
				} else {
					return MatchSize::exactly(1);
				}

			case S::Alternatives: {
				const size_t n = size();

				if (n == 0) {
					return MatchSize::exactly(1); // FIXME
				}

				const MatchSize size = m_slice[0]->match_size();
				match_size_t min_p = size.min();
				match_size_t max_p = size.max();

				for (size_t i = 1; i < n; i++) {
					const MatchSize leaf_size = m_slice[i]->match_size();
					max_p = std::max(max_p, leaf_size.max());
					min_p = std::min(min_p, leaf_size.min());
				}

				return MatchSize::between(min_p, max_p);
			}

			case S::Repeated:
				switch(size()) {
					case 1:
						return MatchSize::at_least(1);
					case 2:
						// TODO inspect second arg
						return MatchSize::at_least(1);
					default:
						return MatchSize::exactly(1);
				}

            case S::Except:
                return MatchSize::at_least(0);

			case S::Optional:
				return MatchSize::at_least(0);

			case S::Shortest:
			case S::Longest: {
				const size_t n = size();

				if (n >= 1 && n <= 2) {
					return m_slice[0]->match_size();
				} else {
					return MatchSize::exactly(1);
				}
			}


			default:
				return MatchSize::exactly(1);
		}
	}

	virtual MatchSize leaf_match_size() const final {
		MatchSize size = MatchSize::exactly(0);
		for (const auto &leaf : m_slice.leaves()) {
			size += leaf->match_size();
		}
		return size;
	}

	virtual const BaseExpressionRef *materialize(UnsafeBaseExpressionRef &materialized) const;

	virtual bool is_numeric() const final {
		if (head()->is_symbol() &&
		    head()->as_symbol()->state().has_attributes(Attributes::NumericFunction)) {

			const size_t n = size();
			for (size_t i = 0; i < n; i++) {
				if (!m_slice[i]->is_numeric())
					return false;
			}

			return true;
		} else {
			return false;
		}
	}

	virtual SortKey sort_key() const final {
		MonomialMap m(Pool::monomial_map_allocator());

		switch (_head->symbol()) {
			case S::Times: {
				const size_t n = size();

				for (size_t i = 0; i < n; i++) {
					const BaseExpressionRef leaf = m_slice[i];

					if (leaf->is_expression() &&
						leaf->as_expression()->head()->symbol() == S::Power &&
						leaf->as_expression()->size() == 2) {

						const BaseExpressionRef * const power =
							leaf->as_expression()->n_leaves<2>();

						const BaseExpressionRef &var = power[0];
						const BaseExpressionRef &exp = power[1];

						if (var->is_symbol()) {
							// FIXME
						}
					} else if (leaf->is_symbol()) {
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
		switch (_head->symbol()) {
			case S::Blank:
				return blank_sort_key(1, size() > 0, this);
			case S::BlankSequence:
				return blank_sort_key(2, size() > 0, this);
			case S::BlankNullSequence:
				return blank_sort_key(3, size() > 0, this);
			case S::PatternTest:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = m_slice[0]->pattern_key();
					key.set_pattern_test(0);
					return key;
				}
			case S::Condition:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = m_slice[0]->pattern_key();
					key.set_condition(0);
					return key;
				}
			case S::Pattern:
				if (size() != 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = m_slice[1]->pattern_key();
					key.set_pattern_test(0);
					return key;
				}
			case S::Optional:
				if (size() < 1 || size() > 2) {
					return not_a_pattern_sort_key(this);
				} else {
					SortKey key = m_slice[0]->pattern_key();
					key.set_optional(1);
					return key;
				}
			case S::Alternatives:
				// FIXME
			case S::Verbatim:
				// FIXME
			case S::OptionsPattern:
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

    virtual BaseExpressionRef custom_format_traverse(
        const BaseExpressionRef &form,
        const Evaluation &evaluation) const;

    virtual std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
        const auto &slice = m_slice;

        switch (head()->symbol()) {
	        case S::StyleBox: {
                const size_t n = size();

		        if (n < 1) {
			        break;
		        }

                StyleBoxOptions modified_options(options);

                for (size_t i = 1; i < n; i++) {
                    const BaseExpressionRef &leaf = slice[i];
                    if (leaf->has_form(S::Rule, 2, evaluation)) {
                        const BaseExpressionRef * const leaves =
                            leaf->as_expression()->n_leaves<2>();
                        const BaseExpressionRef &rhs = leaves[1];
                        switch (leaves[0]->symbol()) {
                            case S::ShowStringCharacters:
                                modified_options.ShowStringCharacters = rhs->is_true();
                                break;
                            default:
                                break;
                        }
                    }
                }

		        return slice[0]->boxes_to_text(modified_options, evaluation);
	        }

	        case S::RowBox: {
                if (size() != 1) {
                    break;
                }

                const BaseExpressionRef &list = slice[0];
                if (!list->is_expression()) {
                    break;
                }
                if (list->as_expression()->head()->symbol() != S::List) {
                    break;
                }

                std::ostringstream s;
                list->as_expression()->with_slice([&s, &options, &evaluation] (const auto &slice) {
                    const size_t n = slice.size();
                    for (size_t i = 0; i < n; i++) {
                        s << slice[i]->boxes_to_text(options, evaluation);
                    }
                });

                return s.str();
            }

	        case S::SuperscriptBox: {
                if (size() != 2) {
                    break;
                }

                std::ostringstream s;
                s << slice[0]->boxes_to_text(options, evaluation);
                s << "^" << slice[1]->boxes_to_text(options, evaluation);
                return s.str();
            }

            default:
                break;
        }

        throw std::runtime_error("box error");
    }

    virtual bool is_negative() const final {
        if (head()->symbol() == S::Times && size() >= 1) {
            return m_slice[0]->is_negative();
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
			s << m_slice[i]->debugform();
		}
		s << "]";
		return s.str();
	}
};

#include "../heap.tcc"
#include "core/atoms/numeric.tcc"
#include "construct.h"
#include "../definitions.h"
#include "../evaluation.h"
#include "../evaluate.h"
#include "../pattern/rewrite.tcc"

template<size_t N>
inline const BaseExpressionRef *Expression::n_leaves() const {
    static_assert(N >= 0 && N <= MaxTinySliceSize, "N is too large");
    return static_cast<const TinySlice<N>*>(_slice_ptr)->refs();
}

template<typename Slice>
inline ExpressionRef ExpressionImplementation<Slice>::slice(
	const BaseExpressionRef &head, index_t begin, index_t end) const {

	const index_t size = index_t(m_slice.size());

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
        return expression(head, m_slice.slice(begin, end));
    } else if (slice_code == BigSliceCode && new_size > MaxTinySliceSize) {
		return expression(head, m_slice.slice(begin, end));
	} else {
		const Slice &slice = m_slice;

		const auto generate_leaves = [begin, end, &head, &slice] (auto &store) {
			for (size_t i = begin; i < end; i++) {
				store(BaseExpressionRef(slice[i]));
			}
		};

		return expression(head, sequential(generate_leaves, new_size));
	}
}

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
BaseExpressionRef ExpressionImplementation<Slice>::replace_all(
	const ArgumentsMap &replacement, const Evaluation &evaluation) const {

	const BaseExpressionRef &old_head = _head;
	const BaseExpressionRef new_head = old_head->replace_all(replacement, evaluation);

	return conditional_map<ExpressionType, SymbolType>(
		new_head ? new_head : old_head,
		[&replacement, &evaluation] (const BaseExpressionRef &leaf) {
			return leaf->replace_all(replacement, evaluation);
		},
		evaluation);
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::clone() const {
	return expression(_head, Slice(m_slice));
}

template<typename Slice>
ExpressionRef ExpressionImplementation<Slice>::clone(const BaseExpressionRef &head) const {
	return expression(head, Slice(m_slice));
}

template<typename Slice>
const BaseExpressionRef *ExpressionImplementation<Slice>::materialize(UnsafeBaseExpressionRef &materialized) const {
	auto expr = expression(_head, m_slice.unpack());
	materialized = expr;
	return expr->m_slice.refs();
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

template<typename Slice>
std::tuple<bool, UnsafeExpressionRef> ExpressionImplementation<Slice>::thread(const Evaluation &evaluation) const {
	const size_t size = this->size();
	const auto &leaves = this->leaves();

    const auto is_threadable = [] (const BaseExpressionRef &leaf) {
		return (leaf->type() == ExpressionType &&
            leaf->as_expression()->head()->symbol() == S::List);
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

template<typename Slice>
inline BaseExpressionRef ExpressionImplementation<Slice>::negate(const Evaluation &evaluation) const {
    if (head()->symbol() == S::Times && size() >= 1) {
        const BaseExpressionRef &leaf = m_slice[0];
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
                        for (size_t i = 1; i < n; i++) {
                            store(BaseExpressionRef(m_slice[i]));
                        }
                    }, size()));
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
		if ((slice.type_mask() & TypeMaskSequence) == 0) {
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
	if (is_expression()) {
		return as_expression()->flatten_sequence();
	} else {
		return ExpressionRef();
	}
}

inline ExpressionRef Expression::flatten_sequence_or_copy() const {
    return coalesce(as_expression()->flatten_sequence(), ExpressionRef(this));
}

inline PatternMatcherRef Expression::expression_matcher() const { // concurrent.
    return ensure_cache()->expression_matcher(this);
}

inline PatternMatcherRef Expression::string_matcher() const { // concurrent.
    return ensure_cache()->string_matcher(this);
}

template<typename Expression>
BaseExpressionRef format_expr(
    const Expression *expr,
    const SymbolPtr form,
    const Evaluation &evaluation) {

    if (expr->is_expression() &&
        expr->as_expression()->head()->is_expression()) {
        // expr is of the form f[...][...]
        return BaseExpressionRef();
    }

    const Symbol * const name = expr->lookup_name();
    const SymbolRules * const rules = name->state().rules();
    if (rules) {
        const optional<BaseExpressionRef> result =
            rules->format_values.apply(expr, form, evaluation);
        if (result && *result) {
            return (*result)->evaluate_or_copy(evaluation);
        }
    }

    return BaseExpressionRef();
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::custom_format_traverse(
    const BaseExpressionRef &form,
    const Evaluation &evaluation) const {

    const BaseExpressionRef new_head =
        head()->custom_format_or_copy(form, evaluation);

    return conditional_map_all(
        new_head, [&form, &evaluation] (const BaseExpressionRef &leaf) {
            return leaf->custom_format(form, evaluation);
        }, evaluation);
}

template<typename Slice>
BaseExpressionRef ExpressionImplementation<Slice>::custom_format(
    const BaseExpressionRef &form,
    const Evaluation &evaluation) const {

    // see BaseExpression.do_format in PyMathics

    BaseExpressionPtr expr_form;
    bool include_form;

    UnsafeBaseExpressionRef expr;

    if (size() == 1) {
        switch (head()->symbol()) {
	        case S::StandardForm:
                if (form->symbol() == S::OutputForm) {
                    expr_form = form.get();
                    include_form = false;
                    expr = m_slice[0];
                    break;
                }
                // fallthrough

	        case S::InputForm:
            case S::OutputForm:
            case S::FullForm:
            case S::TraditionalForm:
            case S::TeXForm:
            case S::MathMLForm:
                expr_form = head();
                include_form = true;
                expr = m_slice[0];
                break;

            default:
                expr_form = form.get();
                include_form = false;
                expr = UnsafeBaseExpressionRef(this);
                break;
        }
    } else {
        expr_form = form.get();
        include_form = false;
        expr = UnsafeBaseExpressionRef(this);
    }

    if (expr_form->symbol() != S::FullForm && expr->is_expression()) {
        if (expr_form->is_symbol()) {
            const BaseExpressionRef formatted =
                format_expr(expr->as_expression(), expr_form->as_symbol(), evaluation);
            if (formatted) {
                expr = formatted->custom_format_or_copy(expr_form, evaluation);
                if (include_form) {
                    expr = expression(expr_form, expr);
                }
                return expr;
            }
        }

        switch (expr->as_expression()->head()->symbol()) {
            case S::StandardForm:
            case S::InputForm:
            case S::OutputForm:
            case S::FullForm:
            case S::TraditionalForm:
            case S::TeXForm:
            case S::MathMLForm: {
                expr = expr->custom_format(form, evaluation);
                break;
            }

            case S::NumberForm:
            case S::Graphics:
                // nothing
                break;

            default:
                expr = expr->custom_format_traverse(form, evaluation);
                break;
        }
    }

    if (include_form) {
        expr = expression(expr_form, expr);
    }

    return expr;
}

template<typename F>
inline auto with_slices(const Expression *a, const Expression *b, const F &f) {
	return a->with_slice([b, &f] (const auto &slice_a) {
		return b->with_slice([&f, &slice_a] (const auto &slice_b) {
			return f(slice_a, slice_b);
		});
	});
};

#endif
