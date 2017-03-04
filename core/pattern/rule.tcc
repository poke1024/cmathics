#pragma once

typedef RewriteRule<Matcher> SubRule;

typedef RewriteRule<Matcher> UpRule;

// DownRule assumes that the expression's head was matched already using the down lookup rule
// process, so it only looks at the leaves.

typedef RewriteRule<SequenceMatcher> DownRule;

inline NewRuleRef make_down_rule(const BaseExpressionRef &patt, const BaseExpressionRef &into) {
    return [patt, into] (const SymbolRef &head, Evaluation &evaluation) -> RuleRef {
        return RuleRef(DownRule::construct(patt, into, evaluation));
    };
}

// note: PatternMatchedBuiltinRule should only be used for builtins that match non-down values (e.g. sub values).
// if you write builtins that match down values, always use BuiltinRule, since it's faster (it doesn't involve the
// pattern match).

template<int N, typename F>
class PatternMatchedBuiltinRule : public Rule, public ExtendedHeapObject<PatternMatchedBuiltinRule<N, F>> {
private:
    const F function;
    const SequenceMatcher matcher;

public:
    inline PatternMatchedBuiltinRule(const BaseExpressionRef &patt, const F &f, const Evaluation &evaluation) :
        Rule(patt, evaluation), function(f), matcher(pattern) {
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

    virtual void pattern_key(SortKey &key, const Evaluation &evaluation) const {
	    pattern->pattern_key(key, evaluation);
    }
};

template<int N, typename Options, typename F>
class PatternMatchedOptionsBuiltinRule :
    public Rule, public ExtendedHeapObject<PatternMatchedOptionsBuiltinRule<N, Options, F>> {

private:
    const F m_function;
    const SequenceMatcher m_matcher;
    const OptionsDefinitions<Options> m_options;

public:
    inline PatternMatchedOptionsBuiltinRule(
        const BaseExpressionRef &patt,
        const F &f,
        const OptionsDefinitions<Options> &options,
        const Evaluation &evaluation) :
        Rule(patt, evaluation), m_function(f), m_matcher(pattern), m_options(options) {
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

    virtual void pattern_key(SortKey &key, const Evaluation &evaluation) const {
        pattern->pattern_key(key, evaluation);
    }
};

template<int N>
inline NewRuleRef make_pattern_matched_builtin_rule(
    const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {

    return [patt, func] (const SymbolRef &head, const Definitions &definitions) -> RuleRef {
        return PatternMatchedBuiltinRule<N, decltype(func)>::construct(patt, func);
    };
}