#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gmp.h>
#include <iostream>
#include <sstream>
#include <vector>

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
#include "core/string.h"
#include "core/builtin.h"
#include "core/evaluate.h"

#include <Python.h>
#include <stdlib.h>
#include <stdexcept>

// a slim wrapper around PyObject.

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)  {
    // taken from http://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // we don't want the '\0' inside
}
namespace python {
    class Context {
    public:
        Context() {
            // note: if Py_Initialize() fails (e.g. with "unable to load the file system codec"), it usually means that
            // your PYTHONHOME environment variable and the Python library we linked this binary against don't match.

            // likewise, if on OS X, you get a "dyld: Library not loaded: libpython3.5m.dylib" or similar on startup,
            // you have to set your PYTHONHOME and then, additionally, set your DYLD_LIBRARY_PATH to PYTHONHOME/lib.
            Py_Initialize();
        }

        ~Context() {
            Py_Finalize();
        }
    };

    class import_error : public std::runtime_error {
    public:
        import_error(const char *module) : std::runtime_error(
            string_format("The Python module '%s' could not be imported. Please check that your PYTHONHOME points "
                "to a Python installation with that package installed.", module)) {
        }
    };

    class attribute_error : public std::runtime_error {
    public:
        attribute_error(std::string o, const char *attribute) : std::runtime_error(
            string_format("Object %s has no attribute %s", o.c_str(), attribute)) {
        }
    };

    class borrowed_reference {
    public:
        PyObject *const _py_object;

        inline borrowed_reference(PyObject *py_object) : _py_object(py_object) {
        }
    };

    class new_reference {
    public:
        PyObject *const _py_object;

        inline new_reference(PyObject *py_object) : _py_object(py_object) {
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
            if (!PyObject_HasAttrString(_py_object, name)) {
                return object(borrowed_reference(Py_None));
            } else {
                return object(new_reference(PyObject_GetAttrString(_py_object, name)));
            }
        }

        inline bool compare(const object &other, int op_id) const {
            int result = PyObject_RichCompareBool(_py_object, other._py_object, op_id);
            if (result == -1) {
                throw std::runtime_error("comparison failed");
            }
            return result == 1;
        }

	    void raise_python_exception() const;

    public:
        friend bool isinstance(const object &o, const object &klass);
        friend object getattr(const object &o, const char *name);

        inline object() : _py_object(nullptr) {
        }

        inline object(const object &o) : _py_object(o._py_object) {
            if (_py_object) {
                Py_INCREF(_py_object);
            }
        }

        inline object(const borrowed_reference &ref) : _py_object(ref._py_object) {
            if (_py_object) {
                Py_INCREF(_py_object);
            }
        }

        inline object(const new_reference &ref) : _py_object(ref._py_object) {
        }

        inline ~object() {
            if (_py_object) {
                Py_DECREF(_py_object);
            }
        }

        inline bool valid() const {
            return _py_object;
        }

        inline object operator[](const char *name) const {
            if (!PyObject_HasAttrString(_py_object, name)) {
                throw attribute_error(repr().as_string(), name);
            } else {
                return object(new_reference(PyObject_GetAttrString(_py_object, name)));
            }
        }

