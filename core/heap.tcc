class Match {
private:
	PatternMatcherRef m_matcher;
	std::vector<Slot, SlotAllocator> m_slots;
	index_t m_slots_fixed;

protected:
	std::atomic<size_t> m_ref_count;

	friend void intrusive_ptr_add_ref(Match *match);
	friend void intrusive_ptr_release(Match *match);


public:
	inline Match() :
		m_ref_count(0),
		m_slots_fixed(0) {
	}

	inline Match(const PatternMatcherRef &matcher) :
		m_ref_count(0),
		m_matcher(matcher),
		m_slots(matcher->variables().size(), Heap::slots_allocator()),
		m_slots_fixed(0) {
	}

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

	inline BaseExpressionRef slot(index_t i) const {
		return m_slots[m_slots[i].index_to_ith].value;
	}

	template<int N>
	typename BaseExpressionTuple<N>::type unpack() const;
};

inline void intrusive_ptr_add_ref(Match *match) {
	match->m_ref_count++;
}

inline void intrusive_ptr_release(Match *match) {
	if (--match->m_ref_count == 0) {
		Heap::release(match);
	}
}

inline SymbolRef Heap::Symbol(const char *name, ExtendedType type) {
	assert(_s_instance);
	return SymbolRef(_s_instance->_symbols.construct(name, type));
}

inline BaseExpressionRef Heap::MachineInteger(machine_integer_t value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_machine_integers.construct(value));
}

inline BaseExpressionRef Heap::MachineReal(machine_real_t value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_machine_reals.construct(value));
}

inline BaseExpressionRef Heap::BigInteger(const mpz_class &value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_integers.construct(value));
}

inline BaseExpressionRef Heap::BigInteger(mpz_class &&value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_integers.construct(value));
}

inline BaseExpressionRef Heap::BigReal(machine_real_t value, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(value, prec));
}

inline BaseExpressionRef Heap::BigReal(const SymbolicForm &form, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(form, prec));
}

inline BaseExpressionRef Heap::BigReal(arb_t value, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(value, prec));
}

inline BaseExpressionRef Heap::BigRational(machine_integer_t x, machine_integer_t y) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_rationals.construct(x, y));
}

inline BaseExpressionRef Heap::BigRational(const mpq_class &value) {
	assert(_s_instance);
	return _s_instance->_big_rationals.construct(value);
}

inline StaticExpressionRef<0> Heap::EmptyExpression(const BaseExpressionRef &head) {
	return StaticExpression<0>(head, EmptySlice());
}

inline DynamicExpressionRef Heap::Expression(const BaseExpressionRef &head, const DynamicSlice &slice) {
	assert(_s_instance);
	return DynamicExpressionRef(_s_instance->_dynamic_expressions.construct(head, slice));
}

inline StringRef Heap::String(std::string &&utf8) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(utf8));
}

inline StringRef Heap::String(const StringExtentRef &extent) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(extent));
}

inline StringRef Heap::String(const StringExtentRef &extent, size_t offset, size_t length) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(extent, offset, length));
}

inline void Heap::release(BaseExpression *expr) {
	switch (expr->type()) {
		case SymbolType:
			_s_instance->_symbols.free(static_cast<class Symbol*>(expr));
			break;

		case MachineIntegerType:
			_s_instance->_machine_integers.free(static_cast<class MachineInteger*>(expr));
			break;

		case BigIntegerType:
			_s_instance->_big_integers.free(static_cast<class BigInteger*>(expr));
			break;

		case BigRationalType:
			_s_instance->_big_rationals.free(static_cast<class BigRational*>(expr));
			break;

		case MachineRealType:
			_s_instance->_machine_reals.free(static_cast<class MachineReal*>(expr));
			break;

		case BigRealType:
			_s_instance->_big_reals.free(static_cast<class BigReal*>(expr));
			break;

		case ExpressionType: {
			const SliceCode code = static_cast<const class Expression*>(expr)->slice_code();
			if (is_static_slice(code)) {
				_s_instance->_static_expression_heap.free(expr, code);
			} else if (code == SliceCode::DynamicSliceCode) {
				_s_instance->_dynamic_expressions.free(
					static_cast<ExpressionImplementation<DynamicSlice>*>(expr));
			} else if (is_packed_slice(code)) {
				delete expr;
			} else {
				throw std::runtime_error("encountered unsupported slice type id");
			}
			break;
		}

		case StringType: {
			_s_instance->_strings.free(static_cast<class String*>(expr));
			break;
		}

		default:
			delete expr;
			break;
	}
}

inline MatchRef Heap::Match(const PatternMatcherRef &matcher) {
	return _s_instance->_matches.construct(matcher);
}

inline MatchRef Heap::DefaultMatch() {
	return _s_instance->_default_match;
}

inline BaseExpressionRef from_primitive(machine_integer_t value) {
	return Heap::MachineInteger(value);
}

static_assert(sizeof(long) == sizeof(machine_integer_t),
	"types long and machine_integer_t must not differ");

inline BaseExpressionRef from_primitive(const mpz_class &value) {
	if (value.fits_slong_p()) {
		return Heap::MachineInteger(machine_integer_t(value.get_si()));
	} else {
		return Heap::BigInteger(value);
	}
}

inline BaseExpressionRef from_primitive(mpz_class &&value) {
	if (value.fits_slong_p()) {
		return Heap::MachineInteger(machine_integer_t(value.get_si()));
	} else {
		return Heap::BigInteger(value);
	}
}

inline BaseExpressionRef from_primitive(machine_real_t value) {
	return Heap::MachineReal(value);
}

inline BaseExpressionRef from_primitive(const mpq_class &value) {
	return Heap::BigRational(value);
}

inline BaseExpressionRef from_primitive(const Numeric::Z &value) {
	return value.to_expression();
}

inline void intrusive_ptr_add_ref(const BaseExpression *expr) {
	++expr->_ref_count;
}

inline void intrusive_ptr_release(const BaseExpression *expr) {
	if(--expr->_ref_count == 0) {
		Heap::release(const_cast<BaseExpression*>(expr));
	}
}
