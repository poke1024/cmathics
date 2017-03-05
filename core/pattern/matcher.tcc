#pragma once

#include "core/types.h"
#include "core/evaluation.h"
#include "core/atoms/symbol.h"
#include "core/expression/implementation.h"
#include "core/builtin.h"
#include "context.h"
#include "sequence.tcc"

class StringMatcher {
private:
    CachedPatternMatcherRef m_matcher;
    const BaseExpressionRef m_patt;
    const Evaluation &m_evaluation;

public:
    inline StringMatcher(
        const BaseExpressionRef &patt,
        const Evaluation &evaluation) :

        m_patt(patt), m_evaluation(evaluation) {

        switch (patt->type()) {
            case ExpressionType:
                m_matcher.initialize(patt->as_expression()->string_matcher());
                break;

            case SymbolType:
                m_matcher.initialize(compile_string_pattern(patt)); // FIXME
                break;

            default:
                break;
        }
    }

    template<typename Callback>
    inline void search(const String *string, const Callback &callback, bool overlap) const {
        if (m_matcher) {
            index_t begin = 0;
            const index_t end = string->length();
            MatchContext context(m_matcher, m_evaluation, MatchContext::NoEndAnchor);
            while (begin < end) {
                const index_t match_end = m_matcher->match(
                        context, string, begin, end);
                if (match_end >= 0) {
                    callback(begin, match_end, context.match);
                    if (overlap) {
                        begin++;
                    } else {
                        begin = match_end;
                    }
                    if (begin < end) {
                        context.reset();
                    }
                } else {
                    begin++;
                }
            }
        } else if (m_patt->type() == StringType) {
            index_t curr = 0;

            const String *patt_string = m_patt->as_string();
            const auto string_unicode = string->unicode();
            const auto patt_unicode = patt_string->unicode();
            const auto match = m_evaluation.definitions.default_match;

            while (true) {
                const index_t next = string_unicode.indexOf(patt_unicode, curr);
                if (next < 0) {
                    break;
                }
                callback(next, next + patt_string->length(), match);
                curr = next + 1;
            }
        }
    }

    inline MatchRef operator()(const String *string) const {
        if (m_matcher) {
            MatchContext context(m_matcher, m_evaluation, 0);
            const index_t match_end = m_matcher->match(
                    context, string, 0, string->length());
            if (match_end >= 0) {
                return context.match;
            } else {
                return MatchRef();
            }
        } else if (m_patt->type() == StringType) {
            if (m_patt->as_string()->same(string)) {
                return m_evaluation.definitions.default_match;
            } else {
                return MatchRef();
            }
        } else {
            return MatchRef();
        }
    }
};

class HeadLeavesMatcher {
private:
    const PatternMatcherRef m_match_head;
    const PatternMatcherRef m_match_leaves;

    template<bool MatchHead>
    inline bool match(
        MatchContext &context,
        const Expression *expr) const {

        const PatternMatcherRef &match_leaves = m_match_leaves;

        if (!match_leaves->might_match(expr->size())) {
            return false;
        }

        if (MatchHead) {
            const BaseExpressionRef &head = expr->head();

            if (m_match_head->match(FastLeafSequence(context, &head), 0, 1) < 0) {
                return false;
            }
        }

        if (expr->has_leaves_array()) {
            if (expr->with_leaves_array([&context, &match_leaves] (const BaseExpressionRef *leaves, size_t size) {
                return match_leaves->match(FastLeafSequence(context, leaves), 0, size);
            }) < 0) {
                return false;
            }
        } else {
            if (match_leaves->match(SlowLeafSequence(context, expr), 0, expr->size()) < 0) {
                return false;
            }
        }

        return true;
    }

public:
    inline HeadLeavesMatcher(
        const PatternMatcherRef &match_head,
        const PatternMatcherRef &match_leaves) :

        m_match_head(match_head),
        m_match_leaves(match_leaves) {
    }

    inline std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "HeadLeavesMatcher(" << m_match_head->name(context) << ", ";
        s << m_match_leaves->name(context) << ")";
        return s.str();
    }

    inline bool with_head(
            MatchContext &context,
            const Expression *expr) const {
        return match<true>(context, expr);
    }

    inline bool without_head(
            MatchContext &context,
            const Expression *expr) const {
        return match<false>(context, expr);
    }
};

