#include "types.h"
#include "symbol.h"
#include "matcher.h"

#define DECLARE_MATCH_METHODS                               \
	virtual bool match(                                     \
		MatchContext &context,                              \
		const FastSlice &slice) const {                     \
		return do_match<FastSlice>(context, slice);         \
	}                                                       \
															\
	virtual bool match(                                     \
		MatchContext &context,                              \
		const SlowSlice &slice) const {                     \
			return do_match<SlowSlice>(context, slice);     \
	}

class TerminateMatcher : public PatternMatcher {
private:
	template<typename Slice>
	inline bool do_match(
		MatchContext &context,
		const Slice &slice) const {

		return slice.size() == 0;
	}

public:
	DECLARE_MATCH_METHODS
};

inline bool same(BaseExpressionPtr a, BaseExpressionPtr b) {
	if (a == b) {
		return true;
	} else {
		return a->same(b);
	}
}

class NoCondition {
public:
	inline bool operator()(const BaseExpressionRef &item) const {
		return true;
	}
};

class HeadCondition {
private:
	const BaseExpressionRef m_head;

public:
	inline HeadCondition(const BaseExpressionRef &head) : m_head(head) {
	}

	inline bool operator()(const BaseExpressionRef &item) const {
		return same(item->head_ptr(), m_head.get());
	}
};

class NoVariable {
public:
	template<typename F>
	inline bool operator()(MatchContext &context, const BaseExpressionRef &item, const F &f) const {
		return f();
	}
};

class AssignVariable {
private:
	SymbolRef m_variable;

public:
	inline AssignVariable(const SymbolRef &variable) : m_variable(variable) {
	}

	template<typename F>
	inline bool operator()(MatchContext &context, const BaseExpressionRef &item, const F &f) const {
		bool match;

		m_variable->set_matched_value(context.id, item);
		try {
			match = f();
		} catch (...) {
			m_variable->clear_matched_value();
			throw;
		}

		if (match) {
			context.matched_variables.prepend(m_variable.get());
		} else {
			m_variable->clear_matched_value();
		}

		return match;
	}
};

class VerifyVariable {
private:
	SymbolRef m_variable;

public:
	inline VerifyVariable(const SymbolRef &variable) : m_variable(variable) {
	}

	template<typename F>
	inline bool operator()(MatchContext &context, const BaseExpressionRef &item, const F &f) const {
		const BaseExpressionRef existing = m_variable->matched_value(context.id);
		assert (existing);
		return same(existing.get(), item.get());
	}
};

template<typename Variable>
class SameMatcher : public PatternMatcher {
private:
	const BaseExpressionRef m_patt;
	const Variable m_variable;

	template<typename Slice>
	inline bool do_match(
		MatchContext &context,
		const Slice &slice) const {

		const auto item = slice[0];
		if (slice.size() > 0 && same(m_patt.get(), item.get())) {
			const PatternMatcherRef &next = m_next;
			return m_variable(context, item, [&context, &slice, &next] () {
				return next->match(context, slice.slice(1));
			});
		} else {
			return false;
		}
	}

public:
	inline SameMatcher(const BaseExpressionRef &patt, const Variable &variable) :
		m_patt(patt), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Condition, typename Variable>
class BlankMatcher : public PatternMatcher {
private:
	const Condition m_condition;
	const Variable m_variable;

	template<typename Slice>
	inline bool do_match(
		MatchContext &context,
		const Slice &slice) const {

		const auto item = slice[0];
		if (slice.size() > 0 && m_condition(item)) {
			const PatternMatcherRef &next = m_next;
			return m_variable(context, item, [&context, &slice, &next] () {
				return next->match(context, slice.slice(1));
			});
		} else {
			return false;
		}
	}

public:
	inline BlankMatcher(const Condition &condition, const Variable &variable) :
		m_condition(condition), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

template<typename Variable>
class ExpressionMatcher : public PatternMatcher {
private:
	const PatternMatcherRef m_match_head;
	const PatternMatcherRef m_match_leaves;
	const Variable m_variable;

	template<typename Slice>
	inline bool do_match(
		MatchContext &context,
		const Slice &slice) const {

		if (slice.size() == 0) {
			return false;
		}

		const auto item = slice[0];

		if (item->type() != ExpressionType) {
			return false;
		}

		const Expression *expr = item->as_expression();

		const BaseExpressionRef head = expr->head();

		if (!m_match_head->match(context, FastSlice(&head, &head + 1))) {
			return false;
		}

		const PatternMatcherRef &match_leaves = m_match_leaves;

		if (slice_needs_no_materialize(expr->slice_code())) {
			if (!expr->with_leaves_array([&context, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
				return match_leaves->match(context, FastSlice(leaves, leaves + size));
			})) {
				return false;
			}
		} else {
			if (!match_leaves->match(context, SlowSlice(expr, 0))) {
				return false;
			}
		}

		const PatternMatcherRef &next = m_next;

		return m_variable(context, item, [&context, &slice, &next] () {
			return next->match(context, slice.slice(1));
		});
	}

public:
	inline ExpressionMatcher(PatternMatcherRef head, PatternMatcherRef leaves, const Variable &variable) :
		m_match_head(head), m_match_leaves(leaves), m_variable(variable) {
	}

