#include "catch2/catch_all.hpp"

#include <string>
#include <string_view>
#include <util.hpp>

namespace ns = redis::util;
using namespace std::literals;

TEST_CASE("ci_hash") {
  ns::ci_hash h;
  CHECK(h("hello world"s) != 0);
  CHECK(h("hello world"s) == h("Hello World"sv));
  CHECK(h("hello world") == h("HellO WorlD"s));
}

TEST_CASE("ci_equal") {
  ns::ci_equal eq;
  CHECK(eq("hello world"sv, "Hello World"s));
  CHECK(eq("hello world\xff"sv, "Hello World\xff"s));
}

TEST_CASE("tokenize a whole string") {
  const std::vector<std::string_view> expected{
      "hello", "world", "here's", "a", "token",
  };
  const std::string_view input(" hello  world here's   a token   ");
  std::vector<std::string_view> result;
  ns::tokenize(input.begin(), input.end(), [&](auto b, auto e) {
    result.emplace_back(b, e);
    return true;
  });
  REQUIRE(result.size() == 5);
  REQUIRE(result == expected);
}

TEST_CASE("tokenize early exit") {
  const std::vector<std::string_view> expected{
      "hello",
  };
  const std::string_view input(" hello  world here's   a token   ");
  std::vector<std::string_view> result;
  ns::tokenize(input.begin(), input.end(), [&](auto b, auto e) {
    result.emplace_back(b, e);
    return false;
  });
  REQUIRE(result.size() == 1);
  REQUIRE(result == expected);
}

TEST_CASE("tokenize with leading whitespace") {
  auto s = " hello world"sv;
  std::size_t count{};
  ns::tokenize(s.data(), s.data() + s.size(), [&](auto...) { return ++count; });
  CHECK(count == 2);
}

TEST_CASE("tokenize with trailing whitespace") {
  auto s = "hello world "sv;
  std::size_t count{};
  ns::tokenize(s.data(), s.data() + s.size(), [&](auto...) { return ++count; });
  CHECK(count == 2);
}

TEST_CASE("tokenize with no leading or trailing whitespace") {
  auto s = "hello world"sv;
  std::size_t count{};
  ns::tokenize(s.data(), s.data() + s.size(), [&](auto...) { return ++count; });
  CHECK(count == 2);
}
