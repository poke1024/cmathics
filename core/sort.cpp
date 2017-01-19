#include "types.h"
#include "expression.h"

int PatternSortKey::compare(const PatternSortKey &key) const {
	int cmp;

	const prefix_t p1 = prefix();
	const prefix_t p2 = key.prefix();

	if (p1 < p2) {
		return -1;
	} else if (p1 > p2) {
		return 1;
	}

	// if we're here, both "this" and "key" are
	// Expressions or both are not.

	const Expression *a = expression;
	const Expression *b = key.expression;

	if (a && b) {
		// compare the heads.
		assert(head_pattern_sort == key.head_pattern_sort);
		cmp = compare_sort_keys(a->head(), b->head(), head_pattern_sort);
		if (cmp) {
			return cmp;
		}

		// now compare the leaves.
		const bool pa = leaf_pattern_sort;
		const bool pb = key.leaf_pattern_sort;
		assert(pa == pb);

		const bool precedence = leaf_precedence;

		cmp = with_slices(a, b, [pa, pb, precedence] (const auto &slice_a, const auto &slice_b) {
			const size_t na = slice_a.size();
			const size_t nb = slice_b.size();

			const size_t size = std::min(na, nb);

			for (size_t i = 0; i < size; i++) {
				const int cmp = compare_sort_keys(slice_a[i], slice_b[i], pa);
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

		if (cmp) {
			return cmp;
		}
	}

	return condition - key.condition;
}