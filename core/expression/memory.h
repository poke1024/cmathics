#pragma once

// this specifies the memory layout of Expressions

template<typename Slice>
class ExpressionImplementation :
    public Expression,
    public PoolObject<ExpressionImplementation<Slice>> {

private:
    const Slice m_slice;

public:
    inline ExpressionImplementation(const BaseExpressionRef &head, const Slice &slice) :
        Expression(head, Slice::code(), &m_slice), m_slice(slice) {
        assert(head);
    }

    inline ExpressionImplementation(const BaseExpressionRef &head) :
        Expression(head, Slice::code(), &m_slice), m_slice() {
        assert(head);
    }

    template<typename F>
    inline ExpressionImplementation(const BaseExpressionRef &head, const FSGenerator<F> &generator) :
        Expression(head, Slice::code(), &m_slice), m_slice(generator) {
    }

    template<typename F>
    inline ExpressionImplementation(const BaseExpressionRef &head, const FPGenerator<F> &generator) :
        Expression(head, Slice::code(), &m_slice), m_slice(generator) {
    }

    inline const Slice &slice() const {
        return m_slice;
    }
};
