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

struct SortKey {
    enum Type {
        IntegerType = 0,
        StringType = 1,
        HeadType = 2,
        LeavesType = 3,
        MonomialType = 4,

        TypeBits = 3,
        TypeMask = (1L << TypeBits) - 1L
    };

    using TypeArray = uint64_t;

    TypeArray mask;
    int size;

    struct Element {
        int8_t integer;
        const char *string;
        Expression *expression;
        optional<Monomial> monomial;
    };

    Element elements[8];

    template<typename... Args>
    SortKey(Args&&... args);

    int compare(const SortKey &key) const {
        int min_size = std::min(size, key.size);

        for (int i = 0; i < min_size; i++) {
            const int t = (mask >> (SortKey::TypeBits * i)) & SortKey::TypeMask;
            assert (t == ((key.mask >> (SortKey::TypeBits * i)) & SortKey::TypeMask));

            int cmp = 0;
            switch (t) {
                case IntegerType:
                    cmp = int(elements[i].integer) - int(key.elements[i].integer);
                    break;

                case StringType:
                    cmp = strcmp(elements[i].string, key.elements[i].string);
                    break;

                case HeadType:
                    cmp = compare_sort_keys(
                        elements[i].expression->head(),
                        key.elements[i].expression->head(),
                        true /* head_pattern_sort */);
                    break;

                case MonomialType:
                    cmp = elements[i].monomial->compare(*key.elements[i].monomial);
                    break;
            }

            if (cmp) {
                return cmp;
            }
        }

        if (size > key.size) {
            return 1;
        } else if (size < key.size) {
            return -1;
        }

        return 0;
    }
};

constexpr SortKey::TypeArray add(SortKey::TypeArray mask, SortKey::Type type, int pos) {
    return mask | (SortKey::TypeArray(type) << (SortKey::TypeBits * pos));
}

template<int Position, SortKey::TypeArray mask>
void make_sort_key(SortKey &data) {
    data.mask = mask;
    data.size = Position;
}

template<int Position, SortKey::TypeArray mask, typename... Args>
void make_sort_key(SortKey &data, MonomialMap &&m, Args&&... args) {
    static_assert(Position < sizeof(data.elements) / sizeof(SortKey::Element), "Position too large");
    data.elements[Position].monomial.emplace(Monomial(std::move(m)));
    return make_sort_key<Position + 1, add(mask, SortKey::MonomialType, Position)>(data, std::move(args)...);
}

template<int Position, SortKey::TypeArray mask, typename... Args>
void make_sort_key(SortKey &data, const char *s, Args&&... args) {
    static_assert(Position < sizeof(data.elements) / sizeof(SortKey::Element), "Position too large");
    data.elements[Position].string = s;
    return make_sort_key<Position + 1, add(mask, SortKey::StringType, Position)>(data, std::move(args)...);
}

template<int Position, SortKey::TypeArray mask, typename... Args>
void make_sort_key(SortKey &data, int x, Args&&... args) {
    static_assert(Position < sizeof(data.elements) / sizeof(SortKey::Element), "Position too large");
    data.elements[Position].integer = x;
    return make_sort_key<Position + 1, add(mask, SortKey::IntegerType, Position)>(data, std::move(args)...);
}

template<typename... Args>
SortKey::SortKey(Args&&... args) {
    make_sort_key<0, 0>(*this, std::move(args)...);
}

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
