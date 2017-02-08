#ifndef CMATHICS_PYTHON_H
#define CMATHICS_PYTHON_H

#include <gmpxx.h>
#include <string>

#include <Python.h>
#include <stdlib.h>
#include <stdexcept>

template<typename... Args>
std::string string_format(const char *format, const Args&... args)  {
    // taken from http://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    size_t size = std::snprintf(nullptr, 0, format, args...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format, args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

// a slim wrapper around PyObject.

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

		inline void raise_python_exception() const;

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

        inline std::string str() const {
            return object(new_reference(PyObject_Str(_py_object))).as_string();
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

        inline long as_small_integer() const {
            if (!PyLong_Check(_py_object)) {
                throw std::runtime_error("not an integer value");
            }
            int overflow;
            long value = PyLong_AsLongAndOverflow(_py_object, &overflow);
            if (overflow == 0 && value != -1) {
                return value;
            } else {
                throw std::runtime_error("integer too large");
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

	inline void object::raise_python_exception() const {
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

	inline object list_iterator::operator*() const {
		return object(borrowed_reference(PyList_GetItem(_py_object, _i)));
	}

	inline object string(const char *s) {
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

#endif //CMATHICS_PYTHON_H