class MatcherBase {
protected:
    CachedPatternMatcherRef m_matcher;

public:
    RewriteBaseExpression prepare(
        const BaseExpressionRef &item,
        Definitions &definitions) const;
};

template<typename OptionsProcessorRef>
class CompleteMatcher : public MatcherBase {
private:
    typedef MatchRef (CompleteMatcher::*MatchFunction)(
        const BaseExpressionRef &,
        const OptionsProcessorRef &,
        const Evaluation &) const;

    const BaseExpressionRef m_patt;
    MatchFunction m_perform_match;

private:
    MatchRef match_atom(
        const BaseExpressionRef &item,
        const OptionsProcessorRef &options,
        const Evaluation &evaluation) const {

        if (m_patt->same(item)) {
            return evaluation.definitions.default_match;
        } else {
            return MatchRef(); // no match
        }
    }

    MatchRef match_expression(
        const BaseExpressionRef &item,
        const OptionsProcessorRef &options,
        const Evaluation &evaluation) const {

        MatchContext context(m_matcher, options, evaluation);
        const index_t match = m_matcher->match(
                FastLeafSequence(context, &item), 0, 1);
        if (match >= 0) {
            return context.match;
        } else {
            return MatchRef(); // no match
        }
    }

    MatchRef match_none(
        const BaseExpressionRef &item, const OptionsProcessorRef &options, const Evaluation &evaluation) const {

        return MatchRef(); // no match
    }

public:
    inline CompleteMatcher(const BaseExpressionRef &patt) :
        m_patt(patt) {

        if (patt->type() == ExpressionType) {
            m_matcher.initialize(patt->as_expression()->expression_matcher());
            if (m_matcher->might_match(1)) {
                m_perform_match = &CompleteMatcher<OptionsProcessorRef>::match_expression;
            } else {
                m_perform_match = &CompleteMatcher<OptionsProcessorRef>::match_none;
            }
        } else {
            m_perform_match = &CompleteMatcher<OptionsProcessorRef>::match_atom;
        }
    }

    inline MatchRef operator()(
        const BaseExpressionRef &item,
        const OptionsProcessorRef &options,
        const Evaluation &evaluation) const {

        return (this->*m_perform_match)(item, options, evaluation);
    }
};

using OptionsMatcher = CompleteMatcher<OptionsProcessorRef>;

class Matcher : public CompleteMatcher<nothing> {
public:
    using CompleteMatcher<nothing>::CompleteMatcher;

    inline MatchRef operator()(
        const BaseExpressionRef &item,
        const Evaluation &evaluation) const {

        return CompleteMatcher<nothing>::operator()(item, nothing(), evaluation);
    }
};

// A SequenceMatcher is a Matcher that takes an Expression and matches only the leaves
// but not the head - it's basically a Matcher that ignores the head and is thus faster.

class SequenceMatcher : public MatcherBase {
private:
    const HeadLeavesMatcher *m_head_leaves_matcher;
    CachedBaseExpressionRef m_head;

    template<typename OptionsProcessorRef>
    inline MatchRef match(
            const Expression *expr,
            const OptionsProcessorRef &options,
            const Evaluation &evaluation) const {

        const auto * const matcher = m_head_leaves_matcher;
        if (!matcher) {
            return MatchRef();
        }
        MatchContext context(m_matcher, options, evaluation);
        if (matcher->without_head(context, expr)) {
            assert(expr->head()->same(m_head.get()));
            return context.match;
        } else {
            return MatchRef();
        }
    }

public:
    inline SequenceMatcher(const BaseExpressionRef &patt) : m_head_leaves_matcher(nullptr) {
        if (patt->type() == ExpressionType) {
            m_head.initialize(patt->as_expression()->head());
            const PatternMatcherRef matcher =
                    patt->as_expression()->expression_matcher();
            const auto head_leaves_matcher = matcher->head_leaves_matcher();
            if (head_leaves_matcher) {
                if (matcher->might_match(1)) {
                    m_matcher.initialize(matcher); // validates ptr to m_head_leaves_matcher
                    m_head_leaves_matcher = head_leaves_matcher;
                } else {
                    m_head_leaves_matcher = nullptr;
                }
            } else {
                throw std::runtime_error("constructed a SequenceMatcher for a non-expression pattern");
            }
        } else {
            throw std::runtime_error("constructed a SequenceMatcher for a non-expression pattern");
        }
    }

