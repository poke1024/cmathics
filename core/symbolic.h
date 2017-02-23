#pragma once

#include <symengine/basic.h>
#include <symengine/complex.h>

/*#if defined(WITH_SYMENGINE_THREAD_SAFE)
#else
static_assert(false, "need WITH_SYMENGINE_THREAD_SAFE");
#endif*/

typedef SymEngine::RCP<const SymEngine::Basic> SymEngineRef;

typedef SymEngine::RCP<const SymEngine::Complex> SymEngineComplexRef;

class SymbolicForm : public Shared<SymbolicForm, SharedPool> {
private:
    const SymEngineRef m_ref;

public:
    inline SymbolicForm(const SymEngineRef &ref) :
            m_ref(ref) {
    }

    inline bool is_none() const {
        // checks whether there is no SymEngine form for this expression.
        return m_ref.is_null();
    }

    inline const SymEngineRef &get() const {
        return m_ref;
    }
};

typedef ConstSharedPtr<SymbolicForm> SymbolicFormRef;
typedef QuasiConstSharedPtr<SymbolicForm> CachedSymbolicFormRef;
typedef UnsafeSharedPtr<SymbolicForm> UnsafeSymbolicFormRef;

typedef SymEngineRef (*SymEngineUnaryFunction)(
    const SymEngineRef&);

typedef SymEngineRef (*SymEngineBinaryFunction)(
    const SymEngineRef&,
    const SymEngineRef&);

typedef SymEngineRef (*SymEngineNAryFunction)(
    const SymEngine::vec_basic&);

BaseExpressionRef from_symbolic_form(const SymEngineRef &ref, const Evaluation &evaluation);

