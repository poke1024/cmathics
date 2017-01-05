#include "strings.h"
#include "unicode/uchar.h"

class StringMatchQ : public Builtin {
public:
	static constexpr const char *name = "StringMatchQ";

	static constexpr const char *docs = R"(
    >> StringMatchQ["abc", "abc"]
     = True

    >> StringMatchQ["abc", "abd"]
     = False
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&StringMatchQ::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr text,
		BaseExpressionPtr patt,
		const Evaluation &evaluation) {

		if (text->type() != StringType) {
			evaluation.message(m_symbol, "string");
			return BaseExpressionRef();
		}

        StringMatcher matcher(patt, evaluation);
		return evaluation.Boolean(bool(matcher(text->as_string())));
	}
};

struct StringCasesOptions {
	BaseExpressionPtr Overlap;
};

class StringCases : public Builtin {
public:
	static constexpr const char *name = "StringCases";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		const OptionsInitializerList options = {
			{"Overlap", offsetof(StringCasesOptions, Overlap), "False"}
		};

		builtin(options, &StringCases::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr text,
		BaseExpressionPtr patt,
		const StringCasesOptions &options,
		const Evaluation &evaluation) {

		if (text->type() != StringType) {
			evaluation.message(m_symbol, "string");
			return BaseExpressionRef();
		}

		const String * const string = text->as_string();

		const auto generate = [string, &options, &evaluation] (BaseExpressionPtr patt, const auto &callback) {
			return expression_from_generator(evaluation.List, [string, &patt, &options, &evaluation, &callback] (auto &storage) {
				const StringMatcher matcher(patt, evaluation);
				matcher.search(string, [string, &callback, &storage] (index_t begin, index_t end, const auto &match) {
					callback(storage, begin, end, match);
				}, options.Overlap->is_true());
			});
		};

		const RuleForm rule_form(patt);
		if (rule_form.is_rule()) {
			return generate(rule_form.left_side().get(),
			    [&rule_form] (auto &storage, index_t begin, index_t end, const auto &match) {
				    storage << rule_form.right_side()->replace_all_or_copy(match);
				});
		} else {
			return generate(patt,
			    [string] (auto &storage, index_t begin, index_t end, const auto &match) {
					storage << string->substr(begin, end);
				});
		}
	}
};

class StringJoin : public Builtin {
public:
    static constexpr const char *name = "StringJoin";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringJoin::apply);
    }

    inline BaseExpressionRef apply(
        const BaseExpressionRef *leaves,
        size_t n,
        const Evaluation &evaluation) {

	    StringExtent::Type extent_type = StringExtent::ascii;
	    size_t number_of_code_points = 0;

	    for (size_t i = 0; i < n; i++) {
		    const BaseExpressionRef &leaf = leaves[i];
		    if (leaf->type() != StringType) {
			    return BaseExpressionRef(); // FIXME
		    }
		    extent_type = std::max(extent_type,
				leaf->as_string()->extent_type());
		    number_of_code_points +=
			    leaf->as_string()->number_of_code_points();
	    }

	    if (extent_type == StringExtent::ascii) {
			std::string text;
		    text.reserve(number_of_code_points);
		    for (size_t i = 0; i < n; i++) {
			    const String *s = leaves[i]->as_string();
			    text.append(s->ascii(), s->length());
		    }
	        return Pool::String(new AsciiStringExtent(std::move(text)));
	    } else {
			UnicodeString text(number_of_code_points, 0, 0);
		    for (size_t i = 0; i < n; i++) {
			    text.append(leaves[i]->as_string()->unicode());
		    }
	        if (extent_type == StringExtent::simple) {
		        return Pool::String(new SimpleStringExtent(text));
	        } else {
		        return Pool::String(new ComplexStringExtent(text));
	        }
	    }
    }
};

class StringRepeat : public Builtin {
public:
    static constexpr const char *name = "StringRepeat";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringRepeat::apply2);
    }

    inline BaseExpressionRef apply2(
        BaseExpressionPtr str,
        BaseExpressionPtr n,
        const Evaluation &evaluation) {

        if (n->type() != MachineIntegerType || str->type() != StringType) {
            return BaseExpressionRef();
        }

	    return str->as_string()->repeat(static_cast<const MachineInteger*>(n)->value);
    }
};

class StringLength : public Builtin {
public:
    static constexpr const char *name = "StringLength";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringLength::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr str,
        const Evaluation &evaluation) {

        if (str->type() != StringType) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        return Pool::MachineInteger(
            static_cast<const String*>(str)->length());
    }
};

class StringTake : public Builtin {
public:
    static constexpr const char *name = "StringTake";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringTake::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr str,
        BaseExpressionPtr n,
        const Evaluation &evaluation) {

        if (str->type() != StringType) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        if (n->type() != MachineIntegerType) {
            return BaseExpressionRef();
        }

        const size_t n_value = static_cast<const MachineInteger*>(n)->value;

        return str->as_string()->take(n_value);
    }
};

class StringDrop : public Builtin {
public:
    static constexpr const char *name = "StringDrop";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringDrop::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr str,
        BaseExpressionPtr n,
        const Evaluation &evaluation) {

        if (str->type() != StringType) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        if (n->type() != MachineIntegerType) {
            return BaseExpressionRef();
        }

        const size_t n_value = static_cast<const MachineInteger*>(n)->value;

        return str->as_string()->drop(n_value);
    }
};

class StringTrim : public Builtin {
public:
    static constexpr const char *name = "StringTrim";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringTrim::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr str,
        const Evaluation &evaluation) {

        if (str->type() != StringType) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        auto s(str->as_string()->unicode());

        index_t end = s.length();
        index_t begin = 0;
        while (begin < end) {
            if (!u_isUWhiteSpace(s.charAt(begin))) {
                break;
            }
            begin++;
        }

        while (end > begin) {
            if (!u_isUWhiteSpace(s.charAt(end - 1))) {
                break;
            }
            end--;
        }

        return str->as_string()->strip_code_points(
            begin, s.length() - end);
    }
};

void Builtins::Strings::initialize() {
	add<StringMatchQ>();
	add<StringCases>();
    add<StringJoin>();
    add<StringRepeat>();
    add<StringLength>();
    add<StringTake>();
    add<StringDrop>();
    add<StringTrim>();
}
