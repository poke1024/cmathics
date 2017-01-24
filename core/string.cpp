#include "types.h"
#include "expression.h"
#include "string.h"

#include "unicode/brkiter.h"
#include "unicode/normalizer2.h"
#include "unicode/errorcode.h"

BaseExpressionPtr String::head(const Evaluation &evaluation) const {
	return evaluation.String;
}

std::string String::format(const SymbolRef &form, const Evaluation &evaluation) const {
    return utf8();
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

			return new AsciiStringExtent(std::move(ascii));
		}
	}

	if ((possible_types & (1 << StringExtent::complex)) == 0) {
		return new SimpleStringExtent(normalized);
	}

    UErrorCode status = U_ZERO_ERROR;

	std::unique_ptr<BreakIterator> bi(
		BreakIterator::createCharacterInstance(Locale::getUS(), status));
	check_status(status, "BreakIterator::createCharacterInstance failed");

	bi->setText(normalized);

	if (is_simple_encoding(bi.get())) {
		return new SimpleStringExtent(normalized);
	} else {
		std::vector<int32_t> offsets = character_offsets(bi.get(), normalized.length());
		return new ComplexStringExtent(normalized, offsets);
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
        return new AsciiStringExtent(std::move(utf8));
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

bool eq_ascii_simple(const AsciiStringExtent *y, size_t iy, const SimpleStringExtent *x, size_t ix, size_t n) {
    const char * const ascii = y->data() + iy;
    const auto simple = x->unicode();

    for (size_t i = 0; i < n; i++) {
        if (simple.charAt(ix + i) != ascii[i]) {
            return false;
        }
    }

	return true;
}

bool eq_ascii_complex(const AsciiStringExtent *y, size_t iy, const ComplexStringExtent *x, size_t ix, size_t n) {
	const char * const ascii = y->data() + iy;
	const auto complex = x->unicode();

	const size_t cp_ix = x->offsets()[ix];
	const size_t cp_n = x->offsets()[ix + n] - cp_ix;

	if (cp_n != n) {
		return false;
	}

	for (size_t i = 0; i < n; i++) {
		if (complex.charAt(cp_ix + i) != ascii[i]) {
			return false;
		}
	}
	return true;

}

AsciiStringExtent::~AsciiStringExtent() {
}

size_t AsciiStringExtent::length() const {
    return m_ascii.size();
}

size_t AsciiStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return length;
}

std::string AsciiStringExtent::utf8(size_t offset, size_t length) const {
    return std::string(m_ascii.data() + offset, length);
}

UnicodeString AsciiStringExtent::unicode(size_t offset, size_t length) const {
	return unicode().tempSubString(offset, length);
}

bool AsciiStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return std::memcmp(m_ascii.data() + offset, ascii_extent->m_ascii.data() + extent_offset, n) == 0;
        }

        case StringExtent::simple: {
            const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
            return eq_ascii_simple(this, offset, simple_extent, extent_offset, n);
        }

	    case StringExtent::complex: {
		    const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
		    return eq_ascii_complex(this, offset, complex_extent, extent_offset, n);
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
	return new AsciiStringExtent(std::move(s));
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
	return unicode().tempSubString(offset, length);
}

size_t SimpleStringExtent::length() const {
    return m_string.length();
}

size_t SimpleStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return length;
}

bool SimpleStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return eq_ascii_simple(ascii_extent, extent_offset, this, offset, n);
        }

        case StringExtent::simple: {
            const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
            return m_string.compare(offset, n, simple_extent->unicode(), extent_offset, n) == 0;
        }

	    case StringExtent::complex: {
		    const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
		    const size_t cp_offset = complex_extent->offsets()[extent_offset];
		    const size_t cp_size = complex_extent->offsets()[extent_offset + n] - cp_offset;
		    if (cp_size != n) {
			    return false;
		    }
		    return m_string.compare(offset, n, complex_extent->unicode(), cp_offset, cp_size) == 0;
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

	return new SimpleStringExtent(text);
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
	return unicode().tempSubString(cp_offset, m_offsets[offset + length] - cp_offset);
}

size_t ComplexStringExtent::length() const {
	return m_offsets.size() - 1;
}

size_t ComplexStringExtent::number_of_code_points(size_t offset, size_t length) const {
	return m_offsets[offset + length] - m_offsets[offset];
}

bool ComplexStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const {
	assert(offset + n < m_offsets.size());

	switch (extent->type()) {
		case StringExtent::ascii: {
			const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
			return eq_ascii_complex(ascii_extent, extent_offset, this, offset, n);
		}

		case StringExtent::simple: {
			const size_t cp_offset = m_offsets[offset];
			const size_t cp_size = m_offsets[offset + n] - cp_offset;
			if (cp_size != n) {
				return false;
			}
			const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
			return m_string.compare(cp_offset, cp_size, simple_extent->unicode(), extent_offset, n) == 0;
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
			return m_string.compare(
				cp_offset, cp_size, complex_extent->unicode(), extent_cp_offset, extent_cp_size) == 0;
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

	return new ComplexStringExtent(text);
}

size_t ComplexStringExtent::walk_code_points(size_t offset, index_t cp_offset) const {
    throw std::runtime_error("not implemented");
}
