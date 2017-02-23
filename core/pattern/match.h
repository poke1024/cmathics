#pragma once

class Match : public Shared<Match, SharedPool> {
private:
    PatternMatcherRef m_matcher;
    SlotVector m_slots;
    index_t m_slots_fixed;
    UnsafeOptionsProcessorRef m_options;

public:
    inline Match(); // only for Pool::DefaultMatch()

    inline Match(const PatternMatcherRef &matcher);

    inline Match(const PatternMatcherRef &matcher, const OptionsProcessorRef &options_processor);

    inline const UnsafeBaseExpressionRef *get_matched_value(const Symbol *variable) const {
        const index_t index = m_matcher->variables().find(variable);
        if (index >= 0) {
            assert(index < m_slots.size());
            return &m_slots[index].value;
        } else {
            return nullptr;
        }
    }

    inline void reset() {
        const size_t n = m_slots_fixed;
        for (size_t i = 0; i < n; i++) {
            assert(i < m_slots.size() && m_slots[i].index_to_ith < m_slots.size());
            m_slots[m_slots[i].index_to_ith].value.reset();
        }
        m_slots_fixed = 0;
        if (m_options) {
            m_options->reset();
        }
    }

    inline bool assign(const index_t slot_index, const BaseExpressionRef &value, bool &is_owner) {
        assert(slot_index >= 0 && slot_index < m_slots.size());
        Slot &slot = m_slots[slot_index];
        if (slot.value) {
            is_owner = false;
            return slot.value->same(value);
        } else {
            slot.value = value;
            assert(m_slots_fixed < m_slots.size());
            m_slots[m_slots_fixed++].index_to_ith = slot_index;
            is_owner = true;
            return true;
        }
    }

    inline void unassign(const index_t slot_index) {
        assert(slot_index >= 0 && slot_index < m_slots.size());
        m_slots_fixed--;
        assert(m_slots_fixed >= 0 && m_slots_fixed < m_slots.size());
        assert(m_slots[m_slots_fixed].index_to_ith == slot_index);
        m_slots[slot_index].value.reset();
    }

    inline void prepend(Match &match) {
        const index_t k = m_slots_fixed;
        const index_t n = match.m_slots_fixed;

        for (index_t i = 0; i < k; i++) {
            assert(i < m_slots.size());
            assert(i + n < m_slots.size());
            m_slots[i + n].index_to_ith = m_slots[i].index_to_ith;
        }

        for (index_t i = 0; i < n; i++) {
            assert(i < m_slots.size());
            const index_t index = match.m_slots[i].index_to_ith;
            m_slots[i].index_to_ith = index;
            assert(index < m_slots.size());
            m_slots[index].value = match.m_slots[index].value;
        }

        m_slots_fixed = n + k;
    }

    inline void backtrack(index_t n) {
        while (m_slots_fixed > n) {
            m_slots_fixed--;
            assert(m_slots_fixed >= 0 && m_slots_fixed < m_slots.size());
            const index_t index = m_slots[m_slots_fixed].index_to_ith;
            assert(index < m_slots.size());
            m_slots[index].value.reset();
        }
    }

    inline size_t n_slots_fixed() const {
        return m_slots_fixed;
    }

    inline const UnsafeBaseExpressionRef &ith_slot(index_t i) const {
        assert(i >= 0 && i < m_slots.size());
        assert(m_slots[i].index_to_ith < m_slots.size());
        return m_slots[m_slots[i].index_to_ith].value;
    }

    inline const UnsafeBaseExpressionRef &slot(index_t i) const {
        assert(i >= 0 && i < m_slots.size());
        return m_slots[i].value;
    }

    template<int N>
    typename BaseExpressionTuple<N>::type unpack() const;

    template<typename Sequence>
    inline index_t options(
        const Sequence &sequence,
        index_t begin,
        index_t end,
        const OptionsProcessor::MatchRest &rest);

    inline const OptionsMap *options() const;
};
