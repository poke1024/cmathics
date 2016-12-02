#include "types.h"
#include "rule.h"

static struct {
	inline bool operator()(const Rules::Entry &entry, const SortKey &key) const {
		return entry.rule->key.compare(key) < 0;
	}
} CompareSortKey;

inline void insert_rule(std::vector<Rules::Entry> &entries, const Rules::Entry &entry) {
	const SortKey key = entry.rule->key;
	const auto i = std::lower_bound(
		entries.begin(), entries.end(), key, CompareSortKey);
	if (i != entries.end() && (*i).rule->pattern->same(entry.rule->pattern)) {
		*i = entry;
	} else {
		entries.insert(i, entry);
	}
}

void Rules::add(const RuleRef &rule) {
	const optional<hash_t> match_hash = rule->match_hash();
	const MatchSize match_size = rule->match_size();
	const Entry entry{rule, match_size, match_hash};
	for (size_t code = 0; code < NumberOfSliceCodes; code++) {
		if (match_size.matches(SliceCode(code))) {
			insert_rule(m_rules[code], entry);
		}
	}
}
