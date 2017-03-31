#include "strings.h"
#include "unicode/uchar.h"
#include "arithmetic/binary.h"

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

struct StringReplaceOptions {
    BaseExpressionPtr IgnoreCase;

    static OptionsInitializerList meta() {
        return {
            {"IgnoreCase", offsetof(StringReplaceOptions, IgnoreCase), "False"}
        };
    };
};

class StringReplace : public Builtin {
public:
    static constexpr const char *name = "StringReplace";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'StringReplace["$string$", "$a$"->"$b$"]'
        <dd>replaces each occurrence of $old$ with $new$ in $string$.
    <dt>'StringReplace["$string$", {"$s1$"->"$sp1$", "$s2$"->"$sp2$"}]'
        <dd>performs multiple replacements of each $si$ by the
        corresponding $spi$ in $string$.
    <dt>'StringReplace["$string$", $srules$, $n$]'
        <dd>only performs the first $n$ replacements.
    <dt>'StringReplace[{"$string1$", "$string2$", ...}, $srules$]'
        <dd>performs the replacements specified by $srules$ on a list
        of strings.
    </dl>

    StringReplace replaces all occurrences of one substring with another:
    >> StringReplace["xyxyxyyyxxxyyxy", "xy" -> "A"]
     = AAAyyxxAyA

    Multiple replacements can be supplied:
    >> StringReplace["xyzwxyzwxxyzxyzw", {"xyz" -> "A", "w" -> "BCD"}]
     = ABCDABCDxAABCD

    Also works for multiple rules:
    >> StringReplace["abba", {"a" -> "A", "b" -> "B"}, 2]
     = ABba

    StringReplace acts on lists of strings too:
    >> StringReplace[{"xyxyxxy", "yxyxyxxxyyxy"}, "xy" -> "A"]
     = {AAxA, yAAxxAyA}

    >> StringReplace["abcabc", "a" -> "b", Infinity]
     = bbcbbc
    >> StringReplace[x, "a" -> "b"]
     : String or list of strings expected at position 1 in StringReplace[x, a -> b].
     = StringReplace[x, a -> b]
    >> StringReplace["xyzwxyzwaxyzxyzw", x]
     : x is not a valid string replacement rule.
     = StringReplace[xyzwxyzwaxyzxyzw, x]
    >> StringReplace["xyzwxyzwaxyzxyzw", x -> y]
     : Element x is not a valid string or pattern element in x.
     = StringReplace[xyzwxyzwaxyzxyzw, x -> y]
    >> StringReplace["abcabc", "a" -> "b", -1]
     : Non-negative integer or Infinity expected at position 3 in StringReplace[abcabc, a -> b, -1].
     = StringReplace[abcabc, a -> b, -1]
    >> StringReplace["abc", "b" -> 4]
     : String expected.
     = a <> 4 <> c

    >> StringReplace["01101100010", "01" .. -> "x"]
     = x1x100x0

    >> StringReplace["abc abcb abdc", "ab" ~~ _ -> "X"]
     = X Xb Xc

    >> StringReplace["abc abcd abcd",  WordBoundary ~~ "abc" ~~ WordBoundary -> "XX"]
     = XX abcd abcd

    >> StringReplace["abcdabcdaabcabcd", {"abc" -> "Y", "d" -> "XXX"}]
     = YXXXYXXXaYYXXX


    >> StringReplace["  Have a nice day.  ", (StartOfString ~~ Whitespace) | (Whitespace ~~ EndOfString) -> ""] // FullForm
     = "Have a nice day."

    >> StringReplace["xyXY", "xy" -> "01"]
     = 01XY
    >> StringReplace["xyXY", "xy" -> "01", IgnoreCase -> True]
     = 0101

    StringReplace also can be used as an operator:
    >> StringReplace["y" -> "ies"]["city"]
     = cities
	)";

