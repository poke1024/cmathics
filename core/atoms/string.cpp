#include "core/types.h"
#include "core/expression/implementation.h"
#include "string.h"

#include "unicode/brkiter.h"
#include "unicode/normalizer2.h"
#include "unicode/errorcode.h"

std::string String::debugform() const {
	return std::string("\"") + utf8() + std::string("\"");
}

BaseExpressionRef String::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    std::ostringstream s;
    s << "\"";
    s << utf8();
    s << "\"";
    return String::construct(s.str());
}

std::string String::boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
    if (options.ShowStringCharacters) {
        return utf8();
    } else {
        return unquoted_utf8();
    }
}

BaseExpressionPtr String::head(const Symbols &symbols) const {
	return symbols.String;
}

std::string String::format(const SymbolRef &form, const Evaluation &evaluation) const {
    return utf8();
}

void String::sort_key(SortKey &key, const Evaluation &evaluation) const {
	return key.construct(0, 1, BaseExpressionPtr(this), 0, 1);
}

bool is_simple_encoding(BreakIterator *bi) {
    int32_t last_p = -1;
    int32_t p = bi->first();
    while (p != BreakIterator::DONE) {
        if (p != last_p + 1) {
            return false;
        }
        last_p = p;
        p = bi->next();
    }
    return true;
}

std::vector<int32_t> character_offsets(BreakIterator *bi, size_t capacity) {
    std::vector<int32_t> offsets;
    offsets.reserve(capacity);
    int32_t p = bi->first();
    while (p != BreakIterator::DONE) {
        offsets.push_back(p);
        p = bi->next();
    }
    return offsets;
}

void check_status(const UErrorCode &status, const char *err) {
	if (U_FAILURE(status)) {
		icu::ErrorCode code;
		code.set(status);
		std::ostringstream s;
		s << err << ": " << code.errorName();
		throw std::runtime_error(s.str());
	}
}

StringExtentRef string_extent_from_normalized(UnicodeString &&normalized, uint8_t possible_types) {
	if (possible_types & (1 << StringExtent::ascii)) {
		const size_t size = normalized.length();
		const UChar *buffer = normalized.getBuffer();

		bool is_ascii = true;

		if (possible_types != (1 << StringExtent::ascii)) {
			for (size_t i = 0; i < size; i++) {
				if (buffer[i] >> 7) {
					is_ascii = false;
					break;
				}
			}
		}

		if (is_ascii) {
			std::string ascii;
			ascii.reserve(size);

			for (size_t i = 0; i < size; i++) {
				ascii.append(1, char(buffer[i]));
			}

			return AsciiStringExtent::construct(std::move(ascii));
		}
	}

	if ((possible_types & (1 << StringExtent::complex)) == 0) {
		return SimpleStringExtent::construct(normalized);
	}

    UErrorCode status = U_ZERO_ERROR;

	std::unique_ptr<BreakIterator> bi(
		BreakIterator::createCharacterInstance(Locale::getUS(), status));
	check_status(status, "BreakIterator::createCharacterInstance failed");

	bi->setText(normalized);

	if (is_simple_encoding(bi.get())) {
		return SimpleStringExtent::construct(normalized);
	} else {
		std::vector<int32_t> offsets = character_offsets(bi.get(), normalized.length());
		return ComplexStringExtent::construct(normalized, offsets);
	}
}

StringExtentRef make_string_extent(std::string &&utf8) {
    bool is_ascii = true;
	const size_t size = utf8.size();

    for (size_t i = 0; i < size; i++) {
        if (utf8[i] >> 7) {
			is_ascii = false;
            break;
        }
    }

    if (is_ascii) {
        return AsciiStringExtent::construct(std::move(utf8));
    }

    UErrorCode status = U_ZERO_ERROR;

    const Normalizer2 *norm = Normalizer2::getNFCInstance(status);
	check_status(status, "Normalizer2::getNFCInstance failed");

    UnicodeString normalized = norm->normalize(
        UnicodeString::fromUTF8(StringPiece(utf8)), status);
	check_status(status, "Normalizer2::normalize failed");

	return string_extent_from_normalized(std::move(normalized),
		 (1 << StringExtent::simple) | (1 << StringExtent::complex));
}

std::vector<int32_t> make_character_offsets(const UnicodeString &normalized) {
	UErrorCode status = U_ZERO_ERROR;

	std::unique_ptr<BreakIterator> bi(
		BreakIterator::createCharacterInstance(Locale::getUS(), status));
	check_status(status, "BreakIterator::createCharacterInstance failed");

	bi->setText(normalized);

	return character_offsets(bi.get(), normalized.length());
}

hash_t StringExtent::hash(size_t offset, size_t length) const {
    const std::string s(utf8(offset, length));
    return djb2(s.c_str());
}

