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

inline MatchId Heap::MatchId() {
	return _s_instance->_match_ids.construct();
}

inline void Heap::release(MatchIdData *data) {
	_s_instance->_match_ids.free(data);
}

inline void intrusive_ptr_add_ref(const BaseExpression *expr) {
	++expr->_ref_count;
}

inline void intrusive_ptr_release(const BaseExpression *expr) {
	if(--expr->_ref_count == 0) {
		Heap::release(const_cast<BaseExpression*>(expr));
	}
}

inline void intrusive_ptr_add_ref(MatchIdData *data) {
	data->m_ref_count++;
}

inline void intrusive_ptr_release(MatchIdData *data) {
	if (--data->m_ref_count == 0) {
		Heap::release(data);
	}
}
