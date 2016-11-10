#ifndef CMATHICS_OPERATIONS_H
#define CMATHICS_OPERATIONS_H

template<typename T>
class OperationsImplementation {
private:
    const T * const _expr;

protected:
    inline auto expr() const {
        return *_expr;
    }

public:
    inline OperationsImplementation() : _expr(nullptr) {
        throw std::runtime_error("check virtual base call instantiation in ExpressionImplementation");
    }

    inline OperationsImplementation(const T *expr) : _expr(expr) {
    }
};

#endif //CMATHICS_OPERATIONS_H
