#ifndef STRING_H
#define STRING_H

#include <string>

#include "types.h"
#include "hash.h"

class StringExtent {
private:
    utf8proc_uint8_t *m_utf8_data; // always NFC
    size_t m_utf8_size;
    index_t m_n_code_points;
    std::vector<utf8proc_ssize_t> m_code_points; // optional

public:
    inline StringExtent(const unicode_t *utf8) {
        m_utf8_data = utf8proc_NFC(utf8);
        m_utf8_size = strlen(reinterpret_cast<const char*>(m_utf8_data));
        m_n_code_points = -1;
    }

    inline ~StringExtent() {
        if (m_utf8_data) {
            free(m_utf8_data);
        }
    }

    inline const utf8proc_uint8_t *data() const { // utf8, NFC
        return m_utf8_data;
    }

    inline size_t size() const { // utf8, NFC
        return m_utf8_size;
    }

    inline const std::vector<utf8proc_ssize_t> &code_points() {
        if (m_n_code_points < 0) {
            const utf8proc_uint8_t * const begin = m_utf8_data;
            const utf8proc_uint8_t * const end = begin + m_utf8_size;

            const utf8proc_uint8_t *p = begin;

            m_code_points.push_back(0);
            while (p < end) {
                utf8proc_int32_t codepoint;
                const utf8proc_ssize_t n = utf8proc_iterate(p, end - p, &codepoint);
                if (n < 1) {
                    break;
                }
                p += n;
                m_code_points.push_back(p - begin);
            }

            m_n_code_points = m_code_points.size() - 1;
        }

        return m_code_points;
    }

    inline size_t n_code_points() {
        code_points();
        return m_n_code_points;
    }
};

typedef std::shared_ptr<StringExtent> StringExtentRef;

class String : public BaseExpression {
private:
	mutable optional<SymbolRef> m_option_symbol; // "System`" + value

    StringExtentRef m_extent;
    size_t m_offset; // into m_extent's m_utf8_data
    size_t m_size; // wrt m_utf8_data's m_utf8_data
    size_t m_cp_offset;

protected:
    friend class CodePointPtr;

    const StringExtentRef &extent() const {
        return m_extent;
    }

    const utf8proc_ssize_t *code_points_address() const {
        const auto &cp = m_extent->code_points();
        return &cp[m_cp_offset];
    }

public:
    explicit String(const std::string &utf8) : BaseExpression(StringExtendedType) {
        m_extent = std::make_shared<StringExtent>(
            reinterpret_cast<const utf8proc_uint8_t*>(utf8.c_str()));
        m_offset = 0;
        m_size = m_extent->size();
        m_cp_offset = 0;
    }

    explicit String(const utf8proc_uint8_t *utf8) : BaseExpression(StringExtendedType) {
        m_extent = std::make_shared<StringExtent>(utf8);
        m_offset = 0;
        m_size = m_extent->size();
        m_cp_offset = 0;
    }

    explicit String(const String *s, size_t cp_index, size_t cp_size) : BaseExpression(StringExtendedType) {
        m_extent = s->m_extent;
        m_cp_offset = s->m_cp_offset + cp_index;
        const size_t cp_index_offset = s->code_point(cp_index);
        m_offset = s->m_offset + cp_index_offset;
        m_size = s->code_point(cp_index + cp_size) - cp_index_offset;
    }

    inline const utf8proc_uint8_t *data() const { // utf8, NFC
        return m_extent->data() + m_offset;
    }

    inline size_t size() const { // utf8, NFC
        return m_size;
    }

    inline size_t code_point(size_t index) const {
        const auto &cp = m_extent->code_points();
        assert(m_cp_offset + index < cp.size());
        return cp[m_cp_offset + index] - m_offset;
    }

	inline SymbolRef option_symbol(const Evaluation &evaluation) const;

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == StringType) {
            const String * const other = static_cast<const String*>(&expr);
            const size_t n = m_size;

            if (n != other->m_size) {
                return false;
            }

            return std::memcmp(data(), other->data(), n) == 0;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        const utf8proc_uint8_t *s = data();
        const size_t n = m_size;

        hash_t result = 5381;

        for (size_t i = 0; i < n; i++) {
            result = ((result << 5) + result) + s[i];
        }

        return hash_pair(string_hash, result);
    }

    inline std::string str() const { // utf8, NFC
        return std::string(reinterpret_cast<const char*>(data()), size());
    }

    virtual std::string fullform() const {
        return str();
    }

    /*inline const std::string &str() const {
        return value;
    }

    inline const char *c_str() const {
        return value.c_str();
    }*/

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    inline size_t length() const {
        return m_extent->n_code_points() - m_cp_offset;
    }

    String take(index_t n) const;
};

inline BaseExpressionRef from_primitive(const std::string &value) {
    return BaseExpressionRef(new String(value));
}

class CodePointPtr {
private:
    const String *m_string;
    const utf8proc_uint8_t * const m_utf8_begin;
    const utf8proc_ssize_t * const m_code_points;
    size_t m_cp_offset;

public:
    inline CodePointPtr(const String *string) :
        m_string(string),
        m_utf8_begin(string->extent()->data()),
        m_code_points(string->code_points_address()),
        m_cp_offset(0) {
    }

    inline CodePointPtr(
        const String *string,
        const utf8proc_uint8_t *begin,
        const utf8proc_ssize_t *code_points,
        size_t cp_offset) :

        m_string(string),
        m_utf8_begin(begin),
        m_code_points(code_points),
        m_cp_offset(cp_offset) {
    }

    inline CodePointPtr(std::nullptr_t) :
        m_string(nullptr),
        m_utf8_begin(nullptr),
        m_code_points(nullptr),
        m_cp_offset(0)  {
    }

    inline const unicode_t *data() const {
        return m_utf8_begin + m_code_points[m_cp_offset];
    }

    inline codepoint_t operator*() const {
        const size_t offset = m_code_points[m_cp_offset];
        utf8proc_int32_t codepoint;
        utf8proc_iterate(
            m_utf8_begin + offset,
            m_code_points[m_cp_offset + 1] - offset,
            &codepoint);
        return codepoint;
    }

    inline codepoint_t operator[](size_t i) const {
        const size_t offset = m_code_points[m_cp_offset + i];
        utf8proc_int32_t codepoint;
        utf8proc_iterate(
            m_utf8_begin + m_code_points[m_cp_offset + i],
            m_code_points[m_cp_offset + i + 1] - offset,
            &codepoint);
        return codepoint;
    }

    inline bool operator==(const CodePointPtr &ptr) const {
        assert(m_utf8_begin == ptr.m_utf8_begin);
        return m_cp_offset == ptr.m_cp_offset;
    }

    inline bool operator<(const CodePointPtr &ptr) const {
        assert(m_utf8_begin == ptr.m_utf8_begin);
        return m_cp_offset < ptr.m_cp_offset;
    }

    inline CodePointPtr operator+(int i) const {
        return CodePointPtr(m_string, m_utf8_begin, m_code_points, m_cp_offset + i);
    }

    inline CodePointPtr operator+(index_t i) const {
        return CodePointPtr(m_string, m_utf8_begin, m_code_points, m_cp_offset + i);
    }

    inline CodePointPtr operator+(size_t i) const {
        return CodePointPtr(m_string, m_utf8_begin, m_code_points, m_cp_offset + i);
    }

    inline CodePointPtr operator++(int) {
        const CodePointPtr old = CodePointPtr(m_string, m_utf8_begin, m_code_points, m_cp_offset);
        m_cp_offset += 1;
        return old;
    }

    inline index_t operator-(const CodePointPtr& ptr) const {
        assert(m_code_points == ptr.m_code_points);
        return m_cp_offset - ptr.m_cp_offset;
    }

    inline operator bool() const {
        return m_utf8_begin;
    }

    inline BaseExpressionRef slice(size_t n) const {
        return Heap::String(m_string, m_cp_offset, n);
    }
};

#endif
