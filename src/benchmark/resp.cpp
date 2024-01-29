#include <benchmark/benchmark.h>

#include <io.hpp>
#include <resp.hpp>

#include <iostream>
#include <random>
#include <sstream>

namespace {
class recorder : public redis::resp::handler {
  void emplace_back(void (redis::resp::handler::*m)()) {
    events_.emplace_back([m](redis::resp::handler &h) { (h.*m)(); });
  }

public:
  void begin_simple_string() override {
    emplace_back(&redis::resp::handler::begin_simple_string);
  }
  void end_simple_string() override {
    emplace_back(&redis::resp::handler::end_simple_string);
  }
  void begin_error() override {
    emplace_back(&redis::resp::handler::begin_error);
  }
  void end_error() override { emplace_back(&redis::resp::handler::end_error); }
  void begin_integer() override {
    emplace_back(&redis::resp::handler::begin_integer);
  }
  void end_integer() override {
    emplace_back(&redis::resp::handler::end_integer);
  }
  void begin_bulk_string(std::int64_t len) override {
    events_.emplace_back(
        [=](redis::resp::handler &h) { h.begin_bulk_string(len); });
  }
  void end_bulk_string() override {
    emplace_back(&redis::resp::handler::end_bulk_string);
  }
  void begin_array(std::int64_t len) override {
    events_.emplace_back([=](redis::resp::handler &h) { h.begin_array(len); });
  }
  void end_array() override { emplace_back(&redis::resp::handler::end_array); }
  void chars(const char *begin, const char *end) override {
    events_.emplace_back(
        [s = std::string(begin, end)](redis::resp::handler &h) {
          h.chars(s.data(), s.data() + s.size());
        });
  }

  std::vector<std::function<void(redis::resp::handler &)>> events_;
};
} // namespace

void write_random_data(std::mt19937 &prng, redis::resp::writer &writer) {
  std::uniform_int_distribution<int> type_dist(0, 8);
  std::uniform_int_distribution<char> alpha_dist('a', 'z');
  std::uniform_int_distribution<char> num_dist('0', '9');
  std::uniform_int_distribution<char> ascii_dist;
  std::uniform_int_distribution<int> strlen_dist(0, 60);
  std::uniform_int_distribution<int> intlen_dist(0, 15);
  std::uniform_int_distribution<int> arrlen_dist(0, 4);

  auto random_chars = [&](auto len, auto &dist) {
    std::array<char, 64> result{};
    std::generate_n(result.begin(), len, [&]() { return dist(prng); });
    writer.chars(result.begin(), result.begin() + len);
  };

  switch (type_dist(prng)) {
  case 0:
    writer.begin_simple_string();
    random_chars(strlen_dist(prng), alpha_dist);
    writer.end_simple_string();
    break;
  case 1:
    writer.begin_error();
    random_chars(strlen_dist(prng), alpha_dist);
    writer.end_error();
    break;
  case 2:
    writer.begin_integer();
    random_chars(intlen_dist(prng), num_dist);
    writer.end_integer();
    break;
  case 3: {
    auto len = strlen_dist(prng);
    writer.begin_bulk_string(len);
    random_chars(len, ascii_dist);
    writer.end_bulk_string();
    break;
  }
  case 4:
    writer.begin_array(-1);
    writer.end_array();
    break;
  case 5:
    writer.begin_bulk_string(-1);
    writer.end_bulk_string();
    break;
  case 6:
  case 7:
  case 8:
  default: {
    auto len = arrlen_dist(prng);
    writer.begin_array(len);
    for (int i = 0; i < len; ++i) {
      write_random_data(prng, writer);
    }
    writer.end_array();
    break;
  }
  }
}

const std::string random_data = []() {
  std::mt19937 prng(42);
  std::ostringstream os;
  redis::resp::writer writer(os);
  for (int i = 0; i < 1 << 10; ++i)
    write_random_data(prng, writer);
  auto result = os.str();
  std::cout << "random_data is [" << result.size() << "] long" << std::endl;
  return std::move(result);
}();

const std::vector<std::function<void(redis::resp::handler &)>>
    random_data_events = []() {
      recorder recording;
      redis::resp::parser parser(recording);
      parser.parse(random_data.data(), random_data.data() + random_data.size());
      return std::move(recording.events_);
    }();

void resp_parsing(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    redis::resp::null_handler h;
    redis::resp::parser parser(h);
    state.ResumeTiming();
    parser.parse(random_data.c_str(), random_data.c_str() + random_data.size());
  }
}

void resp_writing(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    redis::io::ofstreambuf sb(
        redis::io::file_descriptor(::memfd_create, "resp writing benchmark", 0),
        1 << 13);
    std::ostringstream os2;
    redis::resp::writer writer2(os2);
    state.ResumeTiming();
    for (const auto &event : random_data_events)
      event(writer2);
  }
}

BENCHMARK(resp_parsing);
BENCHMARK(resp_writing);
