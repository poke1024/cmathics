// GenericPatternMatcher implements all matching stuff that is not
// handled by the optimized simple ordered matcher, i.e. Attributes
// like Flat, Orderless, etc.

template<typename Callback>
class decider {
private:
    const std::vector<size_t> &m_items;
    std::vector<size_t> m_chosen;
    std::vector<size_t> m_not_chosen;
    const Callback &m_callback;
    const bool m_orderless;

    inline index_t callback() const {
        return m_callback(m_chosen, m_not_chosen);
    }

    index_t decide(index_t index, index_t count) {
        index_t result;

        if (count < 0 || m_items.size() - index < count) {
            return -1;
        } else if (count == 0) {
            const auto old_size = m_not_chosen.size();
            m_not_chosen.insert(m_not_chosen.end(), m_items.begin() + index, m_items.end());
            result = callback();
            m_not_chosen.erase(m_not_chosen.begin() + old_size, m_not_chosen.end());
        } else if (count == m_items.size() - index) {
            const auto old_size = m_chosen.size();
            m_chosen.insert(m_chosen.end(), m_items.begin() + index, m_items.end());
            result = callback();
            m_chosen.erase(m_chosen.begin() + old_size, m_chosen.end());
        } else if (index < m_items.size()) {
            int item = m_items[index];

            m_chosen.push_back(item);
            result = decide(index + 1, count - 1);
            m_chosen.pop_back();

            if (result < 0 && m_orderless) {
                m_not_chosen.push_back(item);
                result = decide(index + 1, count);
                m_not_chosen.pop_back();
            }
        }

        return result;
    }

public:
    decider(const std::vector<size_t> &items, const Callback &callback, bool orderless) :
        m_items(items), m_callback(callback), m_orderless(orderless) {

        m_chosen.reserve(items.size());
        m_not_chosen.reserve(items.size());
    }

    index_t operator()(index_t count) {
        try {
            return decide(0, count);
        } catch (...) {
            m_chosen.clear();
            m_not_chosen.clear();
            throw;
        }
    }
};

template<typename Callback>
index_t subsets(
    const std::vector<size_t> &items,
    const Callback &callback,
    Attributes attributes) {

    decider<Callback> decide(
        items, callback, attributes & Attributes::Orderless);

    if (attributes & Attributes::Flat) {
        for (index_t i = 1; i <= items.size(); i++) {
            const index_t result = decide(i);
            if (result >= 0) {
                return result;
            }
        }
        return -1;
    } else {
        return decide(1);
    }
}

template<typename Dummy, typename MatchRest>
class GenericPatternMatcher :
    public PatternMatcher,
    public ExtendedHeapObject<GenericPatternMatcher<Dummy, MatchRest>> {
private:
    const std::vector<PatternMatcherRef> m_matchers;
    const MatchRest m_rest;

    template<typename Sequence>
    index_t match_generic(
        const Sequence &sequence,
        index_t begin,
        index_t end,
        BaseExpressionPtr head,
        Attributes attributes,
        const std::vector<size_t> &rest,
        size_t arg) const {

        if (arg == m_matchers.size()) {
            if (!rest.empty()) {
                return -1;
            } else {
                return m_rest(sequence, end, end);
            }
        }

        return subsets(rest,
            [this, &sequence, begin, end, head, attributes, arg]
            (const auto &chosen, const auto &not_chosen) -> index_t {

                UnsafeBaseExpressionRef expr;

                if (chosen.size() == 1) {
                    expr = *sequence.element(chosen[0]);
                    if (attributes & Attributes::Flat) {
                        expr = expression(head, expr)->evaluate_or_copy(
                            sequence.context().evaluation);
                    }
                } else {
                    assert(attributes & Attributes::Flat);
                    expr = expression(
                        sequence.context().evaluation.Sequence,
                        sequential([head, &sequence, &chosen] (auto &store) {
                            const size_t n = chosen.size();
                            for (size_t i = 0; i < n; i++) {
                                store(expression(head, *sequence.element(chosen[i])));
                            }
                        }, chosen.size()))->evaluate_or_copy(sequence.context().evaluation);
                }

                const auto &state = sequence.context().match;
                const size_t vars0 = state->n_slots_fixed();

                // std::cout << "trying to match " << expr->debugform() << " as argument " << arg << std::endl;

                if (m_matchers[arg]->match(FlatLeafSequence(sequence.context(), head, expr), 0, 1) == 1) {
                    const index_t match = match_generic(sequence, begin, end, head, attributes, not_chosen, arg + 1);
                    if (match >= 0) {
                        // std::cout << "match success" << std::endl;
                        return match;
                    } else {
                        // std::cout << "match failed" << std::endl;
                        state->backtrack(vars0);
                    }
                }

                return -1;
            }, attributes);
    }

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end) const {

        const BaseExpressionPtr head = sequence.head();
        assert(head);

        const Attributes attributes =
            head->lookup_name()->state().attributes();

        std::vector<size_t> indices;
        indices.reserve(end - begin);
        for (size_t i = begin; i < end; i++) {
            indices.push_back(i);
        }

        return match_generic(sequence, begin, end, head, attributes, indices, 0);
    }

public:
    inline GenericPatternMatcher(
        std::vector<PatternMatcherRef> &&matchers,
        const MatchRest &next) :

        m_matchers(matchers),
        m_rest(next) {

        PatternMatcherSize any_size(
            MatchSize::at_least(0), MatchSize::at_least(0));

        m_size = any_size;
    }

    virtual void set_size(const PatternMatcherSize &size) {
        // noop; we never overwrite the "match all" size we set
        // in our constructor.
    }

    virtual std::string name(const MatchContext &context) const {
        std::ostringstream s;
        s << "GenericPatternMatcher(), " << m_rest.name(context);
        return s.str();
    }

    DECLARE_MATCH_EXPRESSION_METHODS
    DECLARE_NO_MATCH_CHARACTER_METHODS
};
