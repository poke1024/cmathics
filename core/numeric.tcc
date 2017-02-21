namespace  Numeric {

inline BaseExpressionRef Z::to_expression() const {
    if (is_big) {
        return Pool::BigInteger(std::move(mpz_class(big_value)));
    } else {
        return Pool::MachineInteger(machine_integer_t(machine_value));
    }
}

} // namespace Numeric
