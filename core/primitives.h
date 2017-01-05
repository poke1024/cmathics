#ifndef CMATHICS_TO_VALUE_H
#define CMATHICS_TO_VALUE_H

#include "integer.h"
#include "real.h"
#include "complex.h"
#include "rational.h"
#include "numeric.h"

class to_primitive_error : public std::runtime_error {
public:
	to_primitive_error(Type type, const char *name) : std::runtime_error(
		std::string("cannot convert ") + type_name(type) + std::string(" to ") + name) {
	}
};

template<typename T>
class TypeFromPrimitive {
};

#endif //CMATHICS_TO_VALUE_H
