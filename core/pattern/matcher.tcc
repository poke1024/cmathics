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
            const auto match = Pool::DefaultMatch();

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
                return Pool::DefaultMatch();
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

        if (slice_needs_no_materialize(expr->slice_code())) {
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
        const BaseExpressionRef &, const OptionsProcessorRef &, const Evaluation &) const;

    const BaseExpressionRef m_patt;
    MatchFunction m_perform_match;

private:
    MatchRef match_atom(
            const BaseExpressionRef &item, const OptionsProcessorRef &options, const Evaluation &evaluation) const {

        if (m_patt->same(item)) {
            return Pool::DefaultMatch();
        } else {
            return MatchRef(); // no match
        }
    }

    MatchRef match_expression(
            const BaseExpressionRef &item, const OptionsProcessorRef &options, const Evaluation &evaluation) const {

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
        const BaseExpressionRef &item, const OptionsProcessorRef &options, const Evaluation &evaluation) const {

        return (this->*m_perform_match)(item, options, evaluation);
    }
};

using OptionsMatcher = CompleteMatcher<OptionsProcessorRef>;

class Matcher : public CompleteMatcher<nothing> {
public:
    using CompleteMatcher<nothing>::CompleteMatcher;

    inline MatchRef operator()(
        const BaseExpressionRef &item, const Evaluation &evaluation) const {

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

template<typename Rule, typename F>
inline auto match(
    const Rule &rule,
    const F &f,
    const Expression *cache_owner,
    const Evaluation &evaluation) {

    constexpr int has_rhs = std::tuple_size<Rule>::value > 1;
    const BaseExpressionRef &lhs = std::get<0>(rule);
    const BaseExpressionRef &rhs = std::get<std::tuple_size<Rule>::value - 1>(rule);

    if (lhs->type() == ExpressionType) {
        const auto matcher = lhs->as_expression()->expression_matcher();
        if (matcher->might_match(1)) {
            MatchContext context(matcher, evaluation);

            const auto make = [&f, &matcher, &context] (const auto &extract) {
                return f([&matcher, &context, &extract] (const BaseExpressionRef &item) {
                    context.reset();
                    const index_t match = matcher->match(
                        FastLeafSequence(context, &item), 0, 1);
                    if (match >= 0) {
                        return extract(item);
                    } else {
                        return BaseExpressionRef(); // no match
                    }
                });
            };

            if (has_rhs) {
                const RewriteRef rewrite = cache_owner->ensure_cache()->rewrite(matcher, rhs, evaluation);

                return make([&rewrite, &context, &rhs, &evaluation] (const BaseExpressionRef &item) {
                    return rewrite->rewrite_root_or_copy(
                        rhs->as_expression(),
                        [&context] (index_t i, const BaseExpressionRef &prev) {
                            return context.match->slot(i);
                        },
                        context.match->options(), evaluation);
                });
            } else {
                return make([] (const BaseExpressionRef &item) {
                    return item;
                });
            }
        } else {
            return f([] (const BaseExpressionRef &item) {
                return BaseExpressionRef(); // no match
            });
        }
    } else {
        return f([&lhs, &rhs, &has_rhs] (const BaseExpressionRef &item) {
            if (lhs->same(item)) {
                return has_rhs ? rhs : item;
            } else {
                return BaseExpressionRef(); // no match
            }
        });
    }
}

class RuleForm {
private:
    const BaseExpressionRef *m_leaves;

public:
    // note that the scope of "item" must contain that of
    // RuleForm, i.e. item must be referenced at least as
    // long as RuleForm lives.
    inline RuleForm(const BaseExpressionPtr item) {
        if (!item->is_expression()) {
            m_leaves = nullptr;
        } else {
            const auto expr = item->as_expression();

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

template<typename F>
inline auto match(
    const BaseExpressionRef &pattern,
    const F &f,
    const Evaluation &evaluation) {

    const RuleForm rule(pattern.get());

    if (rule.is_rule()) {
        return match(
            std::make_tuple(rule.left_side(), rule.right_side()),
            f,
            pattern->as_expression(),
            evaluation);
    } else {
        return match(
            std::make_tuple(pattern),
            f,
            nullptr,
            evaluation);
}
}

#include "rule.tcc"
#include "match.tcc"
