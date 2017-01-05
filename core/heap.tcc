inline SymbolRef Pool::Symbol(const char *name, ExtendedType type) {
	assert(_s_instance);
	return SymbolRef(_s_instance->_symbols.construct(name, type));
}

inline BaseExpressionRef Pool::MachineInteger(machine_integer_t value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_machine_integers.construct(value));
}

inline BaseExpressionRef Pool::MachineReal(machine_real_t value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_machine_reals.construct(value));
}

inline BaseExpressionRef Pool::MachineComplex(machine_real_t real, machine_real_t imag) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_machine_complexs.construct(real, imag));
}

inline BaseExpressionRef Pool::BigInteger(const mpz_class &value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_integers.construct(value));
}

inline BaseExpressionRef Pool::BigInteger(mpz_class &&value) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_integers.construct(value));
}

inline BaseExpressionRef Pool::BigReal(machine_real_t value, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(value, prec));
}

inline BaseExpressionRef Pool::BigReal(const SymbolicFormRef &form, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(form, prec));
}

inline BaseExpressionRef Pool::BigReal(arb_t value, const Precision &prec) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_reals.construct(value, prec));
}

inline BaseExpressionRef Pool::BigRational(machine_integer_t x, machine_integer_t y) {
	assert(_s_instance);
	return BaseExpressionRef(_s_instance->_big_rationals.construct(x, y));
}

inline BaseExpressionRef Pool::BigRational(const mpq_class &value) {
	assert(_s_instance);
	return _s_instance->_big_rationals.construct(value);
}

inline StaticExpressionRef<0> Pool::EmptyExpression(const BaseExpressionRef &head) {
	return StaticExpression<0>(head, EmptySlice());
}

inline DynamicExpressionRef Pool::Expression(const BaseExpressionRef &head, const DynamicSlice &slice) {
	assert(_s_instance);
	return DynamicExpressionRef(_s_instance->_dynamic_expressions.construct(head, slice));
}

inline StringRef Pool::String(std::string &&utf8) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(utf8));
}

inline StringRef Pool::String(const StringExtentRef &extent) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(extent));
}

inline StringRef Pool::String(const StringExtentRef &extent, size_t offset, size_t length) {
	assert(_s_instance);
	return StringRef(_s_instance->_strings.construct(extent, offset, length));
}

inline void Pool::release(BaseExpression *expr) {
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

        case MachineComplexType:
            _s_instance->_machine_complexs.free(static_cast<class MachineComplex*>(expr));
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

inline MatchRef Pool::Match(const PatternMatcherRef &matcher) {
	return _s_instance->_matches.construct(matcher);
}

inline MatchRef Pool::DefaultMatch() {
	return _s_instance->_default_match;
}

inline BaseExpressionRef from_primitive(machine_integer_t value) {
	return Pool::MachineInteger(value);
}

static_assert(sizeof(long) == sizeof(machine_integer_t),
	"types long and machine_integer_t must not differ");

inline BaseExpressionRef from_primitive(const mpz_class &value) {
	if (value.fits_slong_p()) {
		return Pool::MachineInteger(machine_integer_t(value.get_si()));
	} else {
		return Pool::BigInteger(value);
	}
}

inline BaseExpressionRef from_primitive(mpz_class &&value) {
	if (value.fits_slong_p()) {
		return Pool::MachineInteger(machine_integer_t(value.get_si()));
	} else {
		return Pool::BigInteger(value);
	}
}

inline BaseExpressionRef from_primitive(machine_real_t value) {
	return Pool::MachineReal(value);
}

inline BaseExpressionRef from_primitive(const mpq_class &value) {
	return Pool::BigRational(value);
}

inline BaseExpressionRef from_primitive(const Numeric::Z &value) {
	return value.to_expression();
}

inline Match::Match(const PatternMatcherRef &matcher) :
	m_matcher(matcher),
	m_slots(matcher->variables().size(), Pool::slots_allocator()),
	m_slots_fixed(0) {
}
