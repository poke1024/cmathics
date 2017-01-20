#include "types.h"
#include "expression.h"

int SortKey::compare(const SortKey &key) const {
	int min_size = std::min(size, key.size);

	for (int i = 0; i < min_size; i++) {
		const Element &x = elements[i];
		const Element &y = key.elements[i];

		const int t = x.type;
		assert(t == y.type);

		int cmp = 0;
		switch (t) {
			case IntegerType:
				cmp = int(x.integer) - int(y.integer);
				break;

			case StringType:
				cmp = strcmp(x.string, y.string);
				break;

			case HeadType:
				cmp = compare_sort_keys(
					x.expression->head(),
					y.expression->head(),
					x.pattern_sort);
				break;

			case LeavesType: {
				const bool pattern_sort = x.pattern_sort;
				const bool precedence = x.precedence;

				cmp = with_slices(
					x.expression,
					y.expression,
					[pattern_sort, precedence] (const auto &slice_a, const auto &slice_b) {
						const size_t na = slice_a.size();
						const size_t nb = slice_b.size();

						const size_t size = std::min(na, nb);

						for (size_t i = 0; i < size; i++) {
							const int cmp = compare_sort_keys(slice_a[i], slice_b[i], pattern_sort);
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
				cmp = monomial->compare(*key.monomial);
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
