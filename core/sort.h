#ifndef CMATHICS_SORT_H
#define CMATHICS_SORT_H

inline int compare_sort_keys(
    const BaseExpressionRef &x,
    const BaseExpressionRef &y,
    int pattern_sort);

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

class SortByHead {
public:
	const Expression * const m_expression;
	const bool m_pattern_sort;

	inline SortByHead(const Expression *expression, bool pattern_sort = false) :
		m_expression(expression), m_pattern_sort(pattern_sort) {
	}
};

class SortByLeaves {
public:
	const Expression * const m_expression;
	const bool m_pattern_sort;
	const bool m_precedence;

	inline SortByLeaves(const Expression *expression, bool pattern_sort = false, bool precedence = false) :
		m_expression(expression), m_pattern_sort(pattern_sort), m_precedence(precedence) {
	}
};

struct SortKey {
    enum Type {
        IntegerType = 0,
        StringType = 1,
        HeadType = 2,
        LeavesType = 3,
        MonomialType = 4
    };

    struct Element {
	    union {
		    int8_t integer;
		    const char *string;
		    const Expression *expression;
	    };
	    unsigned type: 3;
		unsigned pattern_sort: 1;
	    unsigned precedence: 1;

	    inline void set(SortKey &key, int x) {
		    integer = x;
		    type = IntegerType;
	    }

        inline void set(SortKey &key, const char *s) {
			string = s;
	        type = StringType;
        }

	    inline void set(SortKey &key, const SortByHead &x) {
		    expression = x.m_expression;
		    pattern_sort = x.m_pattern_sort;
		    type = HeadType;
	    }

	    inline void set(SortKey &key, const SortByLeaves &x) {
		    expression = x.m_expression;
		    pattern_sort = x.m_pattern_sort;
		    precedence = x.m_precedence;
		    type = LeavesType;
	    }

	    inline void set(SortKey &key, MonomialMap &&m) {
		    assert(!bool(key.monomial));
		    key.monomial.emplace(Monomial(std::move(m)));
		    type = MonomialType;
	    }
    };

    Element elements[10];
	optional<Monomial> monomial;
	int size;

    template<typename... Args>
    SortKey(Args&&... args);

    int compare(const SortKey &key) const;

	void set_integer(int index, int value) {
		assert(elements[index].type == IntegerType);
		elements[index].integer = value;
	}

	void set_pattern_test(int value) {
		set_integer(2, value); // see Pattern sort key structure
	}

	void set_condition(int value) {
		set_integer(7, value); // see Pattern sort key structure
	}

	void set_optional(int value) {
		set_integer(4, value); // see Pattern sort key structure
	}
};

template<int Position>
void make_sort_key(SortKey &data) {
    data.size = Position;
}

template<int Position, typename T, typename... Args>
void make_sort_key(SortKey &data, T &&x, Args&&... args) {
    static_assert(Position < sizeof(data.elements) / sizeof(SortKey::Element), "Position too large");
    data.elements[Position].set(data, std::move(x));
    return make_sort_key<Position + 1>(data, std::move(args)...);
}

template<typename... Args>
SortKey::SortKey(Args&&... args) {
    make_sort_key<0>(*this, 0, std::move(args)...);
}

/*
Pattern sort key structure:
0: 0/2:        Atom / Expression
1: pattern:    0 / 11-31 for blanks / 1 for empty Alternatives / 40 for OptionsPattern
2: 0/1:        0 for PatternTest
3: 0/1:        0 for Pattern
4: 0/1:        1 for Optional
5: head / 0 for atoms
6: leaves / 0 for atoms
7: 0/1:        0 for Condition
*/

inline SortKey blank_sort_key(int pattern, bool leaves, const Expression *expression) {
	return SortKey(2, pattern, 1, 1, 0, SortByHead(expression, true), SortByLeaves(expression, true), 1);
}

inline SortKey not_a_pattern_sort_key(const Expression *expression) {
	return SortKey(3, 0, 0, 0, 0, SortByHead(expression, true), SortByLeaves(expression, true), 1);
}

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
