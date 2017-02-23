#pragma once

#include "options.tcc"
#include "unpack.h"

template<int N>
inline typename BaseExpressionTuple<N>::type Match::unpack() const {
    typename BaseExpressionTuple<N>::type t;
    unpack_symbols<N, 0>()(*this, 0, t);
    return t;
};

template<typename Sequence>
inline index_t Match::options(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const OptionsProcessor::MatchRest &rest) {

    if (!m_options) {
        m_options = Pool::DynamicOptionsProcessor();
    }

    return m_options->match(sequence, begin, end, rest);
}

inline const OptionsMap *Match::options() const {
    if (!m_options) {
        return nullptr;
    }
    if (m_options.get()->type == OptionsProcessor::Dynamic) {
        return &static_cast<DynamicOptionsProcessor*>(m_options.get())->options();
    } else {
        return nullptr;
    }
}