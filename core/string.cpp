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
    if (U_FAILURE(status)) {
        icu::ErrorCode code;
        code.set(status);
        std::cerr << "Normalizer2::getNFCInstance failed: " << code.errorName() << std::endl;
        throw std::runtime_error("could not create UNormalizer2");
    }

    const UnicodeString normalized = norm->normalize(
        UnicodeString::fromUTF8(StringPiece(
            reinterpret_cast<const char*>(utf8), size)), status);
    if (U_FAILURE(status)) {
        throw std::runtime_error("could not normalize string");
    }

    std::unique_ptr<BreakIterator> bi(
        BreakIterator::createCharacterInstance(Locale::getUS(), status));
    if (U_FAILURE(status)) {
        throw std::runtime_error("could not create BreakIterator");
    }

    bi->setText(normalized);

    if (is_simple_encoding(bi.get())) {
        return std::make_shared<SimpleStringExtent>(normalized);
    } else {
        std::vector<int32_t> offsets = character_offsets(bi.get(), size);
        throw std::runtime_error("not implemented yet");
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
            return m_string.compare(offset, n, simple_extent->m_string, extent_offset, n) == 0;
        }
        default:
            throw std::runtime_error("unsupported string extent type");
    }

}
