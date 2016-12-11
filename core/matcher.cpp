#include "types.h"
#include "symbol.h"
#include "matcher.h"

class GenericLeafPtr {
private:
	const Expression * const m_expr;
	const size_t m_offset;

public:
	inline GenericLeafPtr(const Expression *expr, size_t offset) : m_expr(expr), m_offset(offset) {
	}

	inline BaseExpressionRef operator*() const {
		return m_expr->materialize_leaf(m_offset);
	}

	inline BaseExpressionRef operator[](size_t i) const {
		return m_expr->materialize_leaf(m_offset + i);
	}

	inline bool operator==(const GenericLeafPtr &ptr) const {
		assert(m_expr == ptr.m_expr);
		return m_offset == ptr.m_offset;
	}

	inline bool operator<(const GenericLeafPtr &ptr) const {
		assert(m_expr == ptr.m_expr);
		return m_offset < ptr.m_offset;
	}

	inline GenericLeafPtr operator+(size_t i) const {
		return GenericLeafPtr(m_expr, m_offset + i);
	}

	inline index_t operator-(const GenericLeafPtr& ptr) const {
		assert(m_expr == ptr.m_expr);
		return m_offset - ptr.m_offset;
	}
};

BaseExpressionRef sequence(const BaseExpressionRef *p, size_t n, const Definitions &definitions) {
	return expression(definitions.symbols().Sequence, [p, n] (auto &storage) {
		for (size_t i = 0; i < n; i++) {
			storage << p[i];
		}
	}, n);
}

BaseExpressionRef sequence(const GenericLeafPtr &p, size_t n, const Definitions &definitions) {
	return BaseExpressionRef(); // FIXME
}

#define DECLARE_MATCH_METHODS                                                                                 \
	virtual bool match(                                                                                       \
		MatchContext &context,                                                                                \
		const BaseExpressionRef *begin,                                                                       \
		const BaseExpressionRef *end) const {                                                                 \
		return do_match<const BaseExpressionRef*>(context, begin, end);                                       \
	}                                                                                                         \
																											  \
	virtual bool match(                                                                                       \
		MatchContext &context,                                                                                \
		GenericLeafPtr begin,                                                                                 \
		GenericLeafPtr end) const {                                                                           \
		return do_match<GenericLeafPtr>(context, begin, end);                                                 \
	}

class TerminateMatcher : public PatternMatcher {
private:
	template<typename LeafPtr>
	inline bool do_match(
		MatchContext &context,
		LeafPtr begin,
		LeafPtr end) const {

		return begin == end;
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

	template<typename LeafPtr>
	inline bool do_match(
		MatchContext &context,
		LeafPtr begin,
		LeafPtr end) const {

		const auto item = *begin;
		if (begin < end && same(m_patt.get(), item.get())) {
			const PatternMatcherRef &next = m_next;
			return m_variable(context, item, [begin, end, &next, &context] () {
				return next->match(context, begin + 1, end);
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

	template<typename LeafPtr>
	inline bool do_match(
		MatchContext &context,
		LeafPtr begin,
		LeafPtr end) const {

		const auto item = *begin;
		if (begin < end && m_condition(item)) {
			const PatternMatcherRef &next = m_next;
			return m_variable(context, item, [begin, end, &next, &context] () {
				return next->match(context, begin + 1, end);
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

template<index_t Minimum, typename Condition, typename Variable>
class BlankSequenceMatcher : public PatternMatcher {
private:
	const Condition m_condition;
	const Variable m_variable;

	template<typename LeafPtr>
	inline bool do_match(
		MatchContext &context,
		LeafPtr begin,
		LeafPtr end) const {

		const index_t n = end - begin;

		const index_t max_size = n - m_size_from_next.min();

		if (max_size < Minimum) {
			return false;
		}

		const index_t min_size = std::max(n - index_t(m_size_from_next.max()), Minimum);

		index_t condition_max_size = max_size;
		for (size_t i = 0; i < max_size; i++) {
			if (!m_condition(begin[i])) {
				condition_max_size = i;
				break;
			}
		}

		const PatternMatcherRef &next = m_next;
		for (index_t i = condition_max_size; condition_max_size >= min_size; i--) {
			return m_variable(context, sequence(begin, i, context.definitions), [begin, end, i, &next, &context] () {
				return next->match(context, begin + i, end);
			});

		}

		return false;
	}

public:
	inline BlankSequenceMatcher(const Condition &condition, const Variable &variable) :
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

	template<typename LeafPtr>
	inline bool do_match(
		MatchContext &context,
		LeafPtr begin,
		LeafPtr end) const {

		if (begin == end) {
			return false;
		}

		const auto item = *begin;

		if (item->type() != ExpressionType) {
			return false;
		}

		const Expression *expr = item->as_expression();

		const PatternMatcherRef &match_leaves = m_match_leaves;

		if (!match_leaves->might_match(expr->size())) {
			return false;
		}

		const BaseExpressionRef head = expr->head();

		if (!m_match_head->match(context, &head, &head + 1)) {
			return false;
		}

		if (slice_needs_no_materialize(expr->slice_code())) {
			if (!expr->with_leaves_array([&context, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
				return match_leaves->match(context, leaves, leaves + size);
			})) {
				return false;
			}
		} else {
			if (!match_leaves->match(context, GenericLeafPtr(expr, 0), GenericLeafPtr(expr, expr->size()))) {
				return false;
			}
		}

		const PatternMatcherRef &next = m_next;

		return m_variable(context, item, [begin, end, &next, &context] () {
			return next->match(context, begin + 1, end);
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

	const size_t n = end - begin;

	std::vector<MatchSize> matchable;
	matchable.reserve(n + 1);
	MatchSize size = MatchSize::exactly(0);
	matchable.push_back(size);
	for (const BaseExpressionRef *i = end - 1; i >= begin; i--) {
		size += (*i)->match_size();
		matchable.push_back(size);
	}
	std::reverse(matchable.begin(), matchable.end());

	PatternMatcherRef initial_matcher;
	PatternMatcherRef previous_matcher;

	for (size_t i = 0; i < n; i++) {
		const BaseExpressionRef &curr = begin[i];

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

		matcher->set_size(matchable[i], matchable[i + 1]);

		if (previous_matcher) {
			previous_matcher->set_next(matcher);
		} else {
			initial_matcher = matcher;
		}

		previous_matcher = matcher;
	}

	assert(previous_matcher);
	previous_matcher->set_next(PatternMatcherRef(new TerminateMatcher));

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

		case SymbolBlankSequence: {
			if (patt_end - patt_begin == 1 && (*patt_begin)->type() == SymbolType) {
				return std::make_tuple(new BlankSequenceMatcher<1, HeadCondition, Variable>(HeadCondition(*patt_begin), do_variable), true);
			} else {
				return std::make_tuple(new BlankSequenceMatcher<1, NoCondition, Variable>(NoCondition(), do_variable), true);
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
