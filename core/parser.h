#ifndef CMATHICS_PARSER_H
#define CMATHICS_PARSER_H

#include "types.h"
#include "expression.h"
#include "definitions.h"
#include "python.h"

class ParseConverter {
private:
	Definitions &_definitions;

	python::object _expression;
	python::object _symbol, _lookup;
	python::object _integer;
	python::object _machine_real;
    python::object _precision_real;
	python::object _string;

    python::object _decimal_string;

public:
	ParseConverter(Definitions &definitions) :
        _definitions(definitions),
        _expression(python::string("Expression")),
        _symbol(python::string("Symbol")),
        _lookup(python::string("Lookup")),
        _integer(python::string("Integer")),
        _machine_real(python::string("MachineReal")),
        _precision_real(python::string("PrecisionReal")),
        _decimal_string(python::string("DecimalString")),
        _string(python::string("String")) {
	}

	BaseExpressionRef convert(const python::object &o) {
		auto kind = o[0];

		if (kind == _symbol) {
			auto name = o[1].as_string();
			return _definitions.lookup(name.c_str());
		} else if (kind == _lookup) {
			auto name = o[1].as_string();
			if (name.find('`') == std::string::npos) {
				// FIXME: add correct context
				name = "System`" + name;
			}
			return _definitions.lookup(name.c_str());
		} else if (kind == _integer) {
			mpz_class x;
			o[1].as_integer(x);
			return from_primitive(x);
		} else if (kind == _machine_real) {
			return from_primitive(o[1].as_float());
        } else if (kind == _precision_real) {
            if (o[1][0] == _decimal_string) {
                const std::string decimals = o[1][1].as_string();
                const double prec_10 = o[2].as_float();
                const Precision prec(prec_10);
	            mpfr_t x;
	            mpfr_init2(x, prec.bits);
	            mpfr_set_str(x, decimals.c_str(), prec.bits, MPFR_RNDN);
                return Pool::BigReal(x, prec);
            } else {
                throw std::runtime_error("unsupported PrecisionReal");
            }
		} else if (kind == _expression) {
			auto head = convert(o[1]);
			LeafVector leaves;

			for (auto leaf : o[2]) {
				leaves.push_back(convert(leaf));
			}

			return expression(head, std::move(leaves));
		} else if (kind == _string) {
			return from_primitive(o[1].as_string());
		} else {
			throw std::runtime_error(std::string(
				"unsupported parsed item of type " + kind.as_string()));
		}
	}
};

class parse_exception : public std::runtime_error {
public:
	parse_exception(const char *s) : std::runtime_error(std::string("failed to parse ") + s) {
	}
};

class Parser {
private:
	python::object _feeder;
	python::object _parse;
	python::object _do_convert;
	ParseConverter _converter;

public:
	Parser(Definitions &definitions) : _converter(definitions) {
		auto parser_module = python::module("mathics.core.parser.parser");
		auto parser = parser_module["Parser"]();

		auto feed_module = python::module("mathics.core.parser.feed");
		auto SingleLineFeeder = feed_module["SingleLineFeeder"];

		_feeder = SingleLineFeeder;
		_parse = parser["parse"];

		auto convert_module = python::module("mathics.core.parser.convert");

		try {
			auto generic_converter = convert_module["GenericConverter"];
			_do_convert = generic_converter()["do_convert"];
		} catch(const python::attribute_error& e) {
			throw std::runtime_error("Your version of Mathics is too old (it does not know "
					                         "mathics.core.parser.convert.GenericConverter). Please make sure that you "
					                         "have a recent version of Mathics installed in your PYTHONHOME.");
		}
	}

	BaseExpressionRef parse(const char *s) {
		try {
			auto tree = _do_convert(_parse(_feeder(s)));
			return _converter.convert(tree);
		} catch(python::python_exception &e) {
			throw parse_exception(s);
		}
	}
};

#endif //CMATHICS_PARSER_H
