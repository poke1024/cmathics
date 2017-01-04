#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "core/runtime.h"

int main(int argc, char** argv) {
    Runtime::init();

    Runtime runtime;

    runtime.run_tests();

    doctest::Context context;

    int res = context.run(); // run

    if (context.shouldExit()) {
        return res;
    }

    int client_stuff_return_code = 0;

    return res + client_stuff_return_code;
}
