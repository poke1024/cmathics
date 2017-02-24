#pragma once

typedef RewriteRule<Matcher> SubRule;

typedef RewriteRule<Matcher> UpRule;

// DownRule assumes that the expression's head was matched already using the down lookup rule
// process, so it only looks at the leaves.

typedef RewriteRule<SequenceMatcher> DownRule;

inline NewRuleRef make_down_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
    return [patt, into] (const SymbolRef &head, Definitions &definitions) -> RuleRef {
        return RuleRef(new DownRule(patt, into, definitions));
    };
}

// note: PatternMatchedBuiltinRule should only be used for builtins that match non-down values (e.g. sub values).
// if you write builtins that match down values, always use BuiltinRule, since it's faster (it doesn't involve the
// pattern match).

template<int N, typename F>
class PatternMatchedBuiltinRule : public Rule {
private:
    const F function;
    const SequenceMatcher matcher;

public:
    inline PatternMatchedBuiltinRule(const BaseExpressionRef &patt, const F &f) :
        Rule(patt), function(f), matcher(pattern) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        const MatchRef match = matcher(expr, evaluation);
        if (match) {
            assert(match->n_slots_fixed() == N);
            return apply_from_tuple(function, std::tuple_cat(
                    std::forward_as_tuple(expr),
                    match->unpack<N>(),
                    std::forward_as_tuple(evaluation)));
        } else {
            return optional<BaseExpressionRef>();
        }
    }

    virtual MatchSize leaf_match_size() const {
        assert(pattern->type() == ExpressionType);
        return pattern->as_expression()->leaf_match_size();
    }

    virtual SortKey pattern_key() const {
        return pattern->pattern_key();
    }
};

template<int N, typename Options, typename F>
class PatternMatchedOptionsBuiltinRule : public Rule {
private:
    const F m_function;
    const SequenceMatcher m_matcher;
    const OptionsDefinitions<Options> m_options;

public:
    inline PatternMatchedOptionsBuiltinRule(
        const BaseExpressionRef &patt,
        const F &f,
        const OptionsDefinitions<Options> &options) :
        Rule(patt), m_function(f), m_matcher(pattern), m_options(options) {
    }

    virtual optional<BaseExpressionRef> try_apply(
            const Expression *expr,
            const Evaluation &evaluation) const {

        StaticOptionsProcessor<Options> processor(m_options);
        const MatchRef match = m_matcher(
                expr, OptionsProcessorRef(&processor), evaluation);
        if (match) {
            assert(match->n_slots_fixed() == N);
            return apply_from_tuple(m_function, std::tuple_cat(
                    std::forward_as_tuple(expr),
                    match->unpack<N>(),
                    std::forward_as_tuple(processor.options(), evaluation)));
        } else {
            return optional<BaseExpressionRef>();
        }
    }

    virtual MatchSize leaf_match_size() const {
        assert(pattern->type() == ExpressionType);
        return pattern->as_expression()->leaf_match_size();
    }

    virtual SortKey pattern_key() const {
        return pattern->pattern_key();
    }
};

template<int N>
inline NewRuleRef make_pattern_matched_builtin_rule(
    const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {

    return [patt, func] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
        return new PatternMatchedBuiltinRule<N, decltype(func)>(patt, func);
    };
}