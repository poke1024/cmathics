#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gmp.h>
#include <iostream>

#include "core/misc.h"
#include "core/expression.h"
#include "core/definitions.h"
#include "core/formatter.h"
#include "core/evaluation.h"
#include "core/pattern.h"
#include "core/integer.h"
#include "core/real.h"
#include "core/rational.h"
#include "core/arithmetic.h"
#include <vector>
#include "core/string.h"

#include <Python.h>
#include <stdlib.h>

// a slim wrapper around PyObject.

namespace python {

    class borrowed_reference {
    public:
        PyObject *const _py_object;

        borrowed_reference(PyObject *py_object) : _py_object(py_object) {
            if (!py_object) {
                throw std::runtime_error("invalid python reference");
            }
        }
    };

    class new_reference {
    public:
        PyObject *const _py_object;

        new_reference(PyObject *py_object) : _py_object(py_object) {
            if (!py_object) {
                throw std::runtime_error("invalid python reference");
            }
        }
    };

    class object;

    class list_iterator {
    private:
        PyObject *_py_object;
        size_t _i;

    public:
        list_iterator() : _py_object(nullptr), _i(0) {
        }

        list_iterator(const list_iterator &i) : _py_object(i._py_object), _i(i._i) {
        }

        list_iterator(PyObject *py_object, size_t i) : _py_object(py_object), _i(i) {
        }

        list_iterator &operator++() {
            _i += 1;
            return *this;
        }

        object operator*() const;

        bool operator!=(const list_iterator &other) const {
            return _py_object != other._py_object || _i != other._i;
        }

        bool operator==(const list_iterator &other) const {
            return _py_object == other._py_object && _i == other._i;
        }
    };

    class object {
    private:
        PyObject *_py_object;

    protected:
        inline bool isinstance(const object &klass) const {
            return PyObject_IsInstance(_py_object, klass._py_object);
        }

        inline object attr(const char *name) const {
            return object(new_reference(PyObject_GetAttrString(_py_object, name)));
        }

        inline bool compare(const object &other, int op_id) const {
            int result = PyObject_RichCompareBool(_py_object, other._py_object, op_id);
            if (result == -1) {
                throw std::runtime_error("comparison failed");
            }
            return result == 1;
        }

    public:
        friend bool isinstance(const object &o, const object &klass);
        friend object getattr(const object &o, const char *name);

        inline object() : _py_object(nullptr) {
        }

        inline object(const borrowed_reference &ref) : _py_object(ref._py_object) {
            Py_INCREF(_py_object);
        }

        inline object(const new_reference &ref) : _py_object(ref._py_object) {
        }

        inline ~object() {
            if (_py_object) {
                Py_DECREF(_py_object);
            }
        }

        inline object &operator=(const object &other) {
            if (_py_object) {
                Py_DECREF(_py_object);
            }
            _py_object = other._py_object;
            Py_INCREF(_py_object);
            return *this;
        }

        inline bool operator==(const object &other) const {
            return compare(other, Py_EQ);
        }

        inline bool operator!=(const object &other) const {
            return compare(other, Py_NE);
        }

        inline list_iterator begin() const {
            return list_iterator(_py_object, 0);
        }

        inline list_iterator end() const {
            return list_iterator(_py_object, PyList_Size(_py_object));
        }

        inline object operator[](size_t i) const {
            PyObject *item;
            if (PyTuple_Check(_py_object)) {
                item = PyTuple_GetItem(_py_object, i);
            } else if (PyList_Check(_py_object)) {
                item = PyList_GetItem(_py_object, i);
            } else {
                throw std::runtime_error("object is neither list nor tuple");
            }
            return object(borrowed_reference(item));
        }

        inline object repr() const {
            return object(new_reference(PyObject_Repr(_py_object)));
        }

