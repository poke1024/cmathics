#include "types.h"
#include "expression.h"

int SortKey::compare(const SortKey &key) const {
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
		cmp = sort_key(a->head(), head_pattern_sort).compare(
			sort_key(b->head(), key.head_pattern_sort));
		if (cmp) {
			return cmp;
		}

		// now compare the leaves.
		const bool pa = leaf_pattern_sort;
		const bool pb = key.leaf_pattern_sort;

		const bool precedence = leaf_precedence;

		cmp = with_leaves_pair(a, b, [pa, pb, precedence] (auto la, size_t na, auto lb, size_t nb) {
			const size_t size = std::min(na, nb);

			for (size_t i = 0; i < size; i++) {
				const int cmp = sort_key(la(i), pa).compare(sort_key(lb(i), pb));
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