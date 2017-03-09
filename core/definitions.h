#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "core/atoms/symbol.h"
#include "numberform.h"
#include <unordered_map>

class Definitions;

class Symbols {
private:
	struct {
	} _dummy;

public:
	Symbols(Definitions &definitions);

	inline Symbols(const Symbols &symbols) {
		std::memcpy(this, &symbols, sizeof(symbols));
	}

	#define SYMBOL(SYMBOLNAME) const Symbol *SYMBOLNAME;
	#include "system_symbols.h"
	#undef SYMBOL

	inline const Symbol *Boolean(bool x) const {
		return x ? True : False;
	}
};

template<typename T>
class BinaryOperator;

class Definitions {
public:
	void update_master_version();

	VersionRef master_version() const;

	inline void update_version() {
		::update_definitions_version(*this);
	}

	inline VersionRef version() const {
		return ::definitions_version(*this);
	}

private:
    std::unordered_map<SymbolKey, MutableSymbolRef> m_definitions;

	const Symbols m_symbols;

	UnsafeSharedPtr<Version> m_master_version;

protected:
	friend class Symbols;

	SymbolRef new_symbol(const char *name, SymbolName symbol = S::GENERIC);

	const Symbol *system_symbol(const char *name, SymbolName symbol);

public:
    Definitions();

	virtual ~Definitions();

    SymbolRef lookup(const char *name);

	inline SymbolRef lookup_no_create(const char *name) const {
		const auto it = m_definitions.find(SymbolKey(name));

		if (it == m_definitions.end()) {
			return SymbolRef();
		} else {
			return it->second;
		}
	}

	inline const Symbols &symbols() const {
		return m_symbols;
	}

	void freeze_as_builtin();

    void reset_user_definitions();

	const NumberFormatter number_form;

    const BaseExpressionRef zero;
    const BaseExpressionRef one;
    const BaseExpressionRef minus_one;

    const SymbolicFormRef no_symbolic_form;
    const MatchRef default_match;
    const ExpressionRef empty_list;

	const BinaryOperator<struct order> *order;
};

#endif