        inline object &operator=(const object &other) {
            if (_py_object) {
                Py_DECREF(_py_object);
            }
            _py_object = other._py_object;
            if (_py_object) {
                Py_INCREF(_py_object);
            }
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

        inline object operator[](int i) const {
            return (*this)[static_cast<size_t>(i)];
        }

        inline object repr() const {
            return object(new_reference(PyObject_Repr(_py_object)));
        }

        inline std::string as_string() const {
            if (!_py_object) {
                return "NULL";
            } else if (PyUnicode_Check(_py_object)) {
                std::string s;
                PyObject *bytes = PyUnicode_AsEncodedString(_py_object, "utf8", "strict");
                if (bytes) {
                    try {
                        s = PyBytes_AsString(bytes);
                    } catch(...) {
                        Py_DECREF(bytes);
                        throw;
                    }
                    Py_DECREF(bytes);
                }
                return s;
            } else {
                return "<not a string>";
            }
        }

        inline void as_integer(mpz_class &x) const {
            if (!PyLong_Check(_py_object)) {
                throw std::runtime_error("not an integer value");
            }
            int overflow;
            long value = PyLong_AsLongAndOverflow(_py_object, &overflow);
            if (overflow == 0 && value != -1) {
                x = value;
            } else {
                object n(new_reference(PyNumber_ToBase(_py_object, 10)));
                std::string s(n.as_string());
                x.set_str(s, 10);
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
	        auto ref = PyObject_CallFunction(_py_object, nullptr);
	        if (ref == nullptr) {
		        raise_python_exception();
	        }
            return object(new_reference(ref));
        }

        inline object operator()(const char *s) const {
	        auto ref = PyObject_CallFunction(_py_object, const_cast<char*>("s"), s);
	        if (ref == nullptr) {
		        raise_python_exception();
	        }
            return object(new_reference(ref));
        }

        inline object operator()(const object &o) const {
	        auto ref = PyObject_CallFunction(_py_object, const_cast<char*>("O"), o._py_object);
	        if (ref == nullptr) {
		        raise_python_exception();
	        }
            return object(new_reference(ref));
        }
    };

	class python_exception : public std::runtime_error {
	private:
		object _type;
		object _value;
		object _traceback;
	public:
		python_exception(object type, object value, object traceback) :
				std::runtime_error("some kind of python exception occured"),
				_type(type),
				_value(value),
				_traceback(traceback) {
		}
	};

	void object::raise_python_exception() const {
		PyObject *ptype;
		PyObject *pvalue;
		PyObject *ptraceback;
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		throw python_exception(
			object(new_reference(ptype)),
			object(new_reference(pvalue)),
			object(new_reference(ptraceback)));
	}

	object None(borrowed_reference(Py_None));

    object list_iterator::operator*() const {
        return object(borrowed_reference(PyList_GetItem(_py_object, _i)));
    }

    object string(const char *s) {
        return object(new_reference(PyUnicode_FromString(s)));
    }

    inline object module(const char *name) {
        auto module = PyImport_ImportModule(name);
        if (!module) {
            throw import_error(name);
        }
        return object(new_reference(module));
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
    python::object _symbol, _lookup;
    python::object _integer;
    python::object _machine_real;
	python::object _string;

public:
    ParseConverter(Definitions &definitions) :
        _definitions(definitions),
        _expression(python::string("Expression")),
        _symbol(python::string("Symbol")),
        _lookup(python::string("Lookup")),
        _integer(python::string("Integer")),
        _machine_real(python::string("MachineReal")),
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
        } else if (kind == _expression) {
	        auto head = convert(o[1]);
	        std::vector<BaseExpressionRef> leaves;

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

struct RuleData {
	BaseExpressionRef pattern;
	RuleRef rule;
};

BaseExpressionRef assign(
	const BaseExpressionRef &lhs,
	const BaseExpressionRef &rhs,
	const Evaluation &evaluation) {

	// FIXME this is just a super simple example implementation.

	const Symbol *name = lhs->lookup_name();
	if (name) {
		const_cast<Symbol*>(name)->add_rule(lhs, rhs);
	}

	return BaseExpressionRef(evaluation.Null);
}

inline const Expression *if_list(const BaseExpressionRef &item) {
    if (item->type() == ExpressionType) {
        const Expression *expr = static_cast<const Expression*>(item.get());
        if (expr->head()->extended_type() == SymbolList) {
            return expr;
        }
    }
    return nullptr;
}

template<bool SYMBOL>
ExpressionRef iterate(
    const BaseExpressionRef &expr,
    Symbol *iterator,
    const BaseExpressionRef &imin,
    const BaseExpressionRef &imax,
    const BaseExpressionRef &di,
    const Evaluation &evaluation) {

    const BaseExpressionRef &LessEqual = evaluation.LessEqual;
    const BaseExpressionRef &Plus = evaluation.Plus;

    std::vector<BaseExpressionRef> result;

    BaseExpressionRef index = imin;
    while (true) {
        const ExtendedType if_continue =
            expression(LessEqual, index, imax)->
                    evaluate_or_copy(evaluation)->extended_type();

        if (if_continue == SymbolFalse) {
            break;
        } else if (if_continue != SymbolTrue) {
            // FIXME error
            break;
        }

        if (SYMBOL) {
	        result.push_back(scope(iterator, index, [&expr, &evaluation] () {
		        return expr->evaluate_or_copy(evaluation);
	        }));
        } else {
            result.push_back(expr->evaluate_or_copy(evaluation));
        }

        index = expression(Plus, index, di)->evaluate_or_copy(evaluation);
    }

    return expression(evaluation.List, std::move(result));
}

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

    inline Parser &parser() {
        return _parser;
    }

    BaseExpressionRef parse(const char *s) {
        return _parser.parse(s);
    }

    void add(const char *name, Attributes attributes, const std::initializer_list<NewRuleRef> &rules) {
        const std::string full_down = std::string("System`") + name;
        const SymbolRef symbol = _definitions.lookup(full_down.c_str());
		symbol->set_attributes(attributes);
        for (const NewRuleRef &new_rule : rules) {
	        symbol->add_rule(new_rule(symbol, _definitions));

        }
    }

    template<int N, typename F>
    NewRuleRef builtin(F func) {
	    return make_builtin_rule<N, F>(func);
    }

	template<int N>
	NewRuleRef pattern_matched_builtin(const char *pattern, typename BuiltinFunctionArguments<N>::type func) {
		BaseExpressionRef parsed_pattern = _parser.parse(pattern);
		return make_pattern_matched_builtin_rule<N>(parsed_pattern, func);
	}

	NewRuleRef rewrite(const char *pattern, const char *into) {
		BaseExpressionRef parsed_pattern =_parser.parse(pattern);
		return make_rewrite_rule(parsed_pattern, _parser.parse(into));
	}

    void initialize() {
        add("Plus",
            Attributes::None, {
		        Plus0,
		        Plus1,
		        Plus2,
		        Plus3
        });

	    add("Subtract",
	        Attributes::None, {
		        rewrite("Subtract[x_, y_]", "Plus[x, Times[-1, y]]")
	        }
	    );

	    add("Times",
	        Attributes::None, {
		        Times2
	        });

	    add("Power",
	        Attributes::None, {
			    Power
	        });

        add("Sqrt",
            Attributes::None, {
                rewrite("Sqrt[x_]", "x ^ (1 / 2)")
            }
        );

	    add("Expand",
	        Attributes::None, {
			    builtin<1>([] (
					const BaseExpressionRef &expr,
					const Evaluation &evaluation) {

                    return expr->expand(evaluation);
			    })
	        });

	    add("Less",
	        Attributes::None, {
			    Less
	        });

        add("LessEqual",
            Attributes::None, {
                LessEqual
            });

	    add("Greater",
	        Attributes::None, {
			    Greater
	        });

        add("GreaterEqual",
            Attributes::None, {
                GreaterEqual
            });

	    add("If",
	        Attributes::HoldRest, {
		        builtin<2>([](
			 		const BaseExpressionRef &cond,
		            const BaseExpressionRef &t,
		            const Evaluation &evaluation) {
					    const ExtendedType type = cond->extended_type();
					    if (type == SymbolTrue) {
						    const BaseExpressionRef r = t->evaluate(evaluation);
						    return r ? r : t;
					    } else if (type == SymbolFalse) {
						    const Definitions &definitions = evaluation.definitions;
						    return BaseExpressionRef(definitions.symbols().Null);
					    } else {
						    return BaseExpressionRef();
					    }
			        }
		        ),
		        builtin<3>([](
				    const BaseExpressionRef &cond,
				    const BaseExpressionRef &t,
			        const BaseExpressionRef &f,
			        const Evaluation &evaluation) {
			        const ExtendedType type = cond->extended_type();
				        if (type == SymbolTrue) {
					        const BaseExpressionRef r = t->evaluate(evaluation);
					        return r ? r : t;
				        } else if (type == SymbolFalse) {
					        const BaseExpressionRef r = f->evaluate(evaluation);
					        return r ? r : f;
				        } else {
						    return BaseExpressionRef();
					    }
			        }
		        ),
		        builtin<4>([](
				    const BaseExpressionRef &cond,
			        const BaseExpressionRef &t,
			        const BaseExpressionRef &f,
			        const BaseExpressionRef &u,
			        const Evaluation &evaluation) {
				        const ExtendedType type = cond->extended_type();
				        if (type == SymbolTrue) {
					        const BaseExpressionRef r = t->evaluate(evaluation);
					        return r ? r : t;
				        } else if (type == SymbolFalse) {
					        const BaseExpressionRef r = f->evaluate(evaluation);
					        return r ? r : f;
				        } else {
					        const BaseExpressionRef r = u->evaluate(evaluation);
					        return r ? r : u;
				        }
			        }
		        )
	        });

	    add("SetDelayed",
	        Attributes::HoldAll + Attributes::SequenceHold, {
			    builtin<2>(assign)
	        });

	    add("Set",
	        Attributes::HoldFirst + Attributes::SequenceHold, {
		        builtin<2>(assign)
	        });

	    add("Apply",
            Attributes::None, {
		        builtin<2>(
	                [](const BaseExpressionRef &f, const BaseExpressionRef &x, const Evaluation &evaluation) {
	                    if (x->type() != ExpressionType) {
	                        throw std::runtime_error("expected Expression at position 2");
	                    }
	                    return x->clone(f);
	                }
	            )
            });

	    add("First",
	        Attributes::None, {
		        builtin<1>(
			        [](const BaseExpressionRef &x, const Evaluation &evaluation) {
				        if (x->type() != ExpressionType) {
					        throw std::runtime_error("expected Expression at position 1");
				        }
				        const Expression *list = static_cast<const Expression*>(x.get());
				        return list->with_leaves_array([] (const BaseExpressionRef *leaves, size_t size) {
					        if (size < 1) {
						        throw std::runtime_error("Expression is empty");
					        }
					        return leaves[0];
				        });
			        }
		        )
	        });

	    add("Length",
	        Attributes::None, {
			    builtin<1>(
			        [](const BaseExpressionRef &x, const Evaluation &evaluation) {
				        if (x->type() != ExpressionType) {
					        return from_primitive(machine_integer_t(0));
				        } else {
					        const Expression *list = static_cast<const Expression*>(x.get());
					        return from_primitive(machine_integer_t(list->size()));
				        }
			        }
		        )
	        });

	    add("Map",
	        Attributes::None, {
			    builtin<2>(
					[](const BaseExpressionRef &f, const BaseExpressionRef &expr, const Evaluation &evaluation) {
						if (expr->type() != ExpressionType) {
							return BaseExpressionRef();
						}
						const Expression *list = static_cast<const Expression*>(expr.get());
						return list->map(f, evaluation);
					}
			    )
	        });

        add("Most",
            Attributes::None, {
	            builtin<1>(
	                [](const BaseExpressionRef &x, const Evaluation &evaluation) -> BaseExpressionRef {
		                if (x->type() != ExpressionType) {
			                return BaseExpressionRef();
		                }
	                    const Expression *list = static_cast<const Expression*>(x.get());
	                    return list->slice(0, -1);
	                }
	            )
            });

        add("Range",
            Attributes::None, {
		        rewrite("Range[imax_]", "Range[1, imax, 1]"),
		        rewrite("Range[imin_, imax_]", "Range[imin, imax, 1]"),
		        builtin<3>(
	                Range
	            )
            });


	    add("Mean",
	        Attributes::None, {
		        rewrite("Mean[x_List]", "Total[x] / Length[x]"),
	        });

	    add("Total",
	        Attributes::None, {
		        rewrite("Total[head_]", "Apply[Plus, head]"),
		        rewrite("Total[head_, n_]", "Apply[Plus, Flatten[head, n]]"),
	        });

	    add("Timing",
	        Attributes::HoldAll, {
			    builtin<1>(
				    [](const BaseExpressionRef &expr, const Evaluation &evaluation) {
					    const auto &List = evaluation.List;
					    const auto start_time = std::chrono::steady_clock::now();
					    const auto evaluated = expr->evaluate(evaluation);
					    const auto end_time = std::chrono::steady_clock::now();
					    const auto microseconds = std::chrono::duration_cast<
						    std::chrono::microseconds>(end_time - start_time).count();
					    return expression(List, {
							from_primitive(double(microseconds) / 1000000.0), evaluated});
				    }
		        )
	        });

	    add("Function",
	        Attributes::HoldAll, {
			    NewRule<FunctionRule>
	        }
	    );

        add("Table",
            Attributes::HoldAll, {
               builtin<2>([] (
                   const BaseExpressionRef &expr,
                   const BaseExpressionRef &iter,
                   const Evaluation &evaluation) -> ExpressionRef {

                   const Expression *list = if_list(iter);
                   if (!list) {
                       return ExpressionRef();
                   }

                   switch (list->size()) {
                       case 1: { // {imax_}
                           const BaseExpressionRef *leaves = list->static_leaves<1>();
                           return iterate<false>(
                               expr,
                               nullptr,
                               Heap::MachineInteger(0),
                               leaves[0]->evaluate_or_copy(evaluation),
                               Heap::MachineInteger(1),
                               evaluation);
                       }

                       case 2: { // {i_Symbol, imax_} or {i_Symbol, {items___}}
                           const BaseExpressionRef *leaves = list->static_leaves<2>();
                           const BaseExpressionRef &iterator = leaves[0];

                           if (iterator->type() == SymbolType) {
                               const BaseExpressionRef &domain = leaves[1];
                               const Expression *domain_list = if_list(domain);

                               if (domain_list) {
                                   domain_list = if_list(domain_list->evaluate_or_copy(evaluation));
                                   if (domain_list) {
                                       return domain_list->iterate(iterator->as_symbol(), expr, evaluation);
                                   }
                               } else {
                                   return iterate<true>(
                                       expr,
                                       iterator->as_symbol(),
                                       Heap::MachineInteger(1),
                                       leaves[1]->evaluate_or_copy(evaluation),
                                       Heap::MachineInteger(1),
                                       evaluation);
                               }
                           }
                           break;
                       }
                       case 3: { // {i_Symbol, imin_, imax_}
                           const BaseExpressionRef *leaves = list->static_leaves<3>();
                           const BaseExpressionRef &iterator = leaves[0];

                           if (iterator->type() == SymbolType) {
                               return iterate<true>(
                                   expr,
                                   iterator->as_symbol(),
                                   leaves[1]->evaluate_or_copy(evaluation),
                                   leaves[2]->evaluate_or_copy(evaluation),
                                   Heap::MachineInteger(1),
                                   evaluation);
                           }
                           break;
                       }
                       case 4: { // {i_Symbol, imin_, imax_, di_}
                           const BaseExpressionRef *leaves = list->static_leaves<4>();
                           const BaseExpressionRef &iterator = leaves[0];

                           if (iterator->type() == SymbolType) {
                               return iterate<true>(
                                   expr,
                                   iterator->as_symbol(),
                                   leaves[1]->evaluate_or_copy(evaluation),
                                   leaves[2]->evaluate_or_copy(evaluation),
                                   leaves[3]->evaluate_or_copy(evaluation),
                                   evaluation);
                           }
                           break;
                       }
                   }

                   return ExpressionRef();
               })
            }
        );
    }
};

void python_test(const char *input) {
    Runtime runtime;

    runtime.initialize();

    auto expr = runtime.parse(input);

    Evaluation evaluation(runtime.definitions(), true);
    std::cout << expr << std::endl;
    std::cout << "evaluating...\n";
    BaseExpressionRef evaluated = evaluation.evaluate(expr);
    std::cout << "done\n";
    std::cout << evaluated << std::endl;
}

/*void pattern_test() {
    Definitions definitions;

    auto x = definitions.new_symbol("System`x");

    auto patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`Blank"), {})
    });

    Match m1 = match(patt, from_primitive(7LL), definitions);
    std::cout << m1 << std::endl;

    patt = expression(definitions.lookup("System`Pattern"), {
        x, expression(definitions.lookup("System`BlankSequence"), {})
    });

    auto some_expr = expression(definitions.lookup("System`Sequence"), {
		from_primitive(1LL), from_primitive(3LL)});

    Match m2 = match(patt, some_expr, definitions);
    std::cout << m2 << std::endl;
}*/

void mini_console() {
    Runtime runtime;
    runtime.initialize();

    std::cout << ">> ";
    for (std::string line; std::getline(std::cin, line);) {
	    if (line.empty()) {
		    break;
	    }

	    try {
		    auto expr = runtime.parse(line.c_str());

		    Evaluation evaluation(runtime.definitions(), true);
		    BaseExpressionRef evaluated = evaluation.evaluate(expr);
		    std::cout << evaluated << std::endl;
	    } catch (parse_exception) {
		    std::cout << ": " << line << " could not be parsed." << std::endl;
	    }

	    std::cout << ">> ";
	    std::cout.flush();
    }
}

int main() {
    Heap::init();
	EvaluateDispatch::init();

    python::Context context;

    mini_console();

    return 0;
}
