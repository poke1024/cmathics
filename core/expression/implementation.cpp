#include "core/types.h"
#include "implementation.h"
#include "core/pattern/arguments.h"
#include "core/evaluate.h"

BaseExpressionRef Expression::evaluate_expression(
	const Evaluation &evaluation) const {

	// Evaluate the head

	UnsafeBaseExpressionRef head = _head;

	while (true) {
		auto new_head = head->evaluate(evaluation);
		if (new_head) {
			head = new_head;
		} else {
			break;
		}
	}

	// Evaluate the leaves and apply rules.

	if (!head->is_symbol()) {
		if (head.get() != _head.get()) {
			const ExpressionRef new_head_expr = static_pointer_cast<const Expression>(clone(head));
			const BaseExpressionRef result = new_head_expr->evaluate_expression_with_non_symbol_head(evaluation);
			if (result) {
				return result;
			} else {
				return new_head_expr;
			}
		} else {
			return evaluate_expression_with_non_symbol_head(evaluation);
		}
	} else {
		const Symbol *head_symbol = head->as_symbol();

		return head_symbol->state().dispatch(
			this, slice_code(), *_slice_ptr, evaluation);
	}
}

SymbolicFormRef Expression::instantiate_symbolic_form(const Evaluation &evaluation) const {
    return evaluation.definitions.no_symbolic_form;
}

optional<hash_t> Expression::compute_match_hash() const {
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

			return with_slice([&head_hash] (const auto &slice) -> optional<hash_t> {
				hash_t result = hash_combine(slice.size(), *head_hash);
				for (auto leaf : slice) {
					const auto leaf_hash = leaf->match_hash();
					if (!leaf_hash) {
						return optional<hash_t>();
					}
					result = hash_combine(result, *leaf_hash);
				}
				return result;
			});
		}
	}
}

inline tribool Expression::equals(const BaseExpression &item) const {
	if (this == &item) {
		return true;
	}
	if (!item.is_expression()) {
		return false;
	}

	const Expression * const expr = item.as_expression();

	if (size() != expr->size()) {
		return false;
	}

	const tribool head = _head->equals(*expr->head());
	if (head != true) {
		return head;
	}

	return with_slices(this, item.as_expression(), [] (const auto &a, const auto &b) {
		const size_t n = a.size();
		assert(n == b.size());
		for (size_t i = 0; i < n; i++) {
			const tribool leaf = a[i]->same(b[i]);
			if (leaf != true) {
				return leaf;
			}
		}

		return tribool(true);
	});
}

MatchSize Expression::match_size() const {
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
				return n_leaves<2>()[1]->match_size();
			} else {
				return MatchSize::exactly(1);
			}

		case S::Alternatives: {
			const size_t n = size();

			if (n == 0) {
				return MatchSize::exactly(1); // FIXME
			}

			return with_slice([] (const auto &slice) {
				const size_t n = slice.size();

				const MatchSize size = slice[0]->match_size();
				match_size_t min_p = size.min();
				match_size_t max_p = size.max();

				for (size_t i = 1; i < n; i++) {
					const MatchSize leaf_size = slice[i]->match_size();
					max_p = std::max(max_p, leaf_size.max());
					min_p = std::min(min_p, leaf_size.min());
				}

				return MatchSize::between(min_p, max_p);
			});
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
				return with_slice([] (const auto &slice) {
					return slice[0]->match_size();
				});
			} else {
				return MatchSize::exactly(1);
			}
		}

		default:
			return MatchSize::exactly(1);
	}
}

bool Expression::is_numeric() const {
	if (head()->is_symbol() &&
	    head()->as_symbol()->state().has_attributes(Attributes::NumericFunction)) {

		return with_slice([] (const auto &slice) {
			const size_t n = slice.size();
			for (size_t i = 0; i < n; i++) {
				if (!slice[i]->is_numeric())
					return false;
			}

			return true;
		});
	} else {
		return false;
	}
}


