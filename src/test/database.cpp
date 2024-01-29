#include <catch2/catch_all.hpp>

#include <chrono>
#include <database.hpp>

namespace ns = redis;

TEST_CASE("set and get, no expiry") {
  ns::database dict;
  dict.set("key", "value");
  auto result = dict.get_string("key", {});
  REQUIRE(!!result);
  CHECK(result->get() == "value");
}

TEST_CASE("set and expired get") {
  using namespace std::chrono_literals;
  ns::database::time_point earlier(1s);
  ns::database::time_point later(2s);
  ns::database dict;
  dict.set("key", "value", earlier);
  auto result = dict.get_string("key", later);
  CHECK(!result);
}

TEST_CASE("set and unexpired get") {
  using namespace std::chrono_literals;
  ns::database::time_point earlier(1s);
  ns::database::time_point later(2s);
  ns::database dict;
  dict.set("key", "value", later);
  auto result = dict.get_string("key", earlier);
  REQUIRE(!!result);
  CHECK(result->get() == "value");
}

TEST_CASE("ex") {
  ns::database::time_point now;
  auto ex = ns::database::ex(now, 1);
  CHECK(ex.time_since_epoch().count() == 1000);
}

TEST_CASE("px") {
  ns::database::time_point now;
  auto px = ns::database::px(now, 1);
  CHECK(px.time_since_epoch().count() == 1);
}

TEST_CASE("exat") {
  auto exat = ns::database::exat(42);
  CHECK(exat.time_since_epoch().count() == 42000);
}

TEST_CASE("pxat") {
  auto pxat = ns::database::pxat(42);
  CHECK(pxat.time_since_epoch().count() == 42);
}
