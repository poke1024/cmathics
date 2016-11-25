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

class Cache {
private:
	NameNode _name;

public:
	NameSet skip_replace_vars;

	bool skip_slots;

	inline Cache() : skip_slots(false) {
	}

	inline Name name() {
		return &_name;
	}
};

#endif //CMATHICS_CACHE_H
