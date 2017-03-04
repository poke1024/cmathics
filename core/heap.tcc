#pragma once

#include "slice/tiny.h"

inline BaseExpressionRef from_primitive(machine_integer_t value) {
	return MachineInteger::construct(value);
}

static_assert(sizeof(long) == sizeof(machine_integer_t),
	"types long and machine_integer_t must not differ");

inline BaseExpressionRef from_primitive(const mpz_class &value) {
	if (value.fits_slong_p()) {
		return MachineInteger::construct(machine_integer_t(value.get_si()));
	} else {
		return BigInteger::construct(value);
	}
}

inline BaseExpressionRef from_primitive(mpz_class &&value) {
	if (value.fits_slong_p()) {
		return MachineInteger::construct(machine_integer_t(value.get_si()));
	} else {
		return BigInteger::construct(value);
	}
}

inline BaseExpressionRef from_primitive(machine_real_t value) {
	return MachineReal::construct(value);
}

inline BaseExpressionRef from_primitive(const mpq_class &value) {
    if (value.get_den() == 1) {
        return from_primitive(value.get_num());
    } else {
        return BigRational::construct(value);
    }
}

inline BaseExpressionRef from_primitive(const Numeric::Z &value) {
	return value.to_expression();
}

inline DynamicOptionsProcessor::DynamicOptionsProcessor() :
	OptionsProcessor(Dynamic) {
}

inline SlotVector::SlotVector(size_t size) : m_size(size) {
    if (size <= n_preallocated) {
        m_slots_ptr = m_slots;
    } else {
        m_vector.emplace(size, LegacyPool::slots_allocator());
        m_slots_ptr = m_vector->data();
    }
}


inline Match::Match() :
    m_slots(),
    m_slots_fixed(0) {
}

inline Match::Match(const PatternMatcherRef &matcher) :
	m_matcher(matcher),
	m_slots(matcher->variables().size()),
	m_slots_fixed(0) {
}

inline Match::Match(const PatternMatcherRef &matcher, const OptionsProcessorRef &options_processor) :
    Match(matcher) {
    m_options = options_processor;
}
