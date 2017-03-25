#pragma once

inline MatchContext::MatchContext(
    const PatternMatcherRef &matcher,
    const Evaluation &evaluation_,
    MatchOptions options_) :

    evaluation(evaluation_),
    match(Match::construct(matcher)),
    options(options_) {
}

inline MatchContext::MatchContext(
    const PatternMatcherRef &matcher,
    const nothing&,
    const Evaluation &evaluation,
    MatchOptions options) :

    MatchContext(matcher, evaluation, options) {
}

inline MatchContext::MatchContext(
    const PatternMatcherRef &matcher,
    const OptionsProcessorRef &options_processor,
    const Evaluation &evaluation_,
    MatchOptions options_) :

    evaluation(evaluation_),
    match(Match::construct(matcher, options_processor)),
    options(options_) {
}

inline void MatchContext::reset() {
    match->reset();
}
