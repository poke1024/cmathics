#include "strings.h"

class StringCases : public Builtin {
public:
	static constexpr const char *name = "StringCases";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&StringCases::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr text,
		BaseExpressionPtr patt,
		const Evaluation &evaluation) {

		if (text->type() != StringType) {
			return BaseExpressionRef();
		}

        const String * const str = static_cast<const String*>(text);

		const PatternMatcherRef matcher = compile_string_pattern(BaseExpressionRef(patt));

		switch (patt->type()) {
			case StringType:
				return expression_from_generator(evaluation.List, [&str, patt] (auto &storage) {
                    const std::string utf8 = str->str();
					const std::string patt_str = static_cast<const String*>(patt)->str();
					size_t pos = 0;
					while (true) {
						size_t new_pos = utf8.find(patt_str, pos);
						if (new_pos == std::string::npos) {
							break;
						}
						storage << patt;
						pos = new_pos + 1;
					}
				});

			case ExpressionType:
				return expression_from_generator(evaluation.List, [str, &patt, &text, &evaluation] (auto &storage) {
                    const CodePointPtr begin(str);
                    const CodePointPtr end = begin + str->length();

                    MatchContext context(patt, text, evaluation.definitions, MatchContext::Unanchored);
                    // FIXME. context should be reset in each iteration step.

					const auto &matcher = patt->as_expression()->string_matcher();
                    CodePointPtr s = begin;
					while (s < end) {
						const CodePointPtr match = matcher->match(context, s, end);
						if (match) {
                            storage << s.slice(match - s);
						}
						s++;
					}
				});

			default:
				return BaseExpressionRef();
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

        std::ostringstream s;

        for (size_t i = 0; i < n; i++) {
            const BaseExpressionRef &leaf = leaves[i];
            if (leaf->type() != StringType) {
                return BaseExpressionRef(); // FIXME
            }
            s << static_cast<const String*>(leaf.get())->str();
        }

        std::string t(s.str());
        return Heap::String(reinterpret_cast<const unicode_t*>(t.c_str()));
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

        const size_t n_value = static_cast<const MachineInteger*>(n)->value;
        const std::string &str_value = static_cast<const String*>(str)->str();

        std::ostringstream s;

        for (size_t i = 0; i < n_value; i++) {
            s << str_value;
        }

        std::string t(s.str());
        return Heap::String(reinterpret_cast<const unicode_t*>(t.c_str()));
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

        const std::string &s = static_cast<const String*>(str)->str();

        const size_t size = utf8proc_decompose(
            reinterpret_cast<const utf8proc_uint8_t*>(s.data()), s.length(), nullptr, 0, utf8proc_option_t(0));

        return Heap::MachineInteger(size);
    }
};

class StringTake : public Builtin {
public:
    static constexpr const char *name = "StringTake";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&StringLength::apply);
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

        const std::string &s = static_cast<const String*>(str)->str();

        utf8proc_int32_t codepoint_ref;
        utf8proc_ssize_t size;
        size_t total_size = 0;

        for (size_t i = 0; i < n_value; i++) {
            size = utf8proc_iterate(
                reinterpret_cast<const utf8proc_uint8_t*>(s.data()), s.length(), &codepoint_ref);
            if (codepoint_ref == -1) {
                break;
            }
            total_size += size;
        }

        return BaseExpressionRef();
    }
};

void Builtins::Strings::initialize() {
	add<StringCases>();
    add<StringJoin>();
    add<StringRepeat>();
    add<StringLength>();
    add<StringTake>();
}
