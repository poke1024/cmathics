#include "strings.h"
#include "unicode/uchar.h"

class StringMatchQ : public Builtin {
public:
	static constexpr const char *name = "StringMatchQ";

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

		const String * const str = text->as_string();
		bool match;

		switch (patt->type()) {
			case StringType:
				match = patt->as_string()->same(str);
				break;

			case ExpressionType: {
                StringMatcher matcher(patt, evaluation);
				match = matcher.matchq(str, 0, str->length());
				break;
			}

			default:
				match = false;
				break;
		}

		return evaluation.Boolean(match);
	}
};

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
			evaluation.message(m_symbol, "string");
			return BaseExpressionRef();
		}

		const String * const str = text->as_string();

		switch (patt->type()) {
			case StringType:
				return expression_from_generator(evaluation.List, [&str, patt] (auto &storage) {
                    const UnicodeString utext = str->unicode();
					const UnicodeString upatt = static_cast<const String*>(patt)->unicode();
					int32_t pos = 0;
					while (true) {
						const int32_t new_pos = utext.indexOf(upatt, pos);
						if (new_pos < 0) {
							break;
						}
						storage << patt;
						pos = new_pos + 1;
					}
				});

            default: {
				return expression_from_generator(evaluation.List, [str, &patt, &text, &evaluation](auto &storage) {
                    const StringMatcher matcher(patt, evaluation, MatchContext::DoNotAnchor);
					index_t s = 0;
                    const index_t end = str->length();
					while (s < end) {
						const BaseExpressionRef match = matcher.match(str, s, end);
						if (match) {
							storage << match;
						}
						s++;
					}
				});
			}
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
	        return Heap::String(std::make_shared<AsciiStringExtent>(std::move(text)));
	    } else {
			UnicodeString text(number_of_code_points, 0, 0);
		    for (size_t i = 0; i < n; i++) {
			    text.append(leaves[i]->as_string()->unicode());
		    }
	        if (extent_type == StringExtent::simple) {
		        return Heap::String(std::make_shared<SimpleStringExtent>(text));
	        } else {
		        return Heap::String(std::make_shared<ComplexStringExtent>(text));
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

        return Heap::MachineInteger(
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

        UnicodeString s(str->as_string()->unicode());

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
