#pragma once

template<int M, int N>
struct unpack_symbols {
    void operator()(
        const Match &match,
        const index_t index,
        typename BaseExpressionTuple<M>::type &t) {

        // symbols are already ordered in the order of their (first) appearance in the original pattern.
        assert(index != match.n_slots_fixed());
        std::get<N>(t) = match.ith_slot(index).get();
        unpack_symbols<M, N + 1>()(match, index + 1, t);
    }
};

template<int M>
struct unpack_symbols<M, M> {
    void operator()(
        const Match &match,
        const index_t index,
        typename BaseExpressionTuple<M>::type &t) {

        assert(index == match.n_slots_fixed());
    }
};
