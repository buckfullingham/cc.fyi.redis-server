#include <benchmark/benchmark.h>

#include <util.hpp>

#include <random>

const std::string random_data = []() {
  std::string result;
  std::mt19937 prng(42);
  std::uniform_int_distribution<char> printable_char(33, 126);

  std::generate_n(std::back_inserter(result), 1 << 10,
                  [&]() { return printable_char(prng); });
  return result;
}();

void case_insensitive_hash(benchmark::State &state) {
  redis::util::ci_hash hash;
  for (auto _ : state) {
    std::size_t h;
    benchmark::DoNotOptimize(h = hash(random_data));
    benchmark::ClobberMemory();
  }
}

void case_insensitive_equal(benchmark::State &state) {
  redis::util::ci_equal eq;
  for (auto _ : state) {
    bool result;
    benchmark::DoNotOptimize(result = eq(random_data, random_data));
    benchmark::ClobberMemory();
  }
}

BENCHMARK(case_insensitive_hash);
BENCHMARK(case_insensitive_equal);
