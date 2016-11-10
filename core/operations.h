#ifndef CMATHICS_OPERATIONS_H
#define CMATHICS_OPERATIONS_H

template<typename T>
class OperationsImplementation {
protected:
    inline const T &expr() const {
        #pragma clang diagnostic ignored "-Wreinterpret-base-class"
        return *reinterpret_cast<const T*>(this);
    }
};

#endif //CMATHICS_OPERATIONS_H
