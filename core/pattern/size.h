#pragma once

typedef int64_t match_size_t; // needs to be signed
constexpr match_size_t MATCH_MAX = INT64_MAX >> 2; // safe for subtractions

class MatchSize {
private:
    match_size_t _min;
    match_size_t _max;

    inline MatchSize(match_size_t min, match_size_t max) : _min(min), _max(max) {
    }

public:
    inline MatchSize() {
    }

    static inline MatchSize exactly(match_size_t n) {
        return MatchSize(n, n);
    }

    static inline MatchSize at_least(match_size_t n) {
        return MatchSize(n, MATCH_MAX);
    }

    static inline MatchSize between(match_size_t min, match_size_t max) {
        return MatchSize(min, max);
    }

    inline match_size_t min() const {
        return _min;
    }

    inline match_size_t max() const {
        return _max;
    }

    inline bool contains(match_size_t s) const {
        return s >= _min && s <= _max;
    }

    inline optional<size_t> fixed_size() const {
        if (_min == _max) { // i.e. a finite, fixed integer
            return _min;
        } else {
            return optional<size_t>();
        }
    }

    inline bool matches(SliceCode code) const {
        if (is_tiny_slice(code)) {
            return contains(tiny_slice_size(code));
        } else {
            // inspect wrt minimum size of non-static slice
            const size_t min_size = MaxTinySliceSize + 1;
            return _max >= min_size;
        }
    }

    inline MatchSize &operator+=(const MatchSize &size) {
        _min += size._min;
        if (_max == MATCH_MAX || size._max == MATCH_MAX) {
            _max = MATCH_MAX;
        } else {
            _max += size._max;
        }
        return *this;
    }
};

typedef std::experimental::optional<MatchSize> OptionalMatchSize;

class PatternMatcherSize {
private:
    MatchSize m_from_here;
    MatchSize m_from_next;

public:
    inline PatternMatcherSize() {
    }

    inline PatternMatcherSize(
        const MatchSize &from_here,
        const MatchSize &from_next) :
        m_from_here(from_here),
        m_from_next(from_next) {
    }

    inline const MatchSize &from_here() const {
        return m_from_here;
    }

    inline const MatchSize &from_next() const {
        return m_from_next;
    }
};