	DECLARE_MATCH_METHODS
};

class PatternCompiler {
private:
	std::set<BaseExpressionRef> m_assigned_variables;

	template<typename M>
	PatternMatcherRef instantiate_matcher(
		const M &make_matcher,
		const SymbolRef *variable);

	template<typename Variable>
	std::tuple<PatternMatcherRef, bool> compile_part(
		const BaseExpressionRef &patt_head,
		const BaseExpressionRef *patt_begin,
		const BaseExpressionRef *patt_end,
		const Variable &do_variable);

public:
	PatternMatcherRef compile(
		const BaseExpressionRef *begin,
		const BaseExpressionRef *end,
		const SymbolRef *variable);
};

template<typename M>
PatternMatcherRef PatternCompiler::instantiate_matcher(
	const M &make_matcher,
	const SymbolRef *variable) {

	PatternMatcherRef matcher;

	if (variable) {
		if (m_assigned_variables.find(*variable) != m_assigned_variables.end()) {
			matcher = std::get<0>(make_matcher(VerifyVariable(*variable)));
		} else {
			bool variable_got_assigned;
			std::tie(matcher, variable_got_assigned) =
				make_matcher(AssignVariable(*variable));
			if (variable_got_assigned) {
				m_assigned_variables.insert(*variable);
			}
		}
	} else {
		matcher = std::get<0>(make_matcher(NoVariable()));
	}

	return matcher;
}

PatternMatcherRef PatternCompiler::compile(
	const BaseExpressionRef *begin,
	const BaseExpressionRef *end,
	const SymbolRef *variable) {

	PatternMatcherRef initial_matcher;
	PatternMatcherRef previous_matcher;

	for (const BaseExpressionRef *i = begin; i < end; i++) {
		const BaseExpressionRef &curr = *i;
		PatternMatcherRef matcher;

		switch (curr->type()) {
			case ExpressionType: {
				PatternCompiler * const compiler = this;
				const Expression * const patt_expr = curr->as_expression();

				matcher = instantiate_matcher([compiler, patt_expr] (auto do_variable) {
					return patt_expr->with_leaves_array(
						[compiler, patt_expr, &do_variable]
						(const BaseExpressionRef *leaves, size_t size) {
							return compiler->compile_part(
								patt_expr->head(), leaves, leaves + size, do_variable);
						});
				}, variable);
				break;
			}

			default:
				matcher = instantiate_matcher([&curr] (auto do_variable) {
					return std::make_tuple(new SameMatcher<decltype(do_variable)>(curr, do_variable), true);
				}, variable);
				break;
		}

		if (previous_matcher) {
			previous_matcher->set_next(matcher);
		} else {
			initial_matcher = matcher;
		}

		previous_matcher = matcher;
	}

	assert(previous_matcher);
	previous_matcher->set_next(PatternMatcherRef(new TerminateMatcher()));

	return initial_matcher;
}

template<typename Variable>
std::tuple<PatternMatcherRef, bool> PatternCompiler::compile_part(
	const BaseExpressionRef &patt_head,
	const BaseExpressionRef *patt_begin,
	const BaseExpressionRef *patt_end,
	const Variable &do_variable) {

	switch (patt_head->extended_type()) {
		case SymbolBlank: {
			if (patt_end - patt_begin == 1 && (*patt_begin)->type() == SymbolType) {
				return std::make_tuple(new BlankMatcher<HeadCondition, Variable>(HeadCondition(*patt_begin), do_variable), true);
			} else {
				return std::make_tuple(new BlankMatcher<NoCondition, Variable>(NoCondition(), do_variable), true);
			}
		}

		case SymbolPattern: {
			if (patt_end - patt_begin == 2) { // 2 leaves?
				if ((*patt_begin)->type() == SymbolType) {
					const SymbolRef variable = boost::const_pointer_cast<Symbol>(
						boost::static_pointer_cast<const Symbol>(*patt_begin));
					return std::make_tuple(compile(patt_begin + 1, patt_begin + 2, &variable), false);
				}
			}
		}
		// fallthrough

		default: {
			const PatternMatcherRef match_head = compile(
				&patt_head, &patt_head + 1, nullptr);

			const PatternMatcherRef match_leaves = compile(
				patt_begin, patt_end, nullptr);

			return std::make_tuple(new ExpressionMatcher<decltype(do_variable)>(match_head, match_leaves, do_variable), true);
		}
	}
}

PatternMatcherRef compile_pattern(const BaseExpressionRef &patt) {
	PatternCompiler compiler;
	return compiler.compile(&patt, &patt + 1, nullptr);
}
