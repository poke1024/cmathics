#ifndef CMATHICS_CACHE_H
#define CMATHICS_CACHE_H

#include <unordered_set>

struct NameNode;

typedef NameNode* Name;

class NameSet {
private:
	std::unordered_set<Name> _names;

protected:
	friend struct NameNode;

	inline void unlink(Name name) {
		_names.erase(name);
	}

public:
	inline ~NameSet();

	inline void add(Name name);

	inline bool contains(Name name) const {
		return _names.find(name) != _names.end();
	}
};

struct NameNode {
	std::unordered_set<NameSet*> _sets;

	inline void link(NameSet *set) {
		_sets.insert(set);
	}

	inline void unlink(NameSet *set) {
		_sets.erase(set);
	}

	inline ~NameNode() {
		for (NameSet *set : _sets) {
			set->unlink(this);
		}
	}
};

inline NameSet::~NameSet() {
	for (Name name : _names) {
		name->unlink(this);
	}
}

inline void NameSet::add(Name name) {
	_names.insert(name);
	try {
		name->link(this);
	} catch(...) {
		_names.erase(name);
		throw;
	}
}

class PatternMatcher;

typedef boost::intrusive_ptr<PatternMatcher> PatternMatcherRef;

class FastLeafSequence;
class SlowLeafSequence;
class AsciiCharacterSequence;
class SimpleCharacterSequence;
class ComplexCharacterSequence;

class PatternMatcher {
private:
    template<typename R, typename F>
    R match(
        MatchContext &context,
        const String *string,
        index_t begin,
        index_t end,
        const F &make_result) const;

protected:
	size_t m_ref_count;
	PatternMatcherRef m_next;
	MatchSize m_size_from_here;
	MatchSize m_size_from_next;

public:
	virtual void set_next(
		const PatternMatcherRef &next) {

		m_next = next;
	}

	virtual void set_size(
		const MatchSize &size_from_here,
		const MatchSize &size_from_next) {

		m_size_from_here = size_from_here;
		m_size_from_next = size_from_next;
	}

	PatternMatcher() : m_ref_count(0) {
	}

	virtual ~PatternMatcher() {
	}

	inline bool might_match(size_t size) const {
		return m_size_from_here.contains(size);
	}

	virtual index_t match(
		MatchContext &context,
		const FastLeafSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		MatchContext &context,
		const SlowLeafSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		MatchContext &context,
		const AsciiCharacterSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		MatchContext &context,
		const SimpleCharacterSequence &sequence,
		index_t begin,
		index_t end) const = 0;

    virtual index_t match(
        MatchContext &context,
        const ComplexCharacterSequence &sequence,
        index_t begin,
        index_t end) const = 0;

    bool matchq(
        MatchContext &context,
        const String *string,
        index_t begin,
        index_t end) const;

    BaseExpressionRef match(
		MatchContext &context,
		const String *string,
		index_t begin,
		index_t end) const;

	friend void intrusive_ptr_add_ref(PatternMatcher *matcher);
	friend void intrusive_ptr_release(PatternMatcher *matcher);
};

inline void intrusive_ptr_add_ref(PatternMatcher *matcher) {
	matcher->m_ref_count++;
}

inline void intrusive_ptr_release(PatternMatcher *matcher) {
	if (--matcher->m_ref_count == 0) {
		delete matcher;
	}
}

class Cache {
private:
	NameNode _name;

public:
	NameSet skip_replace_vars;
	bool skip_slots;
	PatternMatcherRef expression_matcher;
	PatternMatcherRef string_matcher;

	inline Cache() : skip_slots(false) {
	}

	inline Name name() {
		return &_name;
	}
};

#endif //CMATHICS_CACHE_H
