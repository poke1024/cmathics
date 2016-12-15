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

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const = 0;
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

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;
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

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;
};

std::vector<int32_t> make_character_offsets(const UnicodeString &normalized);

class ComplexStringExtent : public StringExtent { // UTF16, varying size
private:
	// why are we using offset tables instead of UTF32 here? because we want to count and
	// return characters, not code points. see myth 2 at http://utf8everywhere.org/.

    UnicodeString m_string;
	std::vector<int32_t> m_offsets;

public:
    inline ComplexStringExtent(UnicodeString &normalized_string) :
        StringExtent(StringExtent::complex) {
        m_string.moveFrom(normalized_string);
        m_offsets = make_character_offsets(normalized_string);
    }

    inline ComplexStringExtent(UnicodeString &normalized_string, std::vector<int32_t> &offsets) :
	    StringExtent(StringExtent::complex) {
	    m_string.moveFrom(normalized_string);
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

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;
};

StringExtentRef make_string_extent(std::string &&utf8);

StringExtentRef string_extent_from_normalized(UnicodeString &&normalized, uint8_t possible_types = 0xff);

class String : public BaseExpression {
private:
	mutable optional<SymbolRef> m_option_symbol; // "System`" + value

    const StringExtentRef m_extent;
    const size_t m_offset;
    const size_t m_length;

protected:
    friend class CharacterSequence;

    const StringExtentRef &extent() const {
        return m_extent;
    }

    inline size_t to_extent_offset(size_t offset) const {
        return m_offset + offset;
    }

public:
	inline String(const std::string &utf8) :
        BaseExpression(StringExtendedType),
        m_extent(make_string_extent(std::string(utf8))),
        m_offset(0),
        m_length(m_extent->length()) {
	}

	inline String(std::string &utf8) :
        BaseExpression(StringExtendedType),
        m_extent(make_string_extent(std::move(utf8))),
        m_offset(0),
        m_length(m_extent->length()) {
    }

	inline String(const StringExtentRef &extent) :
        BaseExpression(StringExtendedType),
        m_extent(extent),
        m_offset(0),
        m_length(extent->length()) {
	}

	inline String(const StringExtentRef &extent, size_t offset, size_t length) :
        BaseExpression(StringExtendedType),
        m_extent(extent),
        m_offset(offset),
        m_length(length) {
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

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

    inline StringRef take(index_t n) const {
        if (n >= 0) {
            return Heap::String(m_extent, m_offset, std::min(n, index_t(m_length)));
        } else {
            n = std::min(-n, index_t(m_length));
            return Heap::String(m_extent, m_offset + m_length - n, n);
        }
    }

    inline StringRef drop(index_t n) const {
        if (n >= 0) {
            n = std::min(n, index_t(m_length));
            return Heap::String(m_extent, m_offset + n, m_length - n);
        } else {
            n = std::min(-n, index_t(m_length));
            return Heap::String(m_extent, m_offset, m_length - n);
        }
    }

	inline StringRef repeat(size_t n) const {
		return Heap::String(m_extent->repeat(m_offset, m_length, n));
	}

    inline size_t number_of_code_points() const {
        return m_extent->number_of_code_points(m_offset, m_length);
    }

    inline StringRef strip_code_points(index_t cp_left, index_t cp_right) const {
        const size_t shift = m_extent->walk_code_points(m_offset, cp_left);
        return Heap::String(
            m_extent,
            m_offset + shift,
            m_length - shift - m_extent->walk_code_points(m_offset + m_length, -cp_right));
    }
};

inline const String *BaseExpression::as_string() const {
	return static_cast<const String*>(this);
}

inline BaseExpressionRef from_primitive(const std::string &value) {
    return BaseExpressionRef(new String(value));
}

class CharacterSequence {
private:
    const String * const m_string;

public:
    class Element {
    private:
        const String * const m_string;
        const index_t m_begin;
        BaseExpressionRef m_element;

    public:
        inline Element(const String *string, index_t begin) : m_string(string), m_begin(begin) {
        }

        inline const BaseExpressionRef &operator*() {
            if (!m_element) {
                m_element = Heap::String(
                    m_string->extent(),
                    m_string->to_extent_offset(m_begin),
                    m_string->to_extent_offset(m_begin + 1));
            }
            return m_element;
        }
    };

    class Sequence {
    private:
        const String * const m_string;
        const index_t m_begin;
        const index_t m_end;
        BaseExpressionRef m_sequence;

    public:
        inline Sequence(const String *string, index_t begin, index_t end) :
            m_string(string), m_begin(begin), m_end(end) {
        }

        inline const BaseExpressionRef &operator*() {
            if (!m_sequence) {
                m_sequence = Heap::String(
                    m_string->extent(),
                    m_string->to_extent_offset(m_begin),
                    m_end - m_begin);
            }
            return m_sequence;
        }
    };

    inline CharacterSequence(const String *string) : m_string(string) {
    }

    inline Element element(index_t begin) const {
        return Element(m_string, begin);
    }

    inline Sequence sequence(index_t begin, index_t end) const {
        assert(begin <= end);
        return Sequence(m_string, begin, end);
    }

    inline index_t same(index_t begin, BaseExpressionPtr other) const {
        const String * const other_string = other->as_string();
        const size_t n = other_string->length();

        if (other_string->same_n(m_string, begin, n)) {
            return begin + n;
        } else {
            return -1;
        }
    }
};

#endif
