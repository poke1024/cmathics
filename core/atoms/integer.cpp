#include "core/types.h"
#include "core/expression/implementation.h"
#include "integer.h"
#include "core/evaluation.h"

std::string MachineInteger::debugform() const {
	return std::to_string(value);
}

BaseExpressionRef MachineInteger::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    return String::construct(std::to_string(value));
}

std::string MachineInteger::boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
	return std::to_string(value);
}

BaseExpressionPtr MachineInteger::head(const Symbols &symbols) const {
    return symbols.Integer;
}

BaseExpressionRef MachineInteger::negate(const Evaluation &evaluation) const {
    return from_primitive(Numeric::Z(value) * Numeric::Z(-1));
}

optional<SExp> MachineInteger::to_s_exp(optional<machine_integer_t> &n) const {
    int non_negative;
    machine_integer_t exp;
    std::string s(std::to_string(value));
    if (value < 0) {
        non_negative = 0;
        // the following also works for the border case value == INT_MIN
        assert(s[0] == '-');
        s.erase(0, 1);
    } else {
        non_negative = 1;
    }
    exp = s.length() - 1;
    n = s.length();
    const StringRef str(String::construct(std::move(s)));
    return std::make_tuple(
        str,
        exp,
        non_negative,
        true);
}

void MachineInteger::sort_key(SortKey &key, const Evaluation &evaluation) const {
	key.construct(0, 0, BaseExpressionPtr(this), evaluation.zero, 1);
}

std::string BigInteger::debugform() const {
	return value.get_str();
}

BaseExpressionRef BigInteger::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    return String::construct(value.get_str());
}

BaseExpressionPtr BigInteger::head(const Symbols &symbols) const {
    return symbols.Integer;
}

std::string BigInteger::boxes_to_text(const StyleBoxOptions &options, const Evaluation &evaluation) const {
	return value.get_str();
}

BaseExpressionRef BigInteger::negate(const Evaluation &evaluation) const {
    mpz_class negated(-value);
    return from_primitive(std::move(negated));
}

optional<SExp> BigInteger::to_s_exp(optional<machine_integer_t> &n) const {
    int non_negative;
    machine_integer_t exp;
    std::string s(value.get_str());
    if (value < 0) {
        non_negative = 0;
        assert(s[0] == '-');
        s.erase(0, 1);
    } else {
        non_negative = 1;
    }
    exp = s.length() - 1;
    n = s.length();
    const StringRef str(String::construct(std::move(s)));
    return std::make_tuple(str, exp, non_negative, true);
}

void BigInteger::sort_key(SortKey &key, const Evaluation &evaluation) const {
	key.construct(0, 0, BaseExpressionPtr(this), evaluation.zero, 1);
}

