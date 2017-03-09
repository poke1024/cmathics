#include "attributes.h"

class NotASymbol : public std::runtime_error {
private:
	BaseExpressionRef m_item;

public:
	NotASymbol(const BaseExpressionRef &item) :
		std::runtime_error("not a symbol"), m_item(item) {
	}

	const BaseExpressionRef &item() const {
		return m_item;
	}
};

template<typename F>
auto for_each_symbol(const BaseExpressionPtr list, const F &f) {
	const auto &emit = [&f] (const BaseExpressionRef &leaf) {
		if (leaf->is_symbol()) {
			f(leaf->as_symbol());
		} else {
			throw NotASymbol(leaf);
		}
	};

	if (list->is_list()) {
		list->as_expression()->with_slice([&emit] (const auto &slice) {
			for (auto leaf : slice) {
				emit(leaf);
			}
		});
	} else {
		emit(list);
	}
}

class AbstractAttributesConverter {
protected:
	virtual void add(Attributes p, Attributes n, SymbolPtr s) = 0;

	inline void add(Attributes p, SymbolPtr s) {
		add(p, Attributes::None, s);
	}

	const Symbols *m_symbols;

public:
	void initialize(const Symbols &symbols) {
		m_symbols = &symbols;

		// sorted alphabetically!
		add(Attributes::Constant, symbols.Constant);
		add(Attributes::Flat, symbols.Flat);
		add(Attributes::HoldAll, symbols.HoldAll);
		add(Attributes::HoldAllComplete, symbols.HoldAllComplete);
		add(Attributes::HoldFirst, Attributes::HoldAll, symbols.HoldFirst);
		add(Attributes::HoldRest, Attributes::HoldAll, symbols.HoldRest);
		add(Attributes::Listable, symbols.Listable);
		add(Attributes::Locked, symbols.Locked);
		add(Attributes::NHoldAll, symbols.NHoldAll);
		add(Attributes::NHoldFirst, Attributes::NHoldAll, symbols.NHoldFirst);
		add(Attributes::NHoldRest, Attributes::NHoldAll, symbols.NHoldRest);
		add(Attributes::NumericFunction, symbols.NumericFunction);
		add(Attributes::OneIdentity, symbols.OneIdentity);
		add(Attributes::Orderless, symbols.Orderless);
		add(Attributes::Protected, symbols.Protected);
		add(Attributes::ReadProtected, symbols.ReadProtected);
		add(Attributes::SequenceHold, symbols.SequenceHold);
	}
};

class AttributesToList : public AbstractAttributesConverter {
protected:
	struct Pair {
		Attributes positive;
		Attributes negative;
		SymbolPtr symbol;
	};

	std::vector<Pair> m_attributes;

	virtual void add(Attributes p, Attributes n, SymbolPtr s) {
		m_attributes.emplace_back(Pair{p, n, s});
	}

public:
	BaseExpressionRef operator()(const Attributes attributes) const {
		TemporaryRefVector list;

		for (const Pair &p : m_attributes) {
			if ((attributes & p.positive) && !(attributes & p.negative)) {
				list.push_back(p.symbol);
			}
		}

		return list.to_expression(m_symbols->List);
	}
};

class ListToAttributes : public AbstractAttributesConverter {
protected:
	std::unordered_map<SymbolName, Attributes>  m_attributes;

	virtual void add(Attributes p, Attributes n, SymbolPtr s) {
		m_attributes[s->symbol()] = p;
	}

public:
	Attributes operator()(const BaseExpressionPtr list) const {
		Attributes attributes = Attributes::None;

		for_each_symbol(list, [this, &attributes] (SymbolPtr s) {
			const auto i = m_attributes.find(s->symbol());
			if (i != m_attributes.end()) {
				attributes = attributes + i->second;
			}
		});

		return attributes;
	}
};

class AttributesBuiltin : public Builtin {
public:
	static constexpr const char *name = "Attributes";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Attributes'[$symbol$]
        <dd>returns the attributes of $symbol$.
    <dt>'Attributes'[$symbol$] = {$attr1$, $attr2$}
        <dd>sets the attributes of $symbol$, replacing any existing attributes.
    </dl>

    >> Attributes[Plus]
     = {Flat, Listable, NumericFunction, OneIdentity, Orderless, Protected}
    'Attributes' always considers the head of an expression:
    >> Attributes[a + b + c]
     = {Flat, Listable, NumericFunction, OneIdentity, Orderless, Protected}
	)";

	static constexpr auto attributes =
		Attributes::HoldAll + Attributes::Listable;

