#ifndef STRING_H
#define STRING_H

#include <string>
#include "unicode/unistr.h"

#include "types.h"
#include "hash.h"

class StringExtent {
public:
    enum Type {
        ascii,
        simple,
        complex
    };

private:
    const Type m_type;

public:
    StringExtent(Type type) : m_type(type) {
    }

    virtual ~StringExtent() {
    }

    inline Type type() const {
        return m_type;
    }

	virtual const UnicodeString &unicode() const = 0;

    virtual size_t length() const = 0; // length in characters (i.e. glyphs)

	virtual size_t number_of_code_points(size_t offset, size_t length) const = 0;

	virtual std::string utf8(size_t offset, size_t length) const = 0;

	virtual UnicodeString unicode(size_t offset, size_t length) const = 0;

    virtual hash_t hash(size_t offset, size_t length) const;

    virtual bool same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const = 0;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const = 0;
};

class AsciiStringExtent : public StringExtent {
private:
    const std::string m_ascii;
	mutable optional<UnicodeString> m_string;

public:
    inline AsciiStringExtent(std::string &&ascii) : StringExtent(StringExtent::ascii), m_ascii(ascii) {
    }

    virtual ~AsciiStringExtent();

    inline const char *data() const {
        return m_ascii.data();
    }

	inline const std::string &ascii() const {
		return m_ascii;
	}

	virtual inline const UnicodeString &unicode() const final {
		if (!m_string) {
			m_string = UnicodeString::fromUTF8(StringPiece(m_ascii));
		}
		return *m_string;
	}

    virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

	virtual size_t length() const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

    virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;
};

class SimpleStringExtent : public StringExtent { // UTF16, fixed size
private:
    UnicodeString m_string;

public:
    inline SimpleStringExtent(UnicodeString &string) : StringExtent(StringExtent::simple) {
	    m_string.moveFrom(string);
    }

	virtual inline const UnicodeString &unicode() const final {
        return m_string;
    }

    virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

    virtual size_t length() const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

	virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;
};

class ComplexStringExtent : public StringExtent { // UTF16, varying size
private:
	// why are we using offset tables instead of UTF32 here? because we want to count and
	// return characters, not code points. see myth 2 at http://utf8everywhere.org/.

    UnicodeString m_string;
	std::vector<int32_t> m_offsets;

public:
    inline ComplexStringExtent(UnicodeString &string, std::vector<int32_t> &offsets) :
	    StringExtent(StringExtent::complex) {
	    m_string.moveFrom(string);
	    std::swap(offsets, m_offsets);
    }

	virtual inline const UnicodeString &unicode() const final {
		return m_string;
	}

	inline const std::vector<int32_t> &offsets() const {
		return m_offsets;
	}

	virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

	virtual size_t length() const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

	virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;
};

StringExtentRef make_string_extent(std::string &&utf8);

class String : public BaseExpression {
private:
	mutable optional<SymbolRef> m_option_symbol; // "System`" + value

    StringExtentRef m_extent;
    size_t m_offset;
    size_t m_length;

protected:
    friend class CharacterPtr;

    const StringExtentRef &extent() const {
        return m_extent;
    }

    inline size_t to_extent_offset(size_t offset) const {
        return m_offset + offset;
    }

public:
	inline String(const std::string &utf8) : BaseExpression(StringExtendedType) {
		m_extent = make_string_extent(std::string(utf8));
		m_offset = 0;
		m_length = m_extent->length();
	}

	inline String(std::string &utf8) : BaseExpression(StringExtendedType) {
        m_extent = make_string_extent(std::move(utf8));
        m_offset = 0;
        m_length = m_extent->length();
    }

	inline String(const StringExtentRef &extent) : BaseExpression(StringExtendedType) {
		m_extent = extent;
		m_offset = 0;
		m_length = m_extent->length();
	}

	inline String(const StringExtentRef &extent, size_t offset, size_t length) : BaseExpression(StringExtendedType) {
        m_extent = extent;
        m_offset = offset;
        m_length = length;
    }

	inline StringExtent::Type extent_type() const {
		return m_extent->type();
	}

    inline bool same_n(const String *s, size_t offset, size_t n) const {
        if (n > m_length) {
            return false;
        } else if (offset + n > s->m_length) {
            return false;
        }

        return m_extent->same_n(
            s->extent().get(), m_offset, s->to_extent_offset(offset), n);
    }

	inline SymbolRef option_symbol(const Evaluation &evaluation) const;

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == StringType) {
            const String * const other = static_cast<const String*>(&expr);
            const size_t n = m_length;

            if (n != other->m_length) {
                return false;
            }

            return m_extent->same_n(
                other->m_extent.get(), m_offset, other->m_offset, n);
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        return hash_pair(string_hash, m_extent->hash(m_offset, m_length));
    }

	inline UnicodeString unicode() const {
		return m_extent->unicode(m_offset, m_length);
	}

    inline std::string utf8() const {
        return m_extent->utf8(m_offset, m_length);
    }

	inline const char *ascii() const {
		if (m_extent->type() == StringExtent::ascii) {
			return static_cast<AsciiStringExtent*>(m_extent.get())->data() + m_offset;
		} else {
			throw std::runtime_error("ascii not supported");
		}
	}

    virtual std::string fullform() const {
        return utf8();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    inline size_t length() const {
        return m_length;
    }

	inline size_t number_of_code_points() const {
		return m_extent->number_of_code_points(m_offset, m_length);
	}

    String take(index_t n) const;

	inline StringRef repeat(size_t n) const {
		return Heap::String(m_extent->repeat(m_offset, m_length, n));
	}
};

inline const String *BaseExpression::as_string() const {
	return static_cast<const String*>(this);
}

inline BaseExpressionRef from_primitive(const std::string &value) {
    return BaseExpressionRef(new String(value));
}

class CharacterPtr {
private:
    const String *m_string;
    size_t m_offset;

public:
    inline CharacterPtr(
        const String *string,
        size_t offset = 0) :

        m_string(string),
        m_offset(offset) {
    }

    inline CharacterPtr(std::nullptr_t) :
        m_string(nullptr),
        m_offset(0)  {
    }

    inline const String *string() const {
        return m_string;
    }

    inline size_t offset() const {
        return m_offset;
    }

    inline StringRef operator*() const {
        return StringRef(); // not implemented
    }

    inline StringRef operator[](size_t i) const {
        return StringRef(); // not implemented
    }

    inline bool operator==(const CharacterPtr &ptr) const {
        assert(m_string == ptr.m_string);
        return m_offset == ptr.m_offset;
    }

    inline bool operator<(const CharacterPtr &ptr) const {
        assert(m_string == ptr.m_string);
        return m_offset < ptr.m_offset;
    }

    inline CharacterPtr operator+(int i) const {
        return CharacterPtr(m_string, m_offset + i);
    }

    inline CharacterPtr operator+(index_t i) const {
        return CharacterPtr(m_string, m_offset + i);
    }

    inline CharacterPtr operator+(size_t i) const {
        return CharacterPtr(m_string, m_offset + i);
    }

    inline CharacterPtr operator++(int) {
        const CharacterPtr old = CharacterPtr(m_string, m_offset);
        m_offset += 1;
        return old;
    }

    inline index_t operator-(const CharacterPtr& ptr) const {
        assert(m_string == ptr.m_string);
        return m_offset - ptr.m_offset;
    }

    inline operator bool() const {
        return m_string;
    }

    inline BaseExpressionRef slice(size_t n) const {
        return Heap::String(m_string->extent(), m_string->to_extent_offset(m_offset), n);
    }
};

#endif