        inline std::string as_string() const {
            std::string s;
            if (PyUnicode_Check(_py_object)) {
                PyObject *bytes = PyUnicode_AsEncodedString(_py_object, "utf8", "strict");
                if (bytes) {
                    s = PyBytes_AsString(bytes);
                    Py_DECREF(bytes);
                }
            }
            return s;
        }

        inline void as_integer(mpz_t x) const {
            if (!PyLong_Check(_py_object)) {
                throw std::runtime_error("not an integer value");
            }
            int overflow;
            long value = PyLong_AsLongAndOverflow(_py_object, &overflow);
            if (overflow == 0 && value != -1) {
                mpz_set_si(x, value);
            } else {
                object n(new_reference(PyNumber_ToBase(_py_object, 10)));
                std::string s(n.as_string());
                mpz_set_str(x, s.c_str(), 10);
            }
        }

        inline double as_float() const {
            double x = PyFloat_AsDouble(_py_object);
            if (PyErr_Occurred()) {
                throw std::runtime_error("could not get float");
            }
            return x;
        }

        inline object type() const {
            return object(new_reference(PyObject_Type(_py_object)));
        }

        inline object operator()() const {
            return object(new_reference(PyObject_CallFunction(_py_object, nullptr)));
        }

        inline object operator()(const char *s) const {
            return object(new_reference(PyObject_CallFunction(_py_object, "s", s)));
        }

        inline object operator()(const object &o) const {
            return object(new_reference(PyObject_CallFunction(_py_object, "O", o._py_object)));
        }
    };

    object list_iterator::operator*() const {
        return object(borrowed_reference(PyList_GetItem(_py_object, _i)));
    }

    object string(const char *s) {
        return object(new_reference(PyUnicode_FromString(s)));
    }

    inline object module(const char *name) {
        return object(new_reference(PyImport_ImportModule(name)));
    }

    inline std::ostream &operator<<(std::ostream &s, const python::object &o) {
        s << o.repr().as_string();
        return s;
    }

    inline bool isinstance(const python::object &o, const python::object &klass) {
        return o.isinstance(klass);
    }

    inline python::object getattr(const python::object &o, const char *name) {
        return o.attr(name);
    }
}

using python::isinstance;
using python::getattr;

// ---

class ParseConverter {
private:
    Definitions &_definitions;

    python::object _expression;
    python::object _symbol, _symbol_lookup;
    python::object _integer;
    python::object _machine_real;

public:
    ParseConverter(Definitions &definitions) :
        _definitions(definitions),
        _expression(python::string("Expression")),
        _symbol(python::string("Symbol")),
        _symbol_lookup(python::string("SymbolLookup")),
        _integer(python::string("Integer")),
        _machine_real(python::string("MachineReal")) {
    }

    BaseExpressionRef convert(const python::object &o) {
        auto kind = o[0];

        if (kind == _symbol) {
            auto name = o[1].as_string();
            return _definitions.lookup(name.c_str());
        } else if (kind == _symbol_lookup) {
            auto name = o[1].as_string();
            if (name.find('`') == std::string::npos) {
                // FIXME: add correct context
                name = "System`" + name;
            }
            return _definitions.lookup(name.c_str());
        } else if (kind == _integer) {
            mpz_t x;
            mpz_init(x);
            try {
                o[1].as_integer(x);
                auto y = integer(x);
                mpz_clear(x);
                return y;
            } catch (...) {
                mpz_clear(x);
                throw;
            }
        } else if (kind == _machine_real) {
            return real(o[1].as_float());
        } else if (kind == _expression) {
            auto head = convert(o[1]);
            std::vector<BaseExpressionRef> leaves;

            for (auto leaf : o[2]) {
                leaves.push_back(convert(leaf));
            }

            return expression(head, leaves);
        } else {
            throw std::runtime_error(std::string(
                "unsupported parsed item of type " + kind.as_string()));
        }
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
        auto parser = getattr(parser_module, "Parser")();

        auto feed_module = python::module("mathics.core.parser.feed");
        auto SingleLineFeeder = getattr(feed_module, "SingleLineFeeder");

        _feeder = SingleLineFeeder;
        _parse = getattr(parser, "parse");

        auto convert_module = python::module("mathics.core.parser.convert");
        _do_convert = getattr(getattr(convert_module, "GenericConverter")(), "do_convert");
    }

