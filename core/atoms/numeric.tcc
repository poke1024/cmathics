namespace  Numeric {

inline BaseExpressionRef Z::to_expression() const {
    if (is_big) {
        return BigInteger::construct(std::move(mpz_class(big_value)));
    } else {
        return MachineInteger::construct(machine_integer_t(machine_value));
    }
}

} // namespace Numeric
