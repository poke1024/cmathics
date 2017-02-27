#pragma once

typedef uint32_t MatchOptions;

class MatchContext {
public:
    enum {
        NoEndAnchor = 1
    };

    const Evaluation &evaluation;
    MatchRef match;
    const MatchOptions options;

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const Evaluation &evaluation_,
        MatchOptions options_ = 0) :

        evaluation(evaluation_),
        match(Match::construct(matcher)),
        options(options_) {
    }

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const nothing&,
        const Evaluation &evaluation,
        MatchOptions options = 0) :

        MatchContext(matcher, evaluation, options) {
    }

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const OptionsProcessorRef &options_processor,
        const Evaluation &evaluation_,
        MatchOptions options_ = 0) :

        evaluation(evaluation_),
        match(Match::construct(matcher, options_processor)),
        options(options_) {
    }

    inline void reset() {
        match->reset();
    }
};