    inline MatchRef operator()(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return match(expr, nothing(), evaluation);
    }

    inline MatchRef operator()(
        const Expression *expr,
        const OptionsProcessorRef &options,
        const Evaluation &evaluation) const {

        return match(expr, options, evaluation);
    }
};

class Replacer : public virtual Shared {
public:
	virtual BaseExpressionRef apply(
		optional<MatchContext> &context,
		const BaseExpressionRef &item,
		const Evaluation &evaluation) const = 0;
};

using ReplacerRef = ConstSharedPtr<Replacer>;

class NoMatchReplacer : public Replacer, public PoolObject<NoMatchReplacer> {
public:
	virtual inline BaseExpressionRef apply(
		optional<MatchContext> &context,
		const BaseExpressionRef &item,
		const Evaluation &evaluation) const final {

		return BaseExpressionRef();
	}
};

template<typename Rewrite>
class SimpleReplacer : public Replacer, public PoolObject<SimpleReplacer<Rewrite>> {
private:
	const BaseExpressionRef m_lhs;
	const Rewrite m_rewrite;

public:
	inline SimpleReplacer(
		const BaseExpressionRef &lhs,
		const Rewrite &rewrite) : m_lhs(lhs), m_rewrite(rewrite) {
	}

	virtual inline BaseExpressionRef apply(
		optional<MatchContext> &context,
		const BaseExpressionRef &item,
		const Evaluation &evaluation) const final {

		if (m_lhs->same(item)) {
			return m_rewrite(item);
		} else {
			return BaseExpressionRef(); // no match
		}
	}
};

template<typename Rewrite>
class ComplexReplacer : public Replacer, public PoolObject<ComplexReplacer<Rewrite>> {
private:
	PatternMatcherRef m_matcher;
	Rewrite m_rewrite;

public:
	inline ComplexReplacer(
		const PatternMatcherRef &matcher,
		const Rewrite &rewrite) : m_matcher(matcher), m_rewrite(rewrite) {
	}

    virtual inline BaseExpressionRef apply(
	    optional<MatchContext> &context,
		const BaseExpressionRef &item,
		const Evaluation &evaluation) const final {

	    if (!context) {
		    context.emplace(MatchContext(m_matcher, evaluation));
	    } else {
		    context->reset();
	    }

	    const index_t match = m_matcher->match(
		    FastLeafSequence(*context, &item), 0, 1);
	    if (match >= 0) {
		    return m_rewrite(*context, item, evaluation);
	    } else {
		    return BaseExpressionRef(); // no match
	    }
    }
};

class EvaluationMessage {
public:
	using Function = std::function<void(
		const SymbolRef &name, const Evaluation &evaluation)>;

private:
	const Function m_message;

public:
	EvaluationMessage(const Function &message) : m_message(message) {
	}

	void operator()(
		const SymbolRef &name,
		const Evaluation &evaluation) const {

		m_message(name, evaluation);
	}
};

class RuleForm {
protected:
	// RuleForm scope must always exceed that of the
	// inspected item, since only pointers are kept.

	const BaseExpressionRef *m_leaves;

public:
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

class OptionalRuleForm : public RuleForm {
public:
	inline OptionalRuleForm(const BaseExpressionRef &item) {
		if (!item->is_expression()) {
			m_leaves = nullptr;
		} else {
			const ExpressionPtr expr = item->as_expression();

			if (expr->size() != 2) {
				m_leaves = nullptr;
			} else {
				switch (expr->head()->symbol()) {
					case S::Rule:
					case S::RuleDelayed:
						m_leaves = expr->n_leaves<2>();
						break;

					default:
						m_leaves = nullptr;
						break;
				}
			}
		}
	}
};

class MandatoryRuleForm : public RuleForm {
private:
	void reps(const BaseExpressionRef &item) const {
		throw EvaluationMessage([item] (const SymbolRef &name, const Evaluation &evaluation) {
			evaluation.message(name, "reps", item);
		});
	}

	void argrx(const ExpressionRef &expr) const {
		assert(expr->head()->is_symbol());
		throw EvaluationMessage([expr] (const SymbolRef &name, const Evaluation &evaluation) {
			evaluation.message(
				expr->head()->as_symbol(), "argrx",
			    expr->head(), MachineInteger::construct(3), MachineInteger::construct(2));
		});
	}

public:
	inline MandatoryRuleForm(const BaseExpressionRef &item) {
		if (!item->is_expression()) {
			reps(item);
		} else {
			const ExpressionPtr expr = item->as_expression();

			switch (expr->head()->symbol()) {
				case S::Rule:
				case S::RuleDelayed:
					if (expr->size() != 2) {
						argrx(expr);
					} else {
						m_leaves = expr->n_leaves<2>();
						return;
					}

				default:
					reps(item);
			}
		}
	}

	inline constexpr bool is_rule() const {
		return true;
	}
};

template<typename RuleForm, typename Factory>
inline auto instantiate_replacer(
    const BaseExpressionRef &pattern,
    const Factory &factory,
    const Evaluation &evaluation) {

	// using Factory here might look convoluted, but with immediate_replace this allows
	// us to save one vcall per matched element in the inner matching loop (namely the
	// one call to Replacer::apply).

	const RuleForm rule(pattern);

    const BaseExpressionRef &lhs =
		rule.is_rule() ? rule.left_side() : pattern;

    if (lhs->is_expression()) {
        const auto matcher = lhs->as_expression()->expression_matcher();
        if (matcher->might_match(1)) {
            if (rule.is_rule()) {
	            const BaseExpressionRef rhs = rule.right_side();

	            const ExpressionPtr cache_owner = pattern->as_expression();

                const RewriteRef do_rewrite =
		            cache_owner->ensure_cache()->rewrite(matcher, rhs, evaluation);

	            const auto rewrite = [rhs, do_rewrite] (
			        const MatchContext &context,
			        const BaseExpressionRef &item,
			        const Evaluation &evaluation) -> BaseExpressionRef {

		            return do_rewrite->rewrite_root_or_copy(
			            rhs->as_expression(),
			            [&context] (index_t i, const BaseExpressionRef &prev) {
				            return context.match->slot(i);
			            },
			            context.match->options(), evaluation);
	            };

	            return factory.template
			        create<ComplexReplacer<decltype(rewrite)>>(matcher, rewrite);
            } else {
	            const auto rewrite = [] (
			        const MatchContext &context,
			        const BaseExpressionRef &item,
			        const Evaluation &evaluation) -> BaseExpressionRef {

		            return item;
	            };

	            return factory.template
			        create<ComplexReplacer<decltype(rewrite)>>(matcher, rewrite);
            }
        } else {
            return factory.template create<NoMatchReplacer>();
        }
    } else {
	    if (rule.is_rule()) {
		    const BaseExpressionRef rhs = rule.right_side();

		    const auto rewrite = [rhs] (const BaseExpressionRef &item) -> BaseExpressionRef {
			    return rhs;
		    };

		    return factory.template
				create<SimpleReplacer<decltype(rewrite)>>(lhs, rewrite);
	    } else {
		    const auto rewrite = [] (const BaseExpressionRef &item) -> BaseExpressionRef {
			    return item;
		    };

		    return factory.template
				create<SimpleReplacer<decltype(rewrite)>>(lhs, rewrite);
	    }
    }
}

template<typename F>
class immediate_replace {
private:
	const F &m_f;
	const Evaluation &m_evaluation;

public:
	inline immediate_replace(const F &f, const Evaluation &evaluation) :
		m_f(f), m_evaluation(evaluation) {
	}

	template<typename Replacer, typename... Args>
	inline auto create(Args&&... args) const {
		Replacer replacer(std::forward<Args>(args)...);
		optional<MatchContext> context;
		return m_f([this, &replacer, &context] (const auto &item) {
			return replacer.apply(context, item, m_evaluation);
		});
	}
};

class replacer_factory {
public:
	template<typename Replacer, typename... Args>
	inline ReplacerRef create(Args&&... args) const {
		return Replacer::construct(std::forward<Args>(args)...);
	}
};

template<typename F>
inline auto match(
    const BaseExpressionRef &pattern,
    const F &f,
    const Evaluation &evaluation) {

	return instantiate_replacer<OptionalRuleForm>(
		pattern, immediate_replace<F>(f, evaluation), evaluation);
}

#include "rule.tcc"
#include "match.tcc"
