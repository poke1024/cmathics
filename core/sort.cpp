#include "types.h"
#include "core/expression/implementation.h"
#include "arithmetic/compare.tcc"

int SortKey::compare(
	const SortKey &key,
	const Evaluation &evaluation) const {

	// note: if evaluation == nullptr, this can only safely
	// perform pattern comparisons. for all non-pattern
	// comparisons, we always assume evaluation != nullptr.

	int min_size = std::min(m_size, key.m_size);

	for (int i = 0; i < min_size; i++) {
		const Element &x = m_elements[i];
		const Element &y = key.m_elements[i];

		const int t = x.type;
		assert(t == y.type);

		int cmp = 0;
		switch (t) {
			case IntegerType:
				cmp = int(x.integer) - int(y.integer);
				break;

			case CharPointerType:
				cmp = strcmp(
					m_data[x.integer].char_pointer,
					key.m_data[y.integer].char_pointer);
				break;

			case HeadType:
				cmp = compare_sort_keys(
					m_data[x.integer].expression->head(),
					key.m_data[y.integer].expression->head(),
					x.pattern_sort,
					evaluation);
				break;

			case LeavesType: {
				const bool pattern_sort = x.pattern_sort;
				const bool precedence = x.precedence;

				cmp = with_slices(
					m_data[x.integer].expression,
					key.m_data[y.integer].expression,
					[pattern_sort, precedence, &evaluation] (const auto &slice_a, const auto &slice_b) {
						const size_t na = slice_a.size();
						const size_t nb = slice_b.size();

						const size_t size = std::min(na, nb);

						for (size_t i = 0; i < size; i++) {
							const int cmp = compare_sort_keys(
								slice_a[i], slice_b[i], pattern_sort, evaluation);
							if (cmp) {
								return cmp;
							}
						}

						if (precedence) {
							return 1;
						} else if (na < nb) {
							return -1;
						} else if (na > nb) {
							return 1;
						} else {
							return 0;
						}
					});
				break;
			}

			case MonomialType:
				cmp = m_monomial->compare(*key.m_monomial);
				break;

			case GenericType: {
				const auto &order = *evaluation.definitions.order;
				cmp = order(
					m_data[x.integer].generic,
		            key.m_data[y.integer].generic,
		            evaluation);
				break;
			}
		}

		if (cmp) {
			return cmp;
		}
	}

	if (m_size > key.m_size) {
		return 1;
	} else if (m_size < key.m_size) {
		return -1;
	}

	return 0;
}