struct eq_ascii_ascii {
    template<typename Eq>
    static inline bool equals(
        const Eq &eq,
        const char *x,
        const char *y,
        size_t n) {

        for (size_t i = 0; i < n; i++) {
            if (!eq(x[i], y[i])) {
                return false;
            }
        }

        return true;
    }
};

struct eq_ascii_simple {
    template<typename Eq>
    static inline bool equals(
        const Eq &eq,
        const AsciiStringExtent *y,
        size_t iy,
        const SimpleStringExtent *x,
        size_t ix,
        size_t n) {

        const char *const ascii = y->data() + iy;
        const auto simple = x->unicode();

        for (size_t i = 0; i < n; i++) {
            if (!eq(simple.charAt(ix + i), ascii[i])) {
                return false;
            }
        }

        return true;
    }
};

struct eq_ascii_complex {
    template<typename Eq>
    static inline bool equals(
        const Eq &eq,
        const AsciiStringExtent *y,
        size_t iy,
        const ComplexStringExtent *x,
        size_t ix,
        size_t n) {

        const char *const ascii = y->data() + iy;
        const auto complex = x->unicode();

        const size_t cp_ix = x->offsets()[ix];
        const size_t cp_n = x->offsets()[ix + n] - cp_ix;

        if (cp_n != n) {
            return false;
        }

        for (size_t i = 0; i < n; i++) {
            if (!eq(complex.charAt(cp_ix + i), ascii[i])) {
                return false;
            }
        }
        return true;
    }
};

template<typename Func, typename... Args>
bool eq_string(bool ignore_case, Args... args) {
    if (!ignore_case) {
        const auto eq = [] (auto c1, auto c2) {
            return c1 == c2;
        };
        return Func::equals(eq, args...);
    } else {
        const auto eq = [] (UChar c1, UChar c2) {
            UErrorCode error;
            return u_strCaseCompare(&c1, 1, &c2, 1, 0, &error) == 0;
        };
        return Func::equals(eq, args...);
    }
}

inline int compare_unicode(
    const UnicodeString &a,
    int32_t a_start,
    int32_t a_length,
    const UnicodeString& b,
    int32_t b_start,
    int32_t b_length,
    bool ignore_case) {

    if (!ignore_case) {
        return a.compare(a_start, a_length, b, b_start, b_length);
    } else {
        return a.caseCompare(a_start, a_length, b, b_start, b_length, 0);
    }
}

AsciiStringExtent::~AsciiStringExtent() {
}

UnicodeString AsciiStringExtent::unicode() const { // concurrent
    const UnicodeStringRef string = std::atomic_load(&m_string);
    if (string) {
        return *string;
    } else {
        const UnicodeStringRef new_string = std::make_shared<UnicodeString>(
            UnicodeString::fromUTF8(StringPiece(m_ascii)));

        std::atomic_store(&m_string, new_string);
        return *new_string;
    }
}

size_t AsciiStringExtent::length() const {
    return m_ascii.size();
}

char AsciiStringExtent::ascii_char_at(size_t offset) const {
	return m_ascii.data()[offset];
}

size_t AsciiStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return length;
}

std::string AsciiStringExtent::utf8(size_t offset, size_t length) const {
    return std::string(m_ascii.data() + offset, length);
}

UnicodeString AsciiStringExtent::unicode(size_t offset, size_t length) const {
	return UnicodeString(unicode(), offset, length);
}

bool AsciiStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n, bool ignore_case) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return eq_string<eq_ascii_ascii>(ignore_case, m_ascii.data() + offset, ascii_extent->m_ascii.data() + extent_offset, n);
        }

        case StringExtent::simple: {
            const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
            return eq_string<eq_ascii_simple>(ignore_case, this, offset, simple_extent, extent_offset, n);
        }

	    case StringExtent::complex: {
		    const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
		    return eq_string<eq_ascii_complex>(ignore_case, this, offset, complex_extent, extent_offset, n);
	    }

	    default:
            throw std::runtime_error("unsupported string extent type");
    }
}

StringExtentRef AsciiStringExtent::repeat(size_t offset, size_t length, size_t n) const {
	std::string s;
	const std::string part = m_ascii.substr(offset, length);
	s.reserve(part.size() * n);
	for (size_t i = 0; i < n; i++) {
		s += part;
	}
	return AsciiStringExtent::construct(std::move(s));
}

size_t AsciiStringExtent::walk_code_points(size_t offset, index_t cp_offset) const {
    return std::abs(cp_offset);
}


std::string SimpleStringExtent::utf8(size_t offset, size_t length) const {
    std::string s;
    m_string.tempSubString(offset, length).toUTF8String(s);
    return s;
}

UnicodeString SimpleStringExtent::unicode(size_t offset, size_t length) const {
	return UnicodeString(m_string, offset, length);
}

size_t SimpleStringExtent::length() const {
    return m_string.length();
}

char SimpleStringExtent::ascii_char_at(size_t offset) const {
	const auto c = m_string.char32At(offset);
	return (c >= 0 && c < 128) ? (char)c : -1;
}

size_t SimpleStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return length;
}

bool SimpleStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n, bool ignore_case) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return eq_string<eq_ascii_simple>(ignore_case, ascii_extent, extent_offset, this, offset, n);
        }

        case StringExtent::simple: {
            const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
            return compare_unicode(m_string, offset, n, simple_extent->unicode(), extent_offset, n, ignore_case) == 0;
        }

	    case StringExtent::complex: {
		    const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
		    const size_t cp_offset = complex_extent->offsets()[extent_offset];
		    const size_t cp_size = complex_extent->offsets()[extent_offset + n] - cp_offset;
		    if (cp_size != n) {
			    return false;
		    }
            return compare_unicode(m_string, offset, n, complex_extent->unicode(), cp_offset, cp_size, ignore_case) == 0;
	    }

        default:
            throw std::runtime_error("unsupported string extent type");
    }
}

StringExtentRef SimpleStringExtent::repeat(size_t offset, size_t length, size_t n) const {
	UnicodeString text(n * m_string.length(), 0, 0);
	const UChar *begin = m_string.getBuffer() + offset;

	for (size_t i = 0; i < n; i++) {
		text.append(begin, length);
	}

	return SimpleStringExtent::construct(text);
}

size_t SimpleStringExtent::walk_code_points(size_t offset, index_t cp_offset) const {
    return std::abs(cp_offset);
}

std::string ComplexStringExtent::utf8(size_t offset, size_t length) const {
	const int32_t cp_offset = m_offsets[offset];
	const int32_t cp_length = m_offsets[offset + length] - cp_offset;
	std::string s;
	m_string.tempSubString(cp_offset, cp_length).toUTF8String(s);
	return s;
}

UnicodeString ComplexStringExtent::unicode(size_t offset, size_t length) const {
	const size_t cp_offset = m_offsets[offset];
	return UnicodeString(m_string, cp_offset, m_offsets[offset + length] - cp_offset);
}

size_t ComplexStringExtent::length() const {
	return m_offsets.size() - 1;
}

char ComplexStringExtent::ascii_char_at(size_t offset) const {
	if (m_offsets[offset + 1] - m_offsets[offset] != 1) {
		return -1;
	}
	const auto c = m_string.char32At(m_offsets[offset]);
	return (c >= 0 && c < 128) ? (char)c : -1;
}

size_t ComplexStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return m_offsets[offset + length] - m_offsets[offset];
}

bool ComplexStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n, bool ignore_case) const {
	assert(offset + n < m_offsets.size());

	switch (extent->type()) {
		case StringExtent::ascii: {
			const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
			return eq_string<eq_ascii_complex>(ignore_case, ascii_extent, extent_offset, this, offset, n);
		}

		case StringExtent::simple: {
			const size_t cp_offset = m_offsets[offset];
			const size_t cp_size = m_offsets[offset + n] - cp_offset;
			if (cp_size != n) {
				return false;
			}
			const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
			return compare_unicode(
                m_string, cp_offset, cp_size, simple_extent->unicode(), extent_offset, n, ignore_case) == 0;
		}

		case StringExtent::complex: {
			const size_t cp_offset = m_offsets[offset];
			const size_t cp_size = m_offsets[offset + n] - cp_offset;
			const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
			const size_t extent_cp_offset = complex_extent->m_offsets[extent_offset];
			const size_t extent_cp_size = complex_extent->m_offsets[extent_offset + n] - extent_cp_offset;
			if (cp_size != extent_cp_size) {
				return false;
			}
			return compare_unicode(
				m_string, cp_offset, cp_size, complex_extent->unicode(), extent_cp_offset, extent_cp_size, ignore_case) == 0;
		}

		default:
			throw std::runtime_error("unsupported string extent type");
	}
}

StringExtentRef ComplexStringExtent::repeat(size_t offset, size_t length, size_t n) const {
	UnicodeString text(n * m_string.length(), 0, 0);

	const UChar *begin = m_string.getBuffer() + m_offsets[offset];
	const size_t size = m_offsets[offset + length] - m_offsets[offset];

	for (size_t i = 0; i < n; i++) {
		text.append(begin, size);
	}

	return ComplexStringExtent::construct(text);
}

size_t ComplexStringExtent::walk_code_points(size_t offset, index_t cp_offset) const {
    throw std::runtime_error("not implemented");
}
