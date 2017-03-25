#pragma once

typedef uint32_t MatchOptions;

class Match;

typedef ConstSharedPtr<Match> MatchRef;
typedef UnsafeSharedPtr<Match> UnsafeMatchRef;
typedef QuasiConstSharedPtr<Match> CachedMatchRef;

class PatternMatcher;

typedef ConstSharedPtr<PatternMatcher> PatternMatcherRef;
typedef QuasiConstSharedPtr<PatternMatcher> CachedPatternMatcherRef;
// typedef SharedPtr<PatternMatcher> MutablePatternMatcherRef;
typedef UnsafeSharedPtr<PatternMatcher> UnsafePatternMatcherRef;

class OptionsProcessor;

using OptionsProcessorRef = ConstSharedPtr<OptionsProcessor>;
using UnsafeOptionsProcessorRef = UnsafeSharedPtr<OptionsProcessor>;

class MatchContext {
public:
    enum {
        NoEndAnchor = 1,
        IgnoreCase = 2
    };

    const Evaluation &evaluation;
    MatchRef match;
    const MatchOptions options;

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const Evaluation &evaluation_,
        MatchOptions options_ = 0);

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const nothing&,
        const Evaluation &evaluation,
        MatchOptions options = 0);

    inline MatchContext(
        const PatternMatcherRef &matcher,
        const OptionsProcessorRef &options_processor,
        const Evaluation &evaluation_,
        MatchOptions options_ = 0);

    inline void reset();
};
