#pragma once

#include "primitives.h"

class PassBaseExpression {
public:
    inline BaseExpressionRef convert(const BaseExpressionRef &u) const {
        return u;
    }
};

template<typename T>
class BaseExpressionToPrimitive {
public:
    inline T convert(const BaseExpressionRef &u) const {
        return to_primitive<T>(u);
    }
};

template<typename T>
class PrimitiveToBaseExpression {
public:
    inline BaseExpressionRef convert(const T &u) const {
        return from_primitive(u);
    }
};

template<typename V>
class PromotePrimitive {
public:
    template<typename U>
    inline V convert(const U &x) const {
        return V(x);
    }
};

template<>
class PromotePrimitive<machine_real_t> {
public:
    template<typename U>
    inline machine_real_t convert(const U &x) const {
        return machine_real_t(x);
    }

    inline machine_real_t convert(const std::string &x) const {
        throw std::runtime_error("illegal promotion");
    }

    inline machine_real_t convert(const mpz_class &x) const {
        throw std::runtime_error("illegal promotion");
    }

    inline machine_real_t convert(const mpq_class &x) const {
        throw std::runtime_error("illegal promotion");
    }
};

template<>
class PromotePrimitive<Numeric::Z> {
public:
    template<typename U>
    inline Numeric::Z convert(const U &x) const {
        return Numeric::Z(x);
    }

    inline Numeric::Z convert(const std::string &x) const {
        throw std::runtime_error("illegal promotion");
    }

    inline Numeric::Z convert(const mpq_class &x) const {
        throw std::runtime_error("illegal promotion");
    }
};

template<typename T, typename TypeConverter>
class PointerIterator {
private:
    const T *_ptr;
    const TypeConverter _converter;

public:
    inline PointerIterator(const TypeConverter &converter) : _converter(converter) {
    }

    inline explicit PointerIterator(const TypeConverter &converter, const T *ptr) :
            _converter(converter), _ptr(ptr) {
    }

    inline auto operator*() const {
        return _converter.convert(*_ptr);
    }

    inline bool operator==(const PointerIterator<T, TypeConverter> &other) const {
        return _ptr == other._ptr;
    }

    inline bool operator!=(const PointerIterator<T, TypeConverter> &other) const {
        return _ptr != other._ptr;
    }

    inline PointerIterator<T, TypeConverter> &operator++() {
        _ptr += 1;
        return *this;
    }

    inline PointerIterator<T, TypeConverter> operator++(int) const {
        return PointerIterator<T, TypeConverter>(_ptr + 1);
    }
};

template<typename T, typename TypeConverter = PassBaseExpression>
class PointerCollection {
private:
    const TypeConverter _converter;
    const T * const _data;
    const size_t _size;

public:
    using Iterator = PointerIterator<T, TypeConverter>;

    inline PointerCollection(const T *data, size_t size, const TypeConverter &converter = TypeConverter()) :
        _converter(converter), _data(data), _size(size) {
    }

    inline Iterator begin() const {
        return PointerIterator<T, TypeConverter>(_converter, _data);
    }

    inline Iterator end() const {
        return PointerIterator<T, TypeConverter>(_converter, _data + _size);
    }

    inline auto operator[](size_t i) const {
        return *Iterator(_converter, _data + i);
    }
};

template<int N, typename T, typename TypeConverter = PassBaseExpression>
class FixedSizePointerCollection {
private:
    const TypeConverter _converter;
    const T * const _data;

public:
    using Iterator = PointerIterator<T, TypeConverter>;

    inline FixedSizePointerCollection(const T *data, const TypeConverter &converter = TypeConverter()) :
        _converter(converter), _data(data) {
    }

    inline Iterator begin() const {
        return PointerIterator<T, TypeConverter>(_converter, _data);
    }

    inline Iterator end() const {
        return PointerIterator<T, TypeConverter>(_converter, _data + N);
    }

    inline auto operator[](size_t i) const {
        return *Iterator(_converter, _data + i);
    }
};
