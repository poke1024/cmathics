###STATIC_IF library for C++14 (g++ 5.1.0 or clang++ 3.4.0)
[![Build Status](https://travis-ci.org/gununu/static_if.svg)](https://travis-ci.org/gununu/static_if)

copyright 2011-2015 (c) gununu

###Lisence    
Use, modification, and distribution is subject to the Boost Software License, Version 1.0. 
(See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


###Usage
Start with STATIC_IF and finish with STATIC_ENDIF.
LAZY_VALUE and LAZY_TYPE are needed to get they dependent name.
 
Can't use following statement in block.
"return", "break", "continue" and "goto"


###Example
```c++
template <int Val>
struct S {
    static_assert(Val < 10, "");
};

void func() {
    int a;
    const int N = 30; 
    STATIC_IF( N < 10 ) {     // (REQUIRED) if block
        S<LAZY_VALUE(N)> v;   // (REQUIRED) LAZY_VALUE or LAZY_TYPE are needed to avoid S's static_assert on clang.
        v.foobar();
        N;                    // Note: g++ 4.9.2 can't use const local variables.
    } STATIC_ELSEIF( N==10 ) { //(optional) elseif block

        STATIC_IF(true){      // (optional) nested if block
            cout << "foo";
        }STATIC_ENDIF   

    } STATIC_ELSEIF( N==20 ) { // (optional) can use elseif as many times

        cout << a;            // can use local variable

    } STATIC_ELSE {           // (optional) else block

    } STATIC_ENDIF            // (REQUIRED) end block
}
```
