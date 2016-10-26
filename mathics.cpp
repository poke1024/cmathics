#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gmp.h>
#include <iostream>

#include "core/misc.h"
#include "core/expression.h"
#include "core/definitions.h"
#include "core/formatter.h"
#include "core/evaluation.h"
#include "core/pattern.h"
#include "core/integer.h"
#include "core/real.h"
#include "core/rational.h"
#include "core/arithmetic.h"
#include <vector>
#include "core/string.h"

#include <Python.h>
#include <stdlib.h>

int main() {
    // set PYTHONHOME to a python with a Mathics installation.
    /*setenv("PYTHONHOME", "/Users/bernhard/Projekte/j5", true);
    Py_Initialize();

    PyObject* main = PyImport_AddModule("__main__");
    PyObject* globalDictionary = PyModule_GetDict(main);
    PyObject* localDictionary = PyDict_New();

    const char* pythonScript = "result = multiplicand * multiplier\n";
    PyDict_SetItemString(localDictionary, "multiplicand", PyLong_FromLong(2));
    PyDict_SetItemString(localDictionary, "multiplier", PyLong_FromLong(5));
    PyRun_String(pythonScript, Py_file_input, globalDictionary, localDictionary);
    long result = PyLong_AsLong(PyDict_GetItemString(localDictionary, "result"));
    std::cout << result;

    Py_Finalize();
    return 0;*/

    Definitions* definitions = new Definitions();

    auto x = definitions->new_symbol("Global`x");

    auto patt = new Expression(definitions->lookup("System`Pattern"), {
        x, new Expression(definitions->lookup("System`Blank"), {})
    });

    Match m = patt->match(definitions, new MachineInteger(7));
    std::cout << m;


    patt = new Expression(definitions->lookup("System`Pattern"), {
        x, new Expression(definitions->lookup("System`BlankSequence"), {})
    });

    m = patt->match(definitions, new Expression(
        definitions->lookup("System`Sequence"), {new MachineInteger(1), new MachineInteger(3)}));
    std::cout << m;

    /*

    Symbol* plus_symbol = definitions->new_symbol("Plus");
    plus_symbol->down_code = _Plus;

    BaseExpression* head = (BaseExpression*) plus_symbol;
    //BaseExpression* la = (BaseExpression*) Definitions_lookup(definitions, "a");
    //BaseExpression* lb = (BaseExpression*) Definitions_lookup(definitions, "b");

    auto la = new MachineInteger(5);
    auto lb = new MachineReal(10.2);

    std::vector<BaseExpression*> leaves;
    leaves.push_back(la);
    leaves.push_back(lb);
    Expression* expr = new Expression(head, leaves);

    Evaluation* evaluation = new Evaluation(definitions, true);

    std::cout << expr->fullform() << std::endl;

    std::cout << "evaluating...\n";
    BaseExpression* result = evaluation->evaluate(expr);
    std::cout << "done\n";

    std::cout << result->fullform();
    */
    // printf("height = %li\n", Expression_height((BaseExpression*) expr));
    // printf("hash = %lu\n", expr->hash);

    return 0;
}
