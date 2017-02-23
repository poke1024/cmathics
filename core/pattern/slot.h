#pragma once

struct Slot {
    // there are two kind of slot indices: (1) the order in which the slots were
    // ordered when compiling the pattern (this is the natural order of the Slot
    // array in class Match). (2) the order in which slots are filled when an
    // expression is matched (this is implemented using index_to_ith).

    UnsafeBaseExpressionRef value; // slot for variable #i
    index_t index_to_ith; // index of i-th fixed slot
};

using SlotAllocator = VectorAllocator<Slot>;

class SlotVector {
private:
    enum {
        n_preallocated = 8
    };

    Slot m_slots[n_preallocated];
    Slot *m_slots_ptr;
    const size_t m_size;
    optional<std::vector<Slot, SlotAllocator>> m_vector;

public:
    inline SlotVector() : m_size(0) {
    }

    inline SlotVector(size_t size);

    inline size_t size() const {
        return m_size;
    }

    inline const Slot &operator[](size_t index) const {
        return m_slots_ptr[index];
    }

    inline Slot &operator[](size_t index) {
        return m_slots_ptr[index];
    }
};

struct SlotDirective {
    enum Action {
        Slot,
        OptionValue,
        Copy,
        Descend,
        IllegalSlot
    };

    const Action m_action;
    const index_t m_slot;
    const SymbolRef m_option_value;
    const BaseExpressionRef m_illegal_slot;

    inline SlotDirective(
        Action action,
        index_t slot = 0,
        const SymbolRef &option = SymbolRef(),
        const BaseExpressionRef &illegal_slot = BaseExpressionRef()) :

        m_action(action),
        m_slot(slot),
        m_option_value(option),
        m_illegal_slot(illegal_slot) {
    }

    inline static SlotDirective slot(index_t slot) {
        return SlotDirective(SlotDirective::Slot, slot);
    }

    inline static SlotDirective option_value(const SymbolRef &option) {
        return SlotDirective(SlotDirective::OptionValue, -1, option);
    }

    inline static SlotDirective copy() {
        return SlotDirective(SlotDirective::Copy);
    }

    inline static SlotDirective illegal_slot(const BaseExpressionRef &slot) {
        return SlotDirective(SlotDirective::IllegalSlot, 0, SymbolRef(), slot);
    }

    inline static SlotDirective descend() {
        return SlotDirective(SlotDirective::Descend);
    }
};
