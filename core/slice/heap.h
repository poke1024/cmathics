#pragma once

template<typename Generator, int UpToSize>
class TinyExpressionFactory {
public:
    std::function<ExpressionRef(void *, const BaseExpressionRef&, const Generator&)> m_construct[UpToSize + 1];

    template<int N>
    void initialize() {
        m_construct[N] = [] (void *pool, const BaseExpressionRef &head, const Generator &f) {
            auto *tpool = reinterpret_cast<ObjectPool<ExpressionImplementation<TinySlice<N>>>*>(pool);
            return ExpressionRef(tpool->construct(head, f));
        };

        STATIC_IF (N >= 1) {
            initialize<N-1>();
        } STATIC_ENDIF
    }

public:
    inline TinyExpressionFactory() {
        initialize<UpToSize>();
    }

    inline ExpressionRef operator()(void * const *pools, const BaseExpressionRef& head, const Generator &f) const {
        const size_t n = f.size();
        return m_construct[n](pools[n], head, f);
    }
};

template<int UpToSize = MaxTinySliceSize>
class TinyExpressionHeap {
private:
    typedef
    std::function<ExpressionRef(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves)>
    MakeFromVector;

    typedef
    std::function<ExpressionRef(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves)>
    MakeFromInitializerList;

    typedef
    std::function<std::tuple<ExpressionRef, BaseExpressionRef*, std::atomic<TypeMask>*>(const BaseExpressionRef &head)>
    MakeLateInit;

    void *_pool[UpToSize + 1];
    MakeFromVector _make_from_vector[UpToSize + 1];
    MakeFromInitializerList _make_from_initializer_list[UpToSize + 1];

    std::function<void(BaseExpression*)> _free[UpToSize + 1];

    template<int N>
    void initialize() {
        const auto pool = new ObjectPool<ExpressionImplementation<TinySlice<N>>>();

        _pool[N] = pool;

        if (N == 0) {
            _make_from_vector[N] = [pool] (
                    const BaseExpressionRef &head,
                    const std::vector<BaseExpressionRef> &leaves) {

                return pool->construct(head);
            };
            _make_from_initializer_list[N] = [pool] (
                    const BaseExpressionRef &head,
                    const std::initializer_list<BaseExpressionRef> &leaves) {

                return pool->construct(head);
            };
        } else {
            _make_from_vector[N] = [pool] (
                    const BaseExpressionRef &head,
                    const std::vector<BaseExpressionRef> &leaves) {

                return pool->construct(head, TinySlice<N>(leaves));
            };
            _make_from_initializer_list[N] = [pool] (
                    const BaseExpressionRef &head,
                    const std::initializer_list<BaseExpressionRef> &leaves) {

                return pool->construct(head, TinySlice<N>(leaves));
            };
        }

        _free[N] = [pool] (BaseExpression *expr) {
            pool->destroy(static_cast<ExpressionImplementation<TinySlice<N>>*>(expr));
        };

        STATIC_IF (N >= 1) {
            initialize<N-1>();
        } STATIC_ENDIF
    }

public:
    inline TinyExpressionHeap() {
        initialize<UpToSize>();
    }

    template<int N>
    TinyExpressionRef<N> allocate(const BaseExpressionRef &head, TinySlice<N> &&slice) {
        static_assert(N <= UpToSize, "N must not be be greater than UpToSize");
        return TinyExpressionRef<N>(static_cast<ObjectPool<ExpressionImplementation<TinySlice<N>>>*>(
                _pool[N])->construct(head, std::move(slice)));
    }

    inline ExpressionRef construct(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
        assert(leaves.size() <= UpToSize);
        return _make_from_vector[leaves.size()](head, leaves);
    }

    template<typename Generator>
    inline auto construct_from_generator(const BaseExpressionRef& head, const Generator &generator) const {
        static TinyExpressionFactory<Generator, UpToSize> factory;
        return factory(_pool, head, generator);
    }

    inline void destroy(BaseExpression *expr, SliceCode slice_id) const {
        const size_t i = tiny_slice_size(slice_id);
        assert(i <= UpToSize);
        _free[i](expr);
    }
};
