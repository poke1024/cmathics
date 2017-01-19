#ifndef CMATHICS_SORT_H
#define CMATHICS_SORT_H

class Monomial {
private:
    MonomialMap m_expressions;

public:
    int compare(const Monomial &other) const {
        MonomialMap self_expressions(
            m_expressions, Pool::monomial_map_allocator());
        MonomialMap other_expressions(
            other.m_expressions, Pool::monomial_map_allocator());

        auto i = self_expressions.begin();
        while (i != self_expressions.end()) {
            auto j = other_expressions.find(i->first);
            if (j != other_expressions.end()) {
                size_t decrement = std::min(i->second, j->second);

                if ((i->second -= decrement) == 0) {
                    i = self_expressions.erase(i);
                } else {
                    i++;
                }

                if ((j->second -= decrement) == 0) {
                    other_expressions.erase(j);
                }
            } else {
                i++;
            }
        }

        i = self_expressions.begin();
        auto j = other_expressions.begin();

        while (true) {
            if (i == self_expressions.end() && j == other_expressions.end()) {
                return 0;
            } else if (i == self_expressions.end()) {
                return -1; // this < other
            } else if (j == other_expressions.end()) {
                return 1; // this > other
            }

            const int z = i->first.compare(j->first);
            if (z) {
                return z;
            }

            auto next_i = i;
            next_i++;
            auto next_j = j;
            next_j++;

            if (i->second != j->second) {
                if (next_i == self_expressions.end() || next_j == other_expressions.end()) {
                    // smaller exponents first
                    if (i->second < j->second) {
                        return -1;
                    } else {
                        return 1;
                    }
                } else {
                    // bigger exponents first
                    if (i->second < j->second) {
                        return 1;
                    } else {
                        return -1;
                    }
                }
            }

            i = next_i;
            j = next_j;
        }

        return 0;
    }

public:
    inline Monomial(MonomialMap &&expressions) :
        m_expressions(std::move(expressions)) {
    }

    inline Monomial(Monomial &&monomial) :
        m_expressions(std::move(monomial.m_expressions)) {
    }
};

class SortKey {
public:
    unsigned index1: 2;
    unsigned index2: 2;
    Monomial monomial;
    /*const Expression *expression;
    unsigned index3: 1;*/

    inline SortKey(
        int in_index1,
        int in_index2,
        MonomialMap &&in_monomial) :

        index1(in_index1),
        index2(in_index2),
        monomial(std::move(in_monomial)) {
    }

    inline SortKey(SortKey &&key) :
        index1(key.index1),
        index2(key.index2),
        monomial(std::move(key.monomial)) {
    }

	inline int compare(const SortKey &key) const {
        int cmp1 = int(index1) - int(key.index1);
        if (cmp1) {
            return cmp1;
        }

        int cmp2 = int(index2) - int(key.index2);
        if (cmp2) {
            return cmp2;
        }

        int cmp3 = monomial.compare(key.monomial);
        if (cmp3) {
            return cmp3;
        }

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
