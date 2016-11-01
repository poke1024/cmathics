// apply_from_tuple is taken from http://www.cppsamples.com/common-tasks/apply-tuple-to-function.html
// in C++17, this will become std::apply

template<typename F, typename Tuple, size_t ...S >
decltype(auto) apply_tuple_impl(F&& fn, Tuple&& t, std::index_sequence<S...>)
{
    return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
}
template<typename F, typename Tuple>
decltype(auto) apply_from_tuple(F&& fn, Tuple&& t)
{
    std::size_t constexpr tSize
            = std::tuple_size<typename std::remove_reference<Tuple>::type>::value;
    return apply_tuple_impl(std::forward<F>(fn),
                            std::forward<Tuple>(t),
                            std::make_index_sequence<tSize>());
}

// BuiltinRule is a special form of Rule for builtin functions.

template<int N, typename... refs>
struct BuiltinFunctionArguments {
    typedef typename BuiltinFunctionArguments<N - 1, BaseExpressionRef, refs...>::type type;
};

template<typename... refs>
struct BuiltinFunctionArguments<0, refs...> {
    typedef std::function<BaseExpressionRef(refs...)> type;
};


template<int N>
class BuiltinRule : public Rule {
private:
    using func_type = typename BuiltinFunctionArguments<N>::type;

    func_type _func;

protected:
    virtual BaseExpressionRef apply(const Match &match) const {
        return apply_from_tuple(_func, match.get<N>());
    }

public:
    inline BuiltinRule(const BaseExpressionRef &patt, func_type func) : Rule(patt), _func(func) {
    }
};

template<int N>
RuleRef make_builtin_rule(const BaseExpressionRef &patt, typename BuiltinFunctionArguments<N>::type func) {
    return std::make_unique<BuiltinRule<N>>(patt, func);
}