void Expression::sort_key(SortKey &key, const Evaluation &evaluation) const {
	MonomialMap m;

	switch (_head->symbol()) {
		case S::Times: {
			with_slice([&m] (const auto &slice) {
				const size_t n = slice.size();

				for (size_t i = 0; i < n; i++) {
					const BaseExpressionRef leaf = slice[i];

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
			});

			break;
		}

		default: {
			break;
		}
	}

	if (!m.empty()) {
		key.construct(
			is_numeric() ? 1 : 2, 2, std::move(m), 1, SortByHead(this), SortByLeaves(this), 1);
	} else {
		key.construct(
			is_numeric() ? 1 : 2, 3, SortByHead(this), SortByLeaves(this), 1);
	}
}

std::string Expression::boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
	return with_slice([this, &options, &evaluation] (const auto &slice) {
		switch (head()->symbol()) {
			case S::StyleBox: {
				const size_t n = size();

				if (n < 1) {
					break;
				}

				StyleBoxOptions modified_options(options);

				for (size_t i = 1; i < n; i++) {
					const BaseExpressionRef &leaf = slice[i];
					if (leaf->has_form(S::Rule, 2)) {
						const BaseExpressionRef *const leaves = leaf->as_expression()->n_leaves<2>();
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
				list->as_expression()->with_slice([&s, &options, &evaluation](const auto &slice) {
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
	});
}

bool Expression::is_negative_introspect() const {
	if (head()->symbol() == S::Times && size() >= 1) {
		return with_slice([] (const auto &slice) {
			return slice[0]->is_negative_introspect();
		});
	} else {
		return false;
	}
}

std::string Expression::debugform() const {
	std::ostringstream s;
	s << head()->debugform();
	s << "[";
	with_slice([&s] (const auto &slice) {
		for (size_t i = 0; i < slice.size(); i++) {
			if (i > 0) {
				s << ", ";
			}
			s << slice[i]->debugform();
		}
	});
	s << "]";
	return s.str();
}

hash_t Expression::hash() const {
	return with_slice([this] (const auto &slice) {
		hash_t result = hash_combine(slice.size(), _head->hash());
		for (auto leaf : slice) {
			result = hash_combine(result, leaf->hash());
		}
		return result;
	});
}

BaseExpressionRef Expression::negate(const Evaluation &evaluation) const {
	if (head()->symbol() == S::Times && size() >= 1) {
		return with_slice([this, &evaluation] (const auto &slice) -> BaseExpressionRef {
			const BaseExpressionRef &leaf = slice[0];
			if (leaf->is_number()) {
				const BaseExpressionRef negated = leaf->negate(evaluation);
				if (negated->is_one()) {
					if (size() == 1) {
						return negated;
					} else {
						return Expression::slice(evaluation.Times, 1);
					}
				} else {
					return expression(evaluation.Times, sequential(
						[&negated, &slice] (auto &store) {
							store(BaseExpressionRef(negated));
							const size_t n = slice.size();
							for (size_t i = 1; i < n; i++) {
								store(BaseExpressionRef(slice[i]));
							}
						}, size()));
				}
			} else {
				return expression(
					evaluation.Times, evaluation.definitions.minus_one, leaf);
			}
		});
	} else {
		return BaseExpression::negate(evaluation);
	}
}

template<typename Compute, typename Recurse>
BaseExpressionRef Expression::do_symbolic(
	const Compute &compute,
	const Recurse &recurse,
	const Evaluation &evaluation) const {

	const SymbolicFormRef form = unsafe_symbolic_form(this, evaluation);

	if (form && !form->is_none()) {
		const SymbolicFormRef new_form = compute(form);
		if (new_form && !new_form->is_none()) {
			return from_symbolic_form(new_form->get(), evaluation);
		} else {
			return BaseExpressionRef();
		}
	} else {
		return selective_conditional_map<ExpressionType>(
			[&recurse, &evaluation] (const BaseExpressionRef &leaf) {
				return recurse(leaf, evaluation);
			},
			evaluation);
	}
}

BaseExpressionRef Expression::expand(const Evaluation &evaluation) const {
	return do_symbolic(
		[] (const SymbolicFormRef &form) {
			const SymEngineRef new_form = SymEngine::expand(form->get());
			if (new_form.get() != form->get().get()) {
				return SymbolicForm::construct(new_form);
			} else {
				return SymbolicForm::construct(SymEngineRef());
			}
		},
		[] (const BaseExpressionRef &leaf, const Evaluation &evaluation) {
			return leaf->expand(evaluation);
		},
		evaluation);
}

BaseExpressionRef Expression::replace_all(
	const MatchRef &match, const Evaluation &evaluation) const {

	return selective_conditional_map<ExpressionType, SymbolType>(
		replace_head(_head, _head->replace_all(match)),
		[&match] (const BaseExpressionRef &leaf) {
			return leaf->replace_all(match);
		},
		evaluation);
}

BaseExpressionRef Expression::replace_all(
	const ArgumentsMap &replacement,
	const Evaluation &evaluation) const {

	return selective_conditional_map<ExpressionType, SymbolType>(
		replace_head(_head, _head->replace_all(replacement, evaluation)),
		[&replacement, &evaluation] (const BaseExpressionRef &leaf) {
			return leaf->replace_all(replacement, evaluation);
		}, evaluation);
}

template<typename Slice>
inline ExpressionRef clone(const BaseExpressionRef &head, const Slice &slice) {
	return ExpressionImplementation<Slice>::construct(head, Slice(slice));
}

BaseExpressionRef Expression::clone() const {
	return with_slice_c([this] (const auto &slice) {
		return ::clone(_head, slice);
	});
}

ExpressionRef Expression::clone(const BaseExpressionRef &head) const {
	return with_slice_c([&head] (const auto &slice) {
		return ::clone(head, slice);
	});
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

BaseExpressionRef Expression::custom_format_traverse(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	return conditional_map(
		replace_head(_head, _head->custom_format(form, evaluation)),
        [&form, &evaluation] (const BaseExpressionRef &leaf) {
			return leaf->custom_format(form, evaluation);
		}, evaluation);
}

BaseExpressionRef Expression::custom_format(
	const BaseExpressionRef &form,
	const Evaluation &evaluation) const {

	// see BaseExpression.do_format in PyMathics

	BaseExpressionPtr expr_form;
	bool include_form;

	UnsafeBaseExpressionRef expr;

	if (size() == 1) {
		const auto * const leaves = n_leaves<1>();

		switch (head()->symbol()) {
			case S::StandardForm:
				if (form->symbol() == S::OutputForm) {
					expr_form = form.get();
					include_form = false;
					expr = leaves[0];
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
				expr = leaves[0];
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

const BaseExpressionRef *Expression::materialize(UnsafeBaseExpressionRef &materialized) const {
    return with_slice_c([this, &materialized] (const auto &slice) {
        auto expr = expression(_head, slice.unpack());
        materialized = expr;
        return expr->slice().refs();
    });
}

BaseExpressionRef Expression::deverbatim() const {
	if (head()->symbol() == S::Verbatim && size() == 1) {
		return n_leaves<1>()[0];
	} else {
		return this;
	}
}
