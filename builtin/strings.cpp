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

		if (!text->is_string()) {
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
		static const OptionsInitializerList options = {
			{"Overlap", offsetof(StringCasesOptions, Overlap), "False"}
		};

		builtin(options, &StringCases::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr text,
		BaseExpressionPtr patt,
		const StringCasesOptions &options,
		const Evaluation &evaluation) {

		if (!text->is_string()) {
			evaluation.message(m_symbol, "string");
			return BaseExpressionRef();
		}

		const String * const string = text->as_string();

		const auto generate = [string, &options, &evaluation] (BaseExpressionPtr patt, const auto &callback) {
			return expression(evaluation.List, sequential([string, &patt, &options, &evaluation, &callback] (auto &store) {
				const StringMatcher matcher(patt, evaluation);
				matcher.search(string, [string, &callback, &store] (index_t begin, index_t end, const auto &match) {
					callback(store, begin, end, match);
				}, options.Overlap->is_true());
			}));
		};

		const OptionalRuleForm rule_form(patt);
		if (rule_form.is_rule()) {
			return generate(rule_form.left_side().get(),
			    [&rule_form] (auto &store, index_t begin, index_t end, const auto &match) {
				    store(rule_form.right_side()->replace_all_or_copy(match));
				});
		} else {
			return generate(patt,
			    [string] (auto &store, index_t begin, index_t end, const auto &match) {
					store(string->substr(begin, end));
				});
		}
	}
};

class StringJoin : public Builtin {
public:
    static constexpr const char *name = "StringJoin";

private:
    class StringArray {
    private:
        const BaseExpressionRef * const m_leaves;
        const size_t m_n;

    public:
        inline StringArray(const BaseExpressionRef *leaves, size_t n) :
            m_leaves(leaves), m_n(n) {
        }

        inline size_t size() const {
            return m_n;
        }

        inline const BaseExpressionRef &operator[](size_t i) const {
            return m_leaves[i];
        }

        inline const BaseExpressionRef *begin() const {
            return m_leaves;
        }

        inline const BaseExpressionRef *end() const {
            return m_leaves + m_n;
        }
    };

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringJoin::apply);
    }

    inline BaseExpressionRef apply(
        const BaseExpressionRef *leaves,
        size_t n,
        const Evaluation &evaluation) {

	    StringRef s = string_array_join(StringArray(leaves, n));
	    if (!s) {
		    evaluation.message(m_symbol, "string");
	    }

	    return s;
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

        if (!n->is_machine_integer() || !str->is_string()) {
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

        if (!str->is_string()) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        return MachineInteger::construct(
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

        if (!str->is_string()) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        if (!n->is_machine_integer()) {
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

        if (!str->is_string()) {
            evaluation.message(m_symbol, "string");
            return BaseExpressionRef();
        }

        if (!n->is_machine_integer()) {
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

        if (!str->is_string()) {
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

class ToString : public Builtin {
public:
	static constexpr const char *name = "ToString";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'ToString[$expr$]'
        <dd>returns a string representation of $expr$.
    </dl>

    >> ToString[2]
     = 2
    >> ToString[2] // InputForm
     = "2"
    >> ToString[a+b]
     = a + b
    #> "U" <> 2
     : String expected.
     = U <> 2
    >> "U" <> ToString[2]
     = U2
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&ToString::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		StyleBoxOptions options;
		return String::construct(expr->format(
			evaluation.OutputForm, evaluation)->boxes_to_text(options, evaluation));
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
	add<ToString>();
}
