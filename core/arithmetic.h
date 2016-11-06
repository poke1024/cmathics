#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "types.h"
#include "expression.h"
#include "pattern.h"

BaseExpressionRef Plus(
	const ExpressionRef &expr,
	const Evaluation &evaluation);

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
	const BaseExpressionRef &imax,
	const BaseExpressionRef &di,
	const Evaluation &evaluation);

#endif
