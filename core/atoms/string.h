#ifndef STRING_H
#define STRING_H

#include <string>
#include "unicode/unistr.h"

#include "core/types.h"
#include "core/hash.h"

class StringExtent : public AbstractHeapObject<StringExtent> {
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

	virtual UnicodeString unicode() const = 0;

    virtual size_t length() const = 0; // length in characters (i.e. glyphs)

	virtual char ascii_char_at(size_t offset) const = 0;

	virtual size_t number_of_code_points(size_t offset, size_t length) const = 0;

	virtual std::string utf8(size_t offset, size_t length) const = 0;

	virtual UnicodeString unicode(size_t offset, size_t length) const = 0;

    virtual hash_t hash(size_t offset, size_t length) const;

    virtual bool same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const = 0;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const = 0;

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const = 0;
};

class AsciiStringExtent : public StringExtent, public ExtendedHeapObject<AsciiStringExtent> {
private:
    typedef std::shared_ptr<UnicodeString> UnicodeStringRef;

    const std::string m_ascii;
	mutable UnicodeStringRef m_string;

public:
    inline AsciiStringExtent(std::string &&ascii) : StringExtent(StringExtent::ascii), m_ascii(ascii) {
    }

    virtual ~AsciiStringExtent();

    static constexpr Type extent_type = ascii;

    inline const char *data() const {
        return m_ascii.data();
    }

	inline const std::string &ascii() const {
		return m_ascii;
	}

	virtual UnicodeString unicode() const final { // concurrent.
		const UnicodeStringRef string = std::atomic_load(&m_string);
		if (string) {
			return *string;
		} else {
			UnicodeStringRef new_string = std::make_shared<UnicodeString>(
				UnicodeString::fromUTF8(StringPiece(m_ascii)));
			std::atomic_store(&m_string, new_string);
			return *new_string;
		}
	}

    virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

	virtual size_t length() const;

	virtual char ascii_char_at(size_t offset) const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

    virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;

    template<typename F>
    inline bool all_code_points(size_t offset, const F &f) const {
        return f(m_ascii[offset]);
    }

	inline bool is_word_boundary(size_t begin, size_t end, size_t offset) const {
		if (offset == begin) { // before the first character
			return isalnum(m_ascii[begin]);
		} else if (offset == end) { // after the last character
			return end > begin && isalnum(m_ascii[end - 1]);
		} else if (offset > begin && offset < end) {
			return isalnum(m_ascii[offset]) != isalnum(m_ascii[offset - 1]);
		} else {
			return false;
		}
	}
};

class SimpleStringExtent : public StringExtent, public ExtendedHeapObject<SimpleStringExtent> { // UTF16, fixed size
private:
    UnicodeString m_string;
	std::vector<bool> m_word_boundaries;

public:
    inline SimpleStringExtent(UnicodeString &string) : StringExtent(StringExtent::simple) {
	    m_string.moveFrom(string);
    }

    static constexpr Type extent_type = simple;

    virtual inline UnicodeString unicode() const final {
        return m_string;
    }

    virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

    virtual size_t length() const;

	virtual char ascii_char_at(size_t offset) const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

	virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;

    template<typename F>
    inline bool all_code_points(size_t offset, const F &f) const {
        return f(m_string[offset]);
    }

	inline bool is_word_boundary(size_t begin, size_t end, size_t offset) const {
		return false; // FIXME
	}
};

std::vector<int32_t> make_character_offsets(const UnicodeString &normalized);

class ComplexStringExtent : public StringExtent, public ExtendedHeapObject<ComplexStringExtent> { // UTF16, varying size
private:
	// why are we using offset tables instead of UTF32 here? because we want to count and
	// return characters, not code points. see myth 2 at http://utf8everywhere.org/.

    UnicodeString m_string;
	std::vector<int32_t> m_offsets;
	std::vector<bool> m_word_boundaries;

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

    static constexpr Type extent_type = complex;

    virtual inline UnicodeString unicode() const final {
		return m_string;
	}

	inline const std::vector<int32_t> &offsets() const {
		return m_offsets;
	}

	virtual std::string utf8(size_t offset, size_t length) const;

	virtual UnicodeString unicode(size_t offset, size_t length) const;

	virtual size_t length() const;

	virtual char ascii_char_at(size_t offset) const;

	virtual size_t number_of_code_points(size_t offset, size_t length) const;

	virtual bool same_n(const StringExtent *x, size_t offset, size_t x_offset, size_t n) const;

	virtual StringExtentRef repeat(size_t offset, size_t length, size_t n) const;

