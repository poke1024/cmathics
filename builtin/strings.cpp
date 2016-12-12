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

		const std::string str = static_cast<const String*>(text)->str();

		PatternMatcherRef matcher = compile_string_pattern(BaseExpressionRef(patt));

		MatchContext context(patt, text, evaluation.definitions, MatchContext::Unanchored);

		switch (patt->type()) {
			case StringType:
				return expression_from_generator(evaluation.List, [&context, &str, patt] (auto &storage) {
					const std::string patt_str = static_cast<const String*>(patt)->str();
					const auto ref = Heap::String(patt_str.c_str());
					size_t pos = 0;
					while (true) {
						size_t new_pos = str.find(patt_str, pos);
						if (new_pos == std::string::npos) {
							break;
						}
						storage << ref;
						pos = new_pos + 1;
					}
				});
			case ExpressionType:
				return expression_from_generator(evaluation.List, [&context, &str, patt] (auto &storage) {
					const char *s = str.c_str();
					const char *end = s + str.length();

					const auto &matcher = patt->as_expression()->string_matcher();
					while (s < end) {
						const char *match = matcher->match(context, s, end);
						if (match) {
							const std::string part(s, match - s);
							storage << Heap::String(part.c_str());
						}
						s++;
					}
				});
			default:
				return BaseExpressionRef();
		}
	}
};

void Builtins::Strings::initialize() {
	add<StringCases>();
}
