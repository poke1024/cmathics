#ifndef PATTERN_H
#define PATTERN_H

struct SlotDirective {
    enum Action {
        Slot,
        Copy,
        Descend
    };

    const Action m_action;
    const index_t m_slot;

    inline SlotDirective(Action action, index_t slot = 0) :
        m_action(action), m_slot(slot) {
    }

    inline static SlotDirective slot(index_t slot) {
        return SlotDirective(SlotDirective::Slot, slot);
    }

    inline static SlotDirective copy() {
        return SlotDirective(SlotDirective::Copy);
    }

    inline static SlotDirective descend() {
        return SlotDirective(SlotDirective::Descend);
    }
};

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

class CompiledVariables {
private:
    VariableMap<index_t> m_indices;
    std::list<SymbolRef> m_symbols;

protected:
    friend class PatternFactory;

    index_t lookup_slot(const SymbolRef &variable) {
        const auto i = m_indices.find(variable.get());
        if (i != m_indices.end()) {
            return i->second;
        } else {
            m_symbols.push_back(variable);
            const index_t index = m_indices.size();
            m_indices[variable.get()] = index;
            return index;
        }
    }

public:
    inline index_t find(const Symbol *variable) const {
        const auto i = m_indices.find(variable);
        if (i != m_indices.end()) {
            return i->second;
        } else {
            return -1;
        }
    }

    inline size_t size() const {
        return m_indices.size();
    }

    inline bool empty() const {
        return m_indices.empty();
    }
};

class CompiledArguments { // for use with class FunctionBody
private:
    const CompiledVariables &m_variables;

public:
    inline CompiledArguments(const CompiledVariables &variables) :
        m_variables(variables) {
    }

    inline SlotDirective operator()(const BaseExpressionRef &item) const;
};

class HeadLeavesMatcher;

class FastLeafSequence;
class SlowLeafSequence;
class AsciiCharacterSequence;
class SimpleCharacterSequence;
class ComplexCharacterSequence;

class PatternMatcher : public Shared<PatternMatcher, SharedHeap> {
protected:
    PatternMatcherSize m_size;
    CompiledVariables m_variables;

public:
    inline void set_size(const PatternMatcherSize &size) {
        m_size = size;
    }

    inline void set_variables(const CompiledVariables &variables) {
        m_variables = variables;
    }

    inline const CompiledVariables &variables() const {
        return m_variables;
    }

    inline PatternMatcher() {
    }

    virtual ~PatternMatcher() {
    }

    inline bool might_match(size_t size) const {
        return m_size.from_here().contains(size);
    }

    inline optional<size_t> fixed_size() const {
        return m_size.from_here().fixed_size();
    }

    virtual const HeadLeavesMatcher *head_leaves_matcher() const {
        return nullptr;
    }

    virtual index_t match(
        const FastLeafSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const SlowLeafSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const AsciiCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const SimpleCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    virtual index_t match(
        const ComplexCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    index_t match(
        MatchContext &context,
        const String *string,
        index_t begin,
        index_t end) const;
};

typedef boost::intrusive_ptr<PatternMatcher> PatternMatcherRef;

struct Slot {
    // there are two kind of slot indices: (1) the order in which the slots were
    // ordered when compiling the pattern (this is the natural order of the Slot
    // array in class Match). (2) the order in which slots are filled when an
    // expression is matched (this is implemented using index_to_ith).

    BaseExpressionRef value; // slot for variable #i
    index_t index_to_ith; // index of i-th fixed slot
};

using SlotAllocator = boost::pool_allocator<Slot>;

class Match : public Shared<Match, SharedPool> {
private:
    PatternMatcherRef m_matcher;
    std::vector<Slot, SlotAllocator> m_slots;
    index_t m_slots_fixed;

public:
    inline Match() :
        m_slots_fixed(0) {
    }

    inline Match(const PatternMatcherRef &matcher);

    inline const BaseExpressionRef *get_matched_value(const Symbol *variable) const {
        const index_t index = m_matcher->variables().find(variable);
        if (index >= 0) {
            return &m_slots[index].value;
        } else {
            return nullptr;
        }
    }

    inline void reset() {
        const size_t n = m_slots_fixed;
        for (size_t i = 0; i < n; i++) {
            m_slots[m_slots[i].index_to_ith].value.reset();
        }
        m_slots_fixed = 0;
    }

    inline bool assign(const index_t slot_index, const BaseExpressionRef &value) {
        Slot &slot = m_slots[slot_index];
        if (slot.value) {
            return slot.value->same(value);
        } else {
            slot.value = value;
            m_slots[m_slots_fixed++].index_to_ith = slot_index;
            return true;
        }
    }

    inline void unassign(const index_t slot_index) {
        m_slots_fixed--;
        assert(m_slots[m_slots_fixed].index_to_ith == slot_index);
        m_slots[slot_index].value.reset();
    }

    inline void prepend(Match &match) {
        const index_t k = m_slots_fixed;
        const index_t n = match.m_slots_fixed;

        for (index_t i = 0; i < k; i++) {
            m_slots[i + n].index_to_ith = m_slots[i].index_to_ith;
        }

        for (index_t i = 0; i < n; i++) {
            const index_t index = match.m_slots[i].index_to_ith;
            m_slots[i].index_to_ith = index;
            m_slots[index].value = match.m_slots[index].value;
        }

        m_slots_fixed = n + k;
    }

    inline void backtrack(index_t n) {
        while (m_slots_fixed > n) {
            m_slots_fixed--;
            const index_t index = m_slots[m_slots_fixed].index_to_ith;
            m_slots[index].value.reset();
        }
    }

    inline size_t n_slots_fixed() const {
        return m_slots_fixed;
    }

    inline const BaseExpressionRef &ith_slot(index_t i) const {
        return m_slots[m_slots[i].index_to_ith].value;
    }

    inline const BaseExpressionRef &slot(index_t i) const {
        return m_slots[i].value;
    }

    template<int N>
    typename BaseExpressionTuple<N>::type unpack() const;
};

#endif