    virtual size_t walk_code_points(size_t offset, index_t cp_offset) const;

    template<typename F>
    inline bool all_code_points(size_t offset, const F &f) const {
        const size_t begin = m_offsets[offset];
        const size_t end = m_offsets[offset + 1];
        for (size_t i = begin; i < end; i++) {
            if (!f(m_string[i])) {
                return false;
            }
        }
        return true;
    }

	inline bool is_word_boundary(size_t begin, size_t end, size_t offset) const {
		return false; // FIXME
	}
};

StringExtentRef make_string_extent(std::string &&utf8);

StringExtentRef string_extent_from_normalized(UnicodeString &&normalized, uint8_t possible_types = 0xff);

template<typename Extent>
class CharacterSequence;

class String : public BaseExpression, public PoolObject<String> {
private:
	mutable MutableSymbolRef m_option_symbol; // "System`" + value
    mutable optional<hash_t> m_hash;

    const StringExtentRef m_extent;
    const index_t m_offset;
    const index_t m_length;

protected:
    template<typename Extent>
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

	virtual std::string debugform() const;

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

	virtual std::string boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

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

    inline bool same(const String *s) const {
        return s->m_length == m_length && same_n(s, 0, s->m_length);
    }

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_string()) {
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
        if (!m_hash) {
            m_hash = m_extent->hash(m_offset, m_length);
        }
        return hash_pair(string_hash, *m_hash);
    }

	inline UnicodeString unicode() const {
		return m_extent->unicode(m_offset, m_length);
	}

    inline std::string utf8() const {
        return m_extent->utf8(m_offset, m_length);
    }

    inline std::string unquoted_utf8() const {
        if (m_length > 0 &&
            m_extent->ascii_char_at(m_offset) == '"' &&
            m_extent->ascii_char_at(m_offset + m_length - 1) == '"') {
            return m_extent->utf8(m_offset + 1, m_length - 2);
        } else {
            return m_extent->utf8(m_offset, m_length);
        }
    }

	inline const char *ascii() const {
		if (m_extent->type() == StringExtent::ascii) {
			return static_cast<AsciiStringExtent*>(m_extent.get())->data() + m_offset;
		} else {
			return nullptr;
		}
	}
	inline char ascii_char_at(index_t index) const {
		assert(index >= 0 && index < m_length);
		return m_extent->ascii_char_at(index + m_offset);
	}

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual MatchSize string_match_size() const {
        return MatchSize::exactly(m_length);
    }

    inline size_t length() const {
        return m_length;
    }

	inline StringRef substr(index_t begin, index_t end = INDEX_MAX) const {
		assert(begin >= 0);
		return String::construct(
			m_extent,
			m_offset + begin,
			std::max(index_t(0), std::min(end - begin, m_length - begin)));
	}

    inline StringRef take(index_t n) const {
        if (n >= 0) {
            return String::construct(m_extent, m_offset, std::min(n, index_t(m_length)));
        } else {
            n = std::min(-n, index_t(m_length));
            return String::construct(m_extent, m_offset + m_length - n, n);
        }
    }

    inline StringRef drop(index_t n) const {
        if (n >= 0) {
            n = std::min(n, index_t(m_length));
            return String::construct(m_extent, m_offset + n, m_length - n);
        } else {
            n = std::min(-n, index_t(m_length));
            return String::construct(m_extent, m_offset, m_length - n);
        }
    }

	inline StringRef repeat(size_t n) const {
		return String::construct(m_extent->repeat(m_offset, m_length, n));
	}

    inline size_t number_of_code_points() const {
        return m_extent->number_of_code_points(m_offset, m_length);
    }

    inline StringRef strip_code_points(index_t cp_left, index_t cp_right) const {
        const size_t shift = m_extent->walk_code_points(m_offset, cp_left);
        return String::construct(
            m_extent,
            m_offset + shift,
            m_length - shift - m_extent->walk_code_points(m_offset + m_length, -cp_right));
    }

    inline SymbolRef option_symbol(const Evaluation &evaluation) const;

	virtual bool is_numeric() const {
		return false;
	}
};

inline const String *BaseExpression::as_string() const {
	return static_cast<const String*>(this);
}

inline BaseExpressionRef from_primitive(const std::string &value) {
    return BaseExpressionRef(String::construct(std::string(value)));
}

template<typename Extent>
class CharacterSequence {
protected:
	MatchContext &m_context;
    Extent * const m_extent;
    const index_t m_offset;
    const index_t m_length;

public:
    class Element {
    private:
        const CharacterSequence<Extent> * const m_sequence;
        const index_t m_begin;
        UnsafeBaseExpressionRef m_cached;

