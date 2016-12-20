#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>

class PatternMatcher;

typedef boost::intrusive_ptr<PatternMatcher> PatternMatcherRef;

class ReplaceCache {
private:
	const std::unordered_set<const Expression*> m_skipped;

public:
	inline ReplaceCache(std::unordered_set<const Expression*> &&skipped) : m_skipped(skipped) {
	}

	inline bool skip(const Expression* expr) const {
		return m_skipped.find(expr) != m_skipped.end();
	}
};

typedef std::shared_ptr<ReplaceCache> ReplaceCacheRef;

class Cache {
protected:
	std::atomic<size_t> m_ref_count;

	friend void intrusive_ptr_add_ref(Cache *cache);
	friend void intrusive_ptr_release(Cache *cache);

public:
	std::atomic<bool> skip_slots;
	ReplaceCacheRef replace_cache;
	PatternMatcherRef expression_matcher;
	PatternMatcherRef string_matcher;

	inline Cache() : m_ref_count(0), skip_slots(false) {
	}
};

typedef boost::intrusive_ptr<Cache> CacheRef;

#endif //CMATHICS_CACHE_H