    BaseExpressionRef parse(const char *s) {
        auto tree = _do_convert(_parse(_feeder(s)));
        return _converter.convert(tree);
    }
};

class Runtime {
private:
    Definitions _definitions;
    Parser _parser;

public:
    Runtime() : _parser(_definitions) {
    }

    inline Definitions &definitions() {
        return _definitions;
    }

    BaseExpressionRef parse(const char *s) {
        return _parser.parse(s);
    }

    template<int N>
    void add_builtin(const char *down, const char *patt, typename BuiltinFunctionArguments<N>::type func) {
        std::string full_down = std::string("System`") + down;
        SymbolRef symbol = _definitions.lookup(full_down.c_str());
        auto rule = make_builtin_rule<N>(_parser.parse(patt), func);
        symbol->add_down_rule(rule);
    }
};

// Timing

// Range

// Append

// Fold

void python_test(const char *input) {
    Runtime runtime;

    auto expr = runtime.parse(input);

    //SymbolRef plus_symbol = definitions.lookup("System`Plus");
    //auto rule = std::make_unique<Rule>(patt, func);

    runtime.add_builtin<1>(
        "Most",
        "Most[x_List]",
        [](const BaseExpressionRef &x, const Evaluation &evaluation) {
            auto list = std::static_pointer_cast<const Expression>(x);
            auto leaves = list->leaves();
            auto n = leaves.size();
            return expression(list->head(), leaves.slice(0, n > 0 ? n - 1 : 0));
    });

    Evaluation evaluation(runtime.definitions(), true);
    std::cout << expr << std::endl;
    std::cout << "evaluating...\n";
    BaseExpressionRef evaluated = evaluation.evaluate(expr);
    std::cout << "done\n";
    std::cout << evaluated << std::endl;
}

void pattern_test() {
    Definitions definitions;

    auto x = definitions.new_symbol("System`x");

    auto patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`Blank"), {})
    });

    Match m = match(patt, integer(7), definitions);
    std::cout << m << std::endl;

    patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`BlankSequence"), {})
    });

    auto some_expr = expression(definitions.lookup("System`Sequence"), {
        integer(1), integer(3)});

    m = match(patt, some_expr, definitions);
    std::cout << m << std::endl;
}

int main() {
    // set PYTHONHOME to a python with a Mathics installation.
    setenv("PYTHONHOME", "/Users/bernhard/Projekte/j5", true);

    pattern_test();

    Py_Initialize();
    //python_test("x + 1.1 + 5.2");
    python_test("Most[{1, 2, 3, 4, 5}]");
    Py_Finalize();
    return 0;

    /*

    Symbol* plus_symbol = definitions->new_symbol("Plus");
    plus_symbol->down_code = _Plus;

    BaseExpression* head = (BaseExpression*) plus_symbol;
    //BaseExpression* la = (BaseExpression*) Definitions_lookup(definitions, "a");
    //BaseExpression* lb = (BaseExpression*) Definitions_lookup(definitions, "b");

    auto la = new MachineInteger(5);
    auto lb = new MachineReal(10.2);

    std::vector<BaseExpression*> leaves;
    leaves.push_back(la);
    leaves.push_back(lb);
    Expression* expr = new Expression(head, leaves);

    Evaluation* evaluation = new Evaluation(definitions, true);

    std::cout << expr->fullform() << std::endl;

    std::cout << "evaluating...\n";
    BaseExpression* result = evaluation->evaluate(expr);
    std::cout << "done\n";

    std::cout << result->fullform();
    */
    // printf("height = %li\n", Expression_height((BaseExpression*) expr));
    // printf("hash = %lu\n", expr->hash);

    return 0;
}
