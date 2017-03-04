#pragma once

#include "concurrent/pool.h"

struct SymbolHash {
	inline std::size_t operator()(const Symbol *symbol) const;

	inline std::size_t operator()(const SymbolRef &symbol) const;
};

class SymbolState;

struct SymbolEqual {
	inline bool operator()(const Symbol* lhs, const Symbol *rhs) const {
		return lhs == rhs;
	}

	inline bool operator()(const Symbol* lhs, const SymbolRef &rhs) const {
		return lhs == rhs.get();
	}

	inline bool operator()(const SymbolRef &lhs, const SymbolRef &rhs) const {
		return lhs == rhs;
	}

	inline bool operator()(const SymbolRef &lhs, const Symbol *rhs) const {
		return lhs.get() == rhs;
	}
};

class SymbolKey {
private:
	const SymbolRef _symbol;
	const char * const _name;

public:
	inline SymbolKey(SymbolRef symbol) : _symbol(symbol), _name(nullptr) {
	}

	inline SymbolKey(const char *name) : _name(name) {
	}

	inline SymbolKey(const SymbolKey &key) : _symbol(key._symbol), _name(key._name) {
	}

	inline int compare(const SymbolKey &key) const;

	inline bool operator==(const SymbolKey &key) const;

	inline bool operator<(const SymbolKey &key) const {
		return compare(key) < 0;
	}

	inline const char *c_str() const;
};

namespace std {
	// for the following hash, also see http://stackoverflow.com/questions/98153/
	// whats-the-best-hashing-algorithm-to-use-on-a-stl-string-when-using-hash-map

	template<>
	struct hash<SymbolKey> {
		inline size_t operator()(const SymbolKey &key) const {
			const char *s = key.c_str();

			size_t hash = 0;
			while (*s) {
				hash = hash * 101 + *s++;
			}

			return hash;
		}
	};
}

template<typename Key, typename Value>
using SymbolMap = std::unordered_map<Key, Value,
	SymbolHash, SymbolEqual,
	ObjectAllocator<std::pair<const Key, Value>>>;

template<typename Value>
using SymbolPtrMap = SymbolMap<const Symbol*, Value>;

template<typename Value>
using SymbolRefMap = SymbolMap<const SymbolRef, Value>;

template<typename T>
class CustomAllocatedMap : public T {
private:
	static ObjectAllocator<typename T::value_type> s_allocator;

public:
	inline CustomAllocatedMap() : T(s_allocator) {
	}

	inline CustomAllocatedMap(const CustomAllocatedMap<T> &map) :
		T(map, s_allocator) {
	}

	inline CustomAllocatedMap(const CustomAllocatedMap<T> &&map) :
		T(std::move(map), s_allocator) {
	}
};

template<typename T>
ObjectAllocator<typename T::value_type> CustomAllocatedMap<T>::s_allocator;

using VariableMap = CustomAllocatedMap<SymbolPtrMap<const BaseExpressionRef*>>;

using OptionsMap = CustomAllocatedMap<SymbolRefMap<UnsafeBaseExpressionRef>>;

using ArgumentsMap = CustomAllocatedMap<SymbolRefMap<UnsafeBaseExpressionRef>>;

using SymbolStateMap = CustomAllocatedMap<SymbolRefMap<SymbolState>>;

using MonomialMapBase = std::map<SymbolKey, size_t, std::less<SymbolKey>,
	ObjectAllocator<std::pair<const SymbolKey, size_t>>>;

class MonomialMap : public CustomAllocatedMap<MonomialMapBase> {
public:
	using CustomAllocatedMap<MonomialMapBase>::CustomAllocatedMap;

	MonomialMap &operator=(const MonomialMap &m) {
		clear();
		for (auto i = m.begin(); i != m.end(); i++) {
			insert(*i);
		}
		return *this;
	}
};
