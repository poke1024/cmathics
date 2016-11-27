#ifndef PATTERN_H
#define PATTERN_H

#include <assert.h>

#include "types.h"
#include "definitions.h"
#include "evaluation.h"
#include "matcher.h"

#include <iostream>

class Blank : public Symbol {
public:
	Blank() :
		Symbol("System`Blank", SymbolBlank) {
	}
};

class BlankSequence : public Symbol {
public:
    BlankSequence() :
	    Symbol("System`BlankSequence", SymbolBlankSequence) {
    }
};

class BlankNullSequence : public Symbol {
public:
    BlankNullSequence() :
	    Symbol("System`BlankNullSequence", SymbolBlankNullSequence) {
    }
};

class Pattern : public Symbol {
public:
    Pattern() :
        Symbol("System`Pattern", SymbolPattern) {
    }
};

class Alternatives : public Symbol {
public:
    Alternatives() :
        Symbol("System`Alternatives", SymbolAlternatives) {
    }
};

class Repeated : public Symbol {
public:
    Repeated() :
        Symbol("System`Repeated", SymbolRepeated) {
    }
};

#endif