    public:
        inline Element(const CharacterSequence<Extent> *sequence, index_t begin) :
            m_sequence(sequence), m_begin(begin) {
        }

	    inline index_t begin() const {
		    return m_begin;
	    }

        inline const BaseExpressionRef &operator*() {
            if (!m_cached) {
                m_cached = String::construct(
                    StringExtentRef(m_sequence->m_extent),
                    m_sequence->m_offset + m_begin,
                    1);
            }
            return m_cached;
        }
    };

    class Sequence {
    private:
        const CharacterSequence<Extent> * const m_sequence;
        const index_t m_begin;
        const index_t m_end;
	    UnsafeBaseExpressionRef m_cached;

    public:
        inline Sequence(const CharacterSequence<Extent> *sequence, index_t begin, index_t end) :
            m_sequence(sequence), m_begin(begin), m_end(end) {
        }

        inline const BaseExpressionRef &operator*() {
            if (!m_cached) {
                m_cached = String::construct(
                    StringExtentRef(m_sequence->m_extent),
                    m_sequence->m_offset + m_begin,
                    m_end - m_begin);
            }
            return m_cached;
        }
    };

    inline CharacterSequence(MatchContext &context, const String *string) :
	    m_context(context),
        m_extent(static_cast<Extent*>(string->extent().get())),
        m_offset(string->to_extent_offset(0)),
        m_length(string->length()) {

        assert(string->extent_type() == Extent::extent_type);
    }

	inline MatchContext &context() const {
		return m_context;
	}

    inline Element element(index_t begin) const {
        return Element(this, begin);
    }

    inline Sequence slice(index_t begin, index_t end) const {
        assert(begin <= end);
        return Sequence(this, begin, end);
    }

    inline index_t same(index_t begin, BaseExpressionPtr other) const {
        assert(other->is_string());
        const String * const other_string = other->as_string();
        const index_t n = other_string->length();
        if (n > m_length - begin) {
            return -1;
        }
        if (m_extent->same_n(other_string->extent().get(), m_offset + begin, other_string->to_extent_offset(0), n)) {
            return begin + n;
        } else {
            return -1;
        }
    }

    template<typename F>
    inline bool all_code_points(size_t offset, const F &f) const {
        return m_extent->all_code_points(m_offset + offset, f);
    }

	inline bool is_word_boundary(size_t offset) const {
		return m_extent->is_word_boundary(m_offset, m_offset + m_length, m_offset + offset);
	}
};

class AsciiCharacterSequence : public CharacterSequence<AsciiStringExtent> {
public:
    inline AsciiCharacterSequence(MatchContext &context, const String *string) :
        CharacterSequence<AsciiStringExtent>(context, string) {
    }
};

class SimpleCharacterSequence : public CharacterSequence<SimpleStringExtent> {
public:
    inline SimpleCharacterSequence(MatchContext &context, const String *string) :
        CharacterSequence<SimpleStringExtent>(context, string) {
    }
};

class ComplexCharacterSequence : public CharacterSequence<ComplexStringExtent> {
public:
    inline ComplexCharacterSequence(MatchContext &context, const String *string) :
        CharacterSequence<ComplexStringExtent>(context, string) {
    }
};

template<typename StringArray>
StringRef string_array_join(const StringArray &array) {
    const size_t n = array.size();

    StringExtent::Type extent_type = StringExtent::ascii;
    size_t number_of_code_points = 0;

    for (const auto &leaf : array) {
        if (!leaf->is_string()) {
            return StringRef();
        }
        extent_type = std::max(extent_type,
            leaf->as_string()->extent_type());
        number_of_code_points +=
            leaf->as_string()->number_of_code_points();
    }

    if (extent_type == StringExtent::ascii) {
        std::string text;
        text.reserve(number_of_code_points);
        for (const auto &leaf : array) {
            const String *s = leaf->as_string();
            const char *ascii = s->ascii();
            assert(ascii);
            text.append(ascii, s->length());
        }
        return String::construct(AsciiStringExtent::construct(std::move(text)));
    } else {
        UnicodeString text(number_of_code_points, 0, 0);
        for (const auto &leaf : array) {
            text.append(leaf->as_string()->unicode());
        }
        if (extent_type == StringExtent::simple) {
            return String::construct(SimpleStringExtent::construct(text));
        } else {
            return String::construct(ComplexStringExtent::construct(text));
        }
    }
}

template<typename... String>
inline StringRef string_join(const String&... rest) {
    const std::initializer_list<StringRef> strings = {rest...};
    return string_array_join(strings);
}

#endif
