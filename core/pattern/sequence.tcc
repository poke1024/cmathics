#pragma once

class FastLeafSequence {
private:
    MatchContext &m_context;
    const BaseExpressionPtr m_head;
    const BaseExpressionRef * const m_array;

public:
    class Element {
    private:
        const BaseExpressionRef * const m_array;
        const index_t m_begin;

    public:
        inline Element(const BaseExpressionRef *array, index_t begin) : m_array(array), m_begin(begin) {
        }

        inline index_t begin() const {
            return m_begin;
        }

        inline const BaseExpressionRef &operator*() const {
            return m_array[m_begin];
        }
    };

    class Sequence {
    private:
        const Evaluation &m_evaluation;
        const BaseExpressionRef * const m_begin;
        const index_t m_n;
        UnsafeBaseExpressionRef m_sequence;

    public:
        inline Sequence(const Evaluation &evaluation, const BaseExpressionRef *array, index_t begin, index_t end) :
            m_evaluation(evaluation), m_begin(array + begin), m_n(end - begin) {
        }

        inline const UnsafeBaseExpressionRef &operator*() {
            if (!m_sequence) {
                const BaseExpressionRef * const begin = m_begin;
                const index_t n = m_n;
                m_sequence = expression(m_evaluation.Sequence, sequential([begin, n] (auto &store) {
                    for (index_t i = 0; i < n; i++) {
                        store(BaseExpressionRef(begin[i]));
                    }
                }, n));
            }
            return m_sequence;
        }
    };

    inline FastLeafSequence(MatchContext &context, BaseExpressionPtr head, const BaseExpressionRef *array) :
        m_context(context), m_head(head), m_array(array) {
    }

    inline MatchContext &context() const {
        return m_context;
    }

    inline BaseExpressionPtr head() const {
        return m_head;
    }

    inline Element element(index_t begin) const {
        return Element(m_array, begin);
    }

    inline Sequence slice(index_t begin, index_t end) const {
        assert(begin <= end);
        return Sequence(m_context.evaluation, m_array, begin, end);
    }

    inline index_t same(index_t begin, BaseExpressionPtr other) const {
        const BaseExpressionPtr expr = m_array[begin].get();
        if (other == expr || other->same(expr)) {
            return begin + 1;
        } else {
            return -1;
        }
    }
};

class SlowLeafSequence {
private:
    MatchContext &m_context;
    const Expression * const m_expr;

public:
    class Element {
    private:
        const Expression * const m_expr;
        const index_t m_begin;
        UnsafeBaseExpressionRef m_element;

    public:
        inline Element(const Expression *expr, index_t begin) : m_expr(expr), m_begin(begin) {
        }

        inline index_t begin() const {
            return m_begin;
        }

        inline const UnsafeBaseExpressionRef &operator*() {
            if (!m_element) {
                m_element = m_expr->materialize_leaf(m_begin);
            }
            return m_element;
        }
    };

    class Sequence {
    private:
        const Evaluation &m_evaluation;
        const Expression * const m_expr;
        const index_t m_begin;
        const index_t m_end;
        UnsafeBaseExpressionRef m_sequence;

    public:
        inline Sequence(const Evaluation &evaluation, const Expression *expr, index_t begin, index_t end) :
            m_evaluation(evaluation), m_expr(expr), m_begin(begin), m_end(end) {
        }

        inline const UnsafeBaseExpressionRef &operator*() {
            if (!m_sequence) {
                m_sequence = m_expr->slice(m_evaluation.Sequence, m_begin, m_end);
            }
            return m_sequence;
        }
    };

    inline SlowLeafSequence(MatchContext &context, const Expression *expr) :
        m_context(context), m_expr(expr) {
    }

    inline MatchContext &context() const {
        return m_context;
    }

    inline BaseExpressionPtr head() const {
        return m_expr->head();
    }

    inline Element element(index_t begin) const {
        return Element(m_expr, begin);
    }

    inline Sequence slice(index_t begin, index_t end) const {
        assert(begin <= end);
        return Sequence(m_context.evaluation, m_expr, begin, end);
    }

    inline index_t same(index_t begin, BaseExpressionPtr other) const {
        if (other->same(m_expr->materialize_leaf(begin))) {
            return begin + 1;
        } else {
            return -1;
        }
    }
};