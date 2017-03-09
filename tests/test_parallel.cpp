// @formatter:off

#include "../core/types.h"
#include "../core/runtime.h"
#include "../tests/doctest.h"
#include "../concurrent/parallel.h"

#include <vector>

TEST_CASE("parallelize") {
	auto &definitions = Runtime::get()->definitions();
	const auto output = std::make_shared<TestOutput>();
	Evaluation evaluation(output, definitions, false);

	constexpr size_t n = 1000000;

	std::vector<int> numbers(n);
	std::vector<std::thread::id> ids(n);

	for (int i = 0; i < n; i++) {
		numbers[i] = 0;
	}

	parallelize([&numbers, &ids] (size_t i) {
		numbers[i] = 3 * i + (i >> 2);
		ids[i] = std::this_thread::get_id();
	}, n, evaluation);

	std::unordered_set<std::thread::id> distinct_ids;

	for (size_t i = 0; i < n; i++) {
		if (numbers[i] != 3 * i + (i >> 2)) {
			CHECK(numbers[i] == 3 * i + (i >> 2));
		}
		distinct_ids.insert(ids[i]);
	}

	CHECK(distinct_ids.size() > 1);
}
