#include "concurrent/pool.h"
#include "../tests/doctest.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <random>
#include <chrono>

class Item {
public:
	size_t m_value;

	Item(size_t value) : m_value(value) {
	}

	~Item() {
		m_value = 0;
	}
};

template<typename Pool>
class Test {
private:
	struct Data {
		Item *item;
		size_t value;
	};

	Pool &m_pool;

	bool m_verify;
	std::vector<Data> m_data;
	size_t m_thread_hash;
	size_t m_index;

	size_t m_n_adds;
	size_t m_n_removes;

public:
	Test(Pool &pool, bool verify) : m_pool(pool), m_index(1) {
		m_n_adds = 0;
		m_n_removes = 0;
		m_verify = verify;
		std::hash<std::thread::id> hasher;
		m_thread_hash = hasher(std::this_thread::get_id());
	}

	void add() {
		const size_t value = m_index ^ m_thread_hash;

		Item *item = m_pool.construct(value);

		if (m_verify) {
			Data data;
			data.item = item;
			data.value = value;
			m_data.push_back(data);
			m_index++;
		}

		m_n_adds++;
	}

	void remove() {
		if (!m_data.empty()) {
			size_t i = rand() % m_data.size();
			m_pool.destroy(m_data[i].item);
			if (m_verify) {
				m_data.erase(m_data.begin() + i);
			}
			m_n_removes++;
		}
	}

	void resize(int64_t n_pools) {
		const int64_t target_size = n_pools * 1024;

		if (target_size > m_data.size()) {
			const int64_t n = target_size - m_data.size() + int64_t(rand()) % 32;
			for (int64_t i = 0; i < n; i++) {
				add();
			}
		} else {
			const int64_t n = m_data.size() - target_size + int64_t(rand()) % 32;
			for (int64_t i = 0; i < n; i++) {
				remove();
			}
		}

		verify();
	}

	void verify() {
		if (m_verify) {
			for (const Data &data : m_data) {
				assert(data.item->m_value == data.value);
			}
		}
	}

	int64_t size() {
		return m_data.size();
	}

	void print() {
		std::cout << "adds: " << m_n_adds << std::endl;
		std::cout << "removes: " << m_n_adds << std::endl;
	}
};

template<typename Pool>
void do_test(Pool *pool, int iterations, bool verify) {
	Test<Pool> test(*pool, verify);

	std::mt19937_64 twister;
	twister.seed(534629461);
	std::uniform_int_distribution<> rnd(1, 64);

	for (int j = 0; j < iterations; j++) {
		test.resize(rnd(twister));
	}

	// test.print();
}

TEST_CASE("Pool") {
	ObjectPool<Item, 32> m_pool;

	// auto time0 = std::chrono::high_resolution_clock::now();

	const bool verify = false;
	const bool iterations = 100;

	std::vector<std::thread> threads;
	const int n_threads = std::max(int(std::thread::hardware_concurrency()), 2);

	for (int i = 0; i < n_threads; i++) {
		threads.push_back(std::thread(do_test<ObjectPool<Item, 32>>, &m_pool, iterations, verify));
	}

	for (std::thread &t : threads) {
		t.join();
	}

	// auto time1 = std::chrono::high_resolution_clock::now();
	// std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(time1 - time0).count() << std::endl;
}