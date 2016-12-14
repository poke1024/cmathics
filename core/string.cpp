#include "types.h"
#include "expression.h"
#include "string.h"

#include "unicode/brkiter.h"
#include "unicode/normalizer2.h"
#include "unicode/errorcode.h"

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

std::vector<int32_t> character_offsets(BreakIterator *bi, size_t size) {
    std::vector<int32_t> offsets;
    offsets.reserve(size);
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

StringExtentRef make_string_extent(const uint8_t *utf8, size_t size) {
    bool ascii = true;

    for (size_t i = 0; i < size; i++) {
        if (utf8[i] >> 7) {
            ascii = false;
            break;
        }
    }

    if (ascii) {
        return std::make_shared<AsciiStringExtent>(utf8, size);
    }

    UErrorCode status = U_ZERO_ERROR;

    const Normalizer2 *norm = Normalizer2::getNFCInstance(status);
	check_status(status, "Normalizer2::getNFCInstance failed");

    const UnicodeString normalized = norm->normalize(
        UnicodeString::fromUTF8(StringPiece(
            reinterpret_cast<const char*>(utf8), size)), status);
	check_status(status, "Normalizer2::normalize failed");

    std::unique_ptr<BreakIterator> bi(
        BreakIterator::createCharacterInstance(Locale::getUS(), status));
	check_status(status, "BreakIterator::createCharacterInstance failed");

    bi->setText(normalized);

    if (is_simple_encoding(bi.get())) {
        return std::make_shared<SimpleStringExtent>(normalized);
    } else {
        std::vector<int32_t> offsets = character_offsets(bi.get(), size);
	    return std::make_shared<ComplexStringExtent>(normalized, offsets);
    }
}

hash_t StringExtent::hash(size_t offset, size_t length) const {
    const std::string s(utf8(offset, length));
    return djb2(s.c_str());
}

bool eq_ascii_simple(const AsciiStringExtent *y, size_t iy, const SimpleStringExtent *x, size_t ix, size_t n) {
    const uint8_t * const ascii = y->data() + iy;
    const UnicodeString &simple = x->string();
    for (size_t i = 0; i < n; i++) {
        if (simple.charAt(ix + i) != ascii[i]) {
            return false;
        }
    }
    return true;
}

bool eq_ascii_complex(const AsciiStringExtent *y, size_t iy, const ComplexStringExtent *x, size_t ix, size_t n) {
	const uint8_t * const ascii = y->data() + iy;
	const UnicodeString &complex = x->string();

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
    delete[] m_data;
}

size_t AsciiStringExtent::length() const {
    return m_size;
}

std::string AsciiStringExtent::utf8(size_t offset, size_t length) const {
    return std::string(reinterpret_cast<const char*>(m_data + offset), length);
}

bool AsciiStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return std::memcmp(m_data + offset, ascii_extent->m_data + extent_offset, n) == 0;
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

std::string SimpleStringExtent::utf8(size_t offset, size_t length) const {
    std::string s;
    m_string.tempSubString(offset, length).toUTF8String(s);
    return s;
}

size_t SimpleStringExtent::length() const {
    return m_string.length();
}

bool SimpleStringExtent::same_n(const StringExtent *extent, size_t offset, size_t extent_offset, size_t n) const {
    switch (extent->type()) {
        case StringExtent::ascii: {
            const AsciiStringExtent *ascii_extent = static_cast<const AsciiStringExtent*>(extent);
            return eq_ascii_simple(ascii_extent, extent_offset, this, offset, n);
        }

        case StringExtent::simple: {
            const SimpleStringExtent *simple_extent = static_cast<const SimpleStringExtent*>(extent);
            return m_string.compare(offset, n, simple_extent->string(), extent_offset, n) == 0;
        }

	    case StringExtent::complex: {
		    const ComplexStringExtent *complex_extent = static_cast<const ComplexStringExtent*>(extent);
		    const size_t cp_offset = complex_extent->offsets()[extent_offset];
		    const size_t cp_size = complex_extent->offsets()[extent_offset + n] - cp_offset;
		    if (cp_size != n) {
			    return false;
		    }
		    return m_string.compare(offset, n, complex_extent->string(), cp_offset, cp_size) == 0;
	    }

        default:
            throw std::runtime_error("unsupported string extent type");
    }
}

std::string ComplexStringExtent::utf8(size_t offset, size_t length) const {
	const int32_t cp_offset = m_offsets[offset];
	const int32_t cp_length = m_offsets[offset + length] - cp_offset;
	std::string s;
	m_string.tempSubString(cp_offset, cp_length).toUTF8String(s);
	return s;
}

size_t ComplexStringExtent::length() const {
	return m_offsets.size() - 1;
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
			return m_string.compare(cp_offset, cp_size, simple_extent->string(), extent_offset, n) == 0;
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
				cp_offset, cp_size, complex_extent->string(), extent_cp_offset, extent_cp_size) == 0;
		}

		default:
			throw std::runtime_error("unsupported string extent type");
	}

}