private:
	AttributesToList m_converter;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&AttributesBuiltin::apply);
		m_converter.initialize(runtime.symbols());
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		const SymbolPtr name = expr->lookup_name();
		if (name) {
			const Attributes attributes = name->state().attributes();
			return m_converter(attributes);
		} else {
			return evaluation.definitions.empty_list;
		}
	}
};

class ModifyAttributes : public Builtin {
protected:
	virtual void modify(SymbolPtr symbol, Attributes attributes, const Evaluation &evaluation) const = 0;

private:
	ListToAttributes m_converter;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&ModifyAttributes::apply);
		m_converter.initialize(runtime.symbols());
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr symbols,
		BaseExpressionPtr attributes_list,
		const Evaluation &evaluation) {

		Attributes attributes;

		try {
			attributes = m_converter(attributes_list);
		} catch (const NotASymbol &e) {
			evaluation.message(m_symbol, "sym", e.item(), MachineInteger::construct(2));
			return BaseExpressionRef();
		}

		try {
			for_each_symbol(symbols, [this, attributes, &evaluation] (const SymbolPtr s) {
				if (s->state().attributes() & Attributes::Locked) {
					evaluation.message(m_symbol, "locked", s);
				} else {
					modify(s, attributes, evaluation);
				}
			});
		} catch (const NotASymbol &e) {
			evaluation.message(m_symbol, "sym", e.item(), MachineInteger::construct(1));
			return BaseExpressionRef();
		}

		return evaluation.Null;
	}
};

class SetAttributes : public ModifyAttributes {
public:
	static constexpr const char *name = "SetAttributes";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'SetAttributes'[$symbol$, $attrib$]
        <dd>adds $attrib$ to $symbol$'s attributes.
    </dl>

    >> SetAttributes[f, Flat]
    >> Attributes[f]
     = {Flat}

    Multiple attributes can be set at the same time using lists:
    >> SetAttributes[{f, g}, {Flat, Orderless}]
    >> Attributes[g]
     = {Flat, Orderless}
	)";

	static constexpr auto attributes = Attributes::HoldFirst;

	using ModifyAttributes::ModifyAttributes;

protected:
	virtual void modify(SymbolPtr symbol, Attributes attributes, const Evaluation &evaluation) const {
		symbol->mutable_state().add_attributes(attributes, evaluation.definitions);
	}
};

class ClearAttributes : public ModifyAttributes {
public:
	static constexpr const char *name = "ClearAttributes";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'ClearAttributes'[$symbol$, $attrib$]
        <dd>removes $attrib$ from $symbol$'s attributes.
    </dl>

    >> SetAttributes[f, Flat]
    >> Attributes[f]
     = {Flat}
    >> ClearAttributes[f, Flat]
    >> Attributes[f]
     = {}
    Attributes that are not even set are simply ignored:
    >> ClearAttributes[{f}, {Flat}]
    >> Attributes[f]
     = {}
	)";

	static constexpr auto attributes = Attributes::HoldFirst;

	using ModifyAttributes::ModifyAttributes;

protected:
	virtual void modify(SymbolPtr symbol, Attributes attributes, const Evaluation &evaluation) const {
		symbol->mutable_state().remove_attributes(attributes, evaluation.definitions);
	}
};

class Protect : public Builtin {
public:
	static constexpr const char *name = "Protect";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Protect'[$symbol$]
        <dd>gives $symbol$ the attribute 'Protected'.
    </dl>
	)";

	static constexpr auto attributes = Attributes::HoldAll;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Protect[symbols__]", "SetAttributes[{symbols}, Protected]");
	}
};

class Unprotect : public Builtin {
public:
	static constexpr const char *name = "Unprotect";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Unprotect'[$symbol$]
        <dd>removes the 'Protected' attribute from $symbol$.
    </dl>
	)";

	static constexpr auto attributes = Attributes::HoldAll;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Unprotect[symbols__]", "ClearAttributes[{symbols}, Protected]");
	}
};

void Builtins::Attributes::initialize() {
	add<AttributesBuiltin>();
	add<SetAttributes>();
	add<ClearAttributes>();
	add<Protect>();
	add<Unprotect>();
}
