#pragma once

inline RewriteRef Cache::rewrite(
    const PatternMatcherRef &matcher,
    const BaseExpressionRef &rhs,
    const Evaluation &evaluation) { // concurrent.

    return RewriteRef(m_rewrite.ensure([&matcher, &rhs, &evaluation] () {
        CompiledArguments arguments(matcher->variables());
        return Rewrite::from_arguments(arguments, rhs, evaluation.definitions);
    }));
}
