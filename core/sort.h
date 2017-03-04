#ifndef CMATHICS_SORT_H
#define CMATHICS_SORT_H

inline int compare_sort_keys(
    const BaseExpressionRef &x,
    const BaseExpressionRef &y,
    int pattern_sort,
	const Evaluation &evaluation);

inline void increment_monomial(
	MonomialMap &m,
    const SymbolRef &s,
    size_t exp) {

	auto i = m.find(s);
	if (i == m.end()) {
		m[s] = exp;
	} else {
		i->second += exp;
	}
}

class Monomial {
private:
    MonomialMap m_expressions;

public:
    int compare(const Monomial &other) const {
        MonomialMap self_expressions(m_expressions);
        MonomialMap other_expressions(other.m_expressions);

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

	inline Monomial(const Monomial &monomial) :
		m_expressions(monomial.m_expressions) {
	}

	Monomial &operator=(const Monomial &monomial) {
		m_expressions = monomial.m_expressions;
		return *this;
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
        IntegerType,
        CharPointerType,
        HeadType,
        LeavesType,
        MonomialType,
	    GenericType
    };

	struct Data {
		union {
			const char *char_pointer;
			ExpressionPtr expression;
			BaseExpressionPtr generic;
		};
	};

    struct Element {
	    unsigned type: 3;
		unsigned pattern_sort: 1;
	    unsigned precedence: 1;
	    unsigned integer: 8;
    } __attribute__ ((__packed__));

	enum {
		n_elements = 10,
		n_data = 3
	};

    Element m_elements[n_elements];
	Data m_data[n_data];
	optional<Monomial> m_monomial;
	int8_t m_size;
	int8_t m_data_size;

	inline SortKey() {
	}

    template<typename... Args>
    inline void construct(Args&&... args);

    int compare(const SortKey &key, const Evaluation &evaluation) const;

	template<int index>
	inline Data &data() {
		static_assert(index < n_data, "too many data elements");
		return m_data[index];
	}

	inline void set_integer(int index, int value) {
		assert(m_elements[index].type == IntegerType);
		m_elements[index].integer = value;
	}

	inline void set_pattern_test(int value) {
		set_integer(2, value); // see Pattern sort key structure
	}

	inline void set_condition(int value) {
		set_integer(7, value); // see Pattern sort key structure
	}

	inline void set_optional(int value) {
		set_integer(4, value); // see Pattern sort key structure
	}

	inline void append(const char *name) {
		assert(m_size < n_elements);
		assert(m_data_size < n_data);
		auto &element = m_elements[m_size++];
		element.type = SortKey::CharPointerType;
		element.integer = m_data_size;
		m_data[m_data_size++].char_pointer = name;
	}
};

template<typename T, int ElementPosition, int DataPosition>
struct BuildSortKey {
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<int, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition;
	}

	inline void store(SortKey &key, int x) {
		auto &element = key.m_elements[ElementPosition];
		element.type = SortKey::IntegerType;
		element.integer = x;
		assert(int(element.integer) == x);
	}
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<const char*, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition + 1;
	}

	inline void store(SortKey &key, const char *x) {
		auto &element = key.m_elements[ElementPosition];
		element.type = SortKey::CharPointerType;
		element.integer = DataPosition;
		key.data<DataPosition>().char_pointer = x;

	}
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<SortByHead, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition + 1;
	}

	inline void store(SortKey &key, const SortByHead &x) {
		auto &element = key.m_elements[ElementPosition];
		element.type = SortKey::HeadType;
		element.integer = DataPosition;
		element.pattern_sort = x.m_pattern_sort;
		key.data<DataPosition>().expression = x.m_expression;

	}
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<SortByLeaves, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition + 1;
	}

	inline void store(SortKey &key, const SortByLeaves &x) {
		auto &element = key.m_elements[ElementPosition];
		element.type = SortKey::LeavesType;
		element.integer = DataPosition;
		element.pattern_sort = x.m_pattern_sort;
		element.precedence = x.m_precedence;
		key.data<DataPosition>().expression = x.m_expression;
	}
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<MonomialMap, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition;
	}

	inline void store(SortKey &key, MonomialMap &&m) {
		key.m_elements[ElementPosition].type = SortKey::MonomialType;
		assert(!bool(key.m_monomial));
		key.m_monomial.emplace(Monomial(std::move(m)));
	}
};

template<int ElementPosition, int DataPosition>
struct BuildSortKey<BaseExpressionPtr, ElementPosition, DataPosition> {
	constexpr int next_data_position() const {
		return DataPosition + 1;
	}

	inline void store(SortKey &key, BaseExpressionPtr item) {
		auto &element = key.m_elements[ElementPosition];
		element.type = SortKey::GenericType;
		element.integer = DataPosition;
		key.data<DataPosition>().generic = item;
	}
};

template<int ElementPosition, int DataPosition>
void make_sort_key(SortKey &data) {
    data.m_size = ElementPosition;
	data.m_data_size = DataPosition;
}

template<int ElementPosition, int DataPosition, typename T, typename... Args>
void make_sort_key(SortKey &data, T &&x, Args&&... args) {
    static_assert(ElementPosition < sizeof(data.m_elements) / sizeof(SortKey::Element), "too many elements");
	BuildSortKey<typename std::remove_const<
		typename std::remove_reference<T>::type>::type, ElementPosition, DataPosition> build;
	build.store(data, std::forward<T>(x));
    return make_sort_key<ElementPosition + 1, build.next_data_position()>(data, std::forward<Args>(args)...);
}

template<typename... Args>
inline void SortKey::construct(Args&&... args) {
    make_sort_key<0, 0>(*this, std::forward<Args>(args)...);
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

inline void blank_sort_key(SortKey &key, int pattern, size_t size, const Expression *expression) {
	if (size > 0) {
		pattern += 10;
	} else {
		pattern += 20;
	}
	key.construct(
		2, pattern, 1, 1, 0, SortByHead(expression, true), SortByLeaves(expression, true), 1);
}

inline void not_a_pattern_sort_key(SortKey &key, const Expression *expression) {
	key.construct(
		3, 0, 0, 0, 0, SortByHead(expression, true), SortByLeaves(expression, true), 1);
}

inline int compare_sort_keys(
    const BaseExpressionRef &x,
    const BaseExpressionRef &y,
    int pattern_sort,
    const Evaluation &evaluation) {

	SortKey kx;
	SortKey ky;

    if (pattern_sort) {
        x->pattern_key(kx, evaluation);
	    y->pattern_key(ky, evaluation);
	    return kx.compare(ky, evaluation);
    } else {
	    x->sort_key(kx, evaluation);
	    y->sort_key(ky, evaluation);
        return kx.compare(ky, evaluation);
    }
}

#endif //CMATHICS_SORT_H
