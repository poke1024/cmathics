#ifndef CMATHICS_SORT_H
#define CMATHICS_SORT_H

class SortKey {
public:
	inline int compare(const SortKey &key) const {
		return 0;
	}
};

class PatternSortKey {
private:
	typedef uint16_t prefix_t;

	inline prefix_t prefix() const {
		return
			(prefix_t(atom_expression) << 9) |
			(prefix_t(mode) << 3) |
			(prefix_t(pattern_test) << 2) |
			(prefix_t(pattern) << 1) |
			(prefix_t(optional) << 0);
	}

public:
	unsigned atom_expression: 2;
	unsigned mode: 6;
	unsigned pattern_test: 1;
	unsigned pattern: 1;
	unsigned optional: 1;

	const Expression *expression;
	unsigned head_pattern_sort: 1;
	unsigned leaf_pattern_sort: 1;
	unsigned leaf_precedence: 1;

	unsigned condition: 1;

	inline PatternSortKey(
		int in_atom_expression,
	    int in_mode,
	    int in_pattern_test,
	    int in_pattern,
	    int in_optional,
		const Expression *in_expression,
	    int in_condition) :

		atom_expression(in_atom_expression),
		mode(in_mode),
		pattern_test(in_pattern_test),
		pattern(in_pattern),
		optional(in_optional),
		expression(in_expression),
		head_pattern_sort(1),
		leaf_pattern_sort(1),
		leaf_precedence(0),
		condition(in_condition) {
	}

	static inline PatternSortKey Blank(int pattern, bool leaves, const Expression *expression) {
		return PatternSortKey(2, pattern, 1, 1, 0, expression, 1);
	}

	static inline PatternSortKey NotAPattern(const Expression *expression) {
		return PatternSortKey(3, 0, 0, 0, 0, expression, 1);
	}

	int compare(const PatternSortKey &key) const;
};

inline int compare_sort_keys(
	const BaseExpressionRef &x,
	const BaseExpressionRef &y,
	int pattern_sort) {

	if (pattern_sort) {
		return x->pattern_key().compare(y->pattern_key());
	} else {
		return x->sort_key().compare(y->sort_key());
	}
}

#endif //CMATHICS_SORT_H
