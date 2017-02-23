#pragma once

#include <list>
#include "slot.h"

class CompiledVariables {
private:
	std::unordered_map<UnsafeSymbolRef, index_t, SymbolHash, SymbolEqual> m_indices;
    //SymbolPtrMap<index_t> m_indices;
    std::list<UnsafeSymbolRef> m_symbols;

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
	inline CompiledVariables() {
	}

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