private:
    class err_strse : public std::exception {
    };

    class err_srep : public std::exception {
    public:
        const BaseExpressionRef expr;

        err_srep(const BaseExpressionRef &expr_) : expr(expr_) {

        }
    };

	class err_innf : public std::exception {
	};

    BaseExpressionRef string_replace_1(
        BaseExpressionPtr text,
        BaseExpressionPtr patt,
        machine_integer_t n_to_replace,
        const StringReplaceOptions &options,
        const Evaluation &evaluation) {

        if (!text->is_string()) {
            throw err_strse();
        }

        const StringPtr string = text->as_string();

        TemporaryRefVector chunks;
        size_t last_index = 0;

        const auto push = [string, &chunks, &last_index, &n_to_replace, &evaluation] (
            const StringCases::IteratorRef &iterator, const BaseExpressionRef &rhs) -> bool {

            const size_t begin = iterator->begin();
            const size_t end = iterator->end();

            if (begin > last_index) {
                chunks.push_back(string->substr(last_index, begin));
            }

            chunks.push_back(rhs->replace_all_or_copy(iterator->match(), evaluation));
            last_index = end;

            return --n_to_replace > 0;
        };

        if (patt->is_list()) {
            std::vector<std::tuple<index_t, StringCases::IteratorRef, UnsafeBaseExpressionRef>> rules;
            rules.reserve(patt->as_expression()->size());

            patt->as_expression()->with_slice([string, &evaluation, &rules, &options] (const auto &slice) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    const OptionalRuleForm rule_form(slice[i]);
                    if (!rule_form.is_rule()) {
                        throw err_srep(slice[i]);
                    }
                    const StringCases cases(rule_form.left_side().get(), evaluation);
                    const StringCases::IteratorRef iterator = cases(string, options.IgnoreCase->is_true());
                    if (iterator->next()) {
                        rules.push_back(std::make_tuple(i, iterator, rule_form.right_side()));
                    }
                }
            });

            const auto compare = [] (const auto &a, const auto &b) {
                const auto i = std::get<1>(a)->begin();
                const auto j = std::get<1>(b)->begin();
                if (i > j) {
                    return true;
                } else if (i < j) {
                    return false;
                } else {
                    // compare pattern position in list; item with smaller position is prioritized
                    return std::get<0>(a) > std::get<0>(b);
                }
            };

            std::make_heap(rules.begin(), rules.end(), compare);

            while (!rules.empty()) {
                std::pop_heap(rules.begin(), rules.end(), compare);
                const auto &rule = rules.back();
                const auto &iterator = std::get<1>(rule);

                if (!push(iterator, std::get<2>(rule))) {
                    break;
                }

                if (iterator->next()) {
                    std::push_heap(rules.begin(), rules.end(), compare);
                } else {
                    rules.pop_back();
                }
            }
        } else {
            const OptionalRuleForm rule_form(patt);
            if (!rule_form.is_rule()) {
                throw err_srep(patt);
            }

            const StringCases cases(rule_form.left_side().get(), evaluation);
            const auto iterator = cases(string, options.IgnoreCase->is_true());

            while (iterator->next()) {
                if (!push(iterator, rule_form.right_side())) {
                    break;
                }
            }
        }

        if (last_index < string->length()) {
            chunks.push_back(string->substr(last_index));
        }

        StringRef result = string_array_join(chunks);
        if (result) {
            return result;
        } else {
            return chunks.to_expression(evaluation.StringJoin);
        }
    }

    BaseExpressionRef string_replace(
        ExpressionPtr expr,
        BaseExpressionPtr text,
        BaseExpressionPtr patt,
        machine_integer_t n_to_replace,
        const StringReplaceOptions &options,
        const Evaluation &evaluation) {

        try {
            if (text->is_list()) {
                return text->as_expression()->map(
                    evaluation.List, [this, &patt, &n_to_replace, &options, &evaluation](const auto &leaf) {
                        return string_replace_1(leaf.get(), patt, n_to_replace, options, evaluation);
                    });
            } else {
                return string_replace_1(text, patt, n_to_replace, options, evaluation);
            }
        } catch (const err_srep &err) {
            evaluation.message(m_symbol, "srep", err.expr);
            return BaseExpressionRef();
        } catch (const err_strse&) {
            evaluation.message(m_symbol, "strse", evaluation.definitions.one, expr);
            return BaseExpressionRef();
        } catch (const IllegalStringPattern &err) {
            evaluation.message(evaluation.StringExpression, "invld", err.what(), err.where());
            return BaseExpressionRef();
        }
    }

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("strse", "String or list of strings expected at position `1` in `2`.");
        message("srep", "`1` is not a valid string replacement rule.");
	    message("innf", "Non-negative integer or Infinity expected at position `1` in `2`.");

        builtin("StringReplace[rule_][string_]", "StringReplace[string, rule]");

        builtin(
            "StringReplace[text_, patt_, Shortest[n_:Infinity], OptionsPattern[StringReplace]]",
            &StringReplace::apply);
    }

    inline BaseExpressionRef apply(
        ExpressionPtr expr,
        BaseExpressionPtr text,
        BaseExpressionPtr patt,
        BaseExpressionPtr n,
        const StringReplaceOptions &options,
        const Evaluation &evaluation) {

        machine_integer_t remaining;

	    try {
		    const auto n_integer = n->get_machine_int_value();

		    if (n_integer) {
			    remaining = *n_integer;
			    if (remaining == 0) {
				    return text;
			    } else if (remaining < 0) {
					throw err_innf();
			    }
		    } else if (n->is_positive_infinity()) {
			    remaining = std::numeric_limits<machine_integer_t>::max();
		    } else {
			    throw err_innf();
		    }
	    } catch (const err_innf&) {
		    evaluation.message(m_symbol, "innf", MachineInteger::construct(3), expr);
		    return BaseExpressionRef();
	    }

        return string_replace(
            expr, text, patt, remaining, options, evaluation);
    }
};

