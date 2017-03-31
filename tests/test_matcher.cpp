// @formatter:off

#include "../core/types.h"
#include "../core/runtime.h"
#include "../tests/doctest.h"

TEST_CASE("match nested") {
    Runtime * const runtime = Runtime::get();

    const auto no_output = std::make_shared<NoOutput>();
    Evaluation evaluation(no_output, runtime->definitions(), false);

    auto pattern = runtime->parse("f[g[{x___}, y__], z__]");
    auto item = runtime->parse("f[g[{1, 5}, 7, 8], 9, 10]");

    const Matcher matcher(pattern, runtime->evaluation());
    MatchRef m = matcher(item, evaluation);
	CHECK(bool(m) == true);

    auto x_ptr = m->get_matched_value(runtime->definitions().lookup("System`x").get());
    CHECK(x_ptr != nullptr);
    auto x_expected = runtime->parse("Sequence[1, 5]");
    CHECK((*x_ptr)->same(x_expected));

    auto y_ptr = m->get_matched_value(runtime->definitions().lookup("System`y").get());
    CHECK(y_ptr != nullptr);
    auto y_expected = runtime->parse("Sequence[7, 8]");
    CHECK((*y_ptr)->same(y_expected));

    auto z_ptr = m->get_matched_value(runtime->definitions().lookup("System`z").get());
    CHECK(z_ptr != nullptr);
    auto z_expected = runtime->parse("Sequence[9, 10]");
    CHECK((*z_ptr)->same(z_expected));
}
