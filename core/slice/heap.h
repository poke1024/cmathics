#pragma once

template<typename Generator>
class TinyExpressionFactory {
public:
    std::function<ExpressionRef(const BaseExpressionRef&, const Generator&)> m_construct[MaxTinySliceSize + 1];

    template<int N>
    void initialize() {
        m_construct[N] = [] (const BaseExpressionRef &head, const Generator &f) {
            return ExpressionImplementation<TinySlice<N>>::construct(head, f);
        };

        STATIC_IF (N >= 1) {
            initialize<N-1>();
        } STATIC_ENDIF
    }

public:
    inline TinyExpressionFactory() {
        initialize<MaxTinySliceSize>();
    }

    inline ExpressionRef operator()(const BaseExpressionRef& head, const Generator &f) const {
        return m_construct[f.size()](head, f);
    }
};

template<typename Generator>
inline ExpressionRef tiny_expression(const BaseExpressionRef& head, const Generator &f) {
    static TinyExpressionFactory<Generator> factory;
    return factory(head, f);
}

