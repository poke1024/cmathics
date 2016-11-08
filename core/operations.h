#ifndef CMATHICS_OPERATIONS_H
#define CMATHICS_OPERATIONS_H

template<typename T>
class OperationsImplementation {
private:
    const T * const _expr;

protected:
    template<typename V>
    inline auto primitives() const {
        return _expr->_leaves.template primitives<V>();
    }

    inline auto leaves() const {
        return _expr->_leaves.leaves();
    }

    inline auto head() const {
        return _expr->head();
    }

    inline auto expr() const {
        return *_expr;
    }

public:
    inline OperationsImplementation() : _expr(nullptr) {
    }

    inline OperationsImplementation(const T &expr) : _expr(&expr) {
    }
};

#endif //CMATHICS_OPERATIONS_H