struct StringCasesOptions {
	BaseExpressionPtr Overlap;
    BaseExpressionPtr IgnoreCase;
};

class StringCasesBuiltin : public Builtin {
public:
	static constexpr const char *name = "StringCases";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		static const OptionsInitializerList options = {
			{"Overlap", offsetof(StringCasesOptions, Overlap), "False"},
            {"IgnoreCase", offsetof(StringCasesOptions, IgnoreCase), "False"}
		};

		builtin(options, &StringCasesBuiltin::apply);
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
                const StringCases cases(patt, evaluation);

                const auto iterator = cases(string, options.IgnoreCase->is_true());
                iterator->set_overlap(options.Overlap->is_true());
                while (iterator->next()) {
                    callback(store, iterator->begin(), iterator->end(), iterator->match());
				}
			}));
		};

		const OptionalRuleForm rule_form(patt);
		if (rule_form.is_rule()) {
			return generate(rule_form.left_side().get(),
			    [&rule_form, &evaluation] (auto &store, index_t begin, index_t end, const auto &match) {
				    store(rule_form.right_side()->replace_all_or_copy(match, evaluation));
				});
		} else {
			return generate(patt,
			    [string] (auto &store, index_t begin, index_t end, const auto &match) {
					store(string->substr(begin, end));
				});
		}
	}
};

class StringJoin : public BinaryOperatorBuiltin {
public:
    static constexpr const char *name = "StringJoin";

	//static constexpr auto attributes =
	//	Attributes::Flat + Attributes::OneIdentity;

	virtual const char *operator_name() const {
		return "<>";
	}

	virtual int precedence() const {
		return 600;
	}

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
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
	    add_binary_operator_formats();

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

class StringExpression : public Builtin {
public:
    static constexpr const char *name = "StringExpression";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("invld", "Element `1` is not a valid string or pattern element in `2`.");
        message("cond", "Ignored restriction given for `1` in `2` as it does not match previous occurences of `1`.");
    }
};

void Builtins::Strings::initialize() {
	add<StringMatchQ>();
    add<StringReplace>();
	add<StringCasesBuiltin>();
    add<StringJoin>();
    add<StringRepeat>();
    add<StringLength>();
    add<StringTake>();
    add<StringDrop>();
    add<StringTrim>();
	add<ToString>();
    add<StringExpression>();
}
