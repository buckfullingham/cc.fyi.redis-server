#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "identity_handler.hpp"

#include <commands.hpp>

namespace ns = redis;

class fixture {
protected:
  fixture()
      : now_(),
        db_([this]() { return now_; },
            [this]() { return std::make_unique<std::istream>(&stringbuf_); },
            [this]() { return std::make_unique<std::ostream>(&stringbuf_); }) {
  }

  std::string submit(redis::commands::cmd_t cmd,
                     const redis::commands::args_t &args) {
    redis::test::identity_handler output;
    cmd(args, db_, output);
    return output.result_;
  }

  redis::database::now_t now_;
  std::stringbuf stringbuf_;
  redis::database db_;
};

TEST_CASE_METHOD(fixture, "ping") {
  const auto result = submit(redis_cmd_ping, {"PINg"});
  CHECK(result == "+PONG\r\n");
}

TEST_CASE_METHOD(fixture, "ping with msg") {
  const auto result = submit(redis_cmd_ping, {"PInG", "msg"});
  CHECK(result == "$3\r\nmsg\r\n");
}

TEST_CASE_METHOD(fixture, "echo") {
  const auto result = submit(redis_cmd_echo, {"EcHO", "msg"});
  CHECK(result == "$3\r\nmsg\r\n");
}

TEST_CASE_METHOD(fixture, "set get and del") {
  CHECK(submit(redis_cmd_set, {"SeT", "key", "value"}) == "+OK\r\n");
  CHECK(submit(redis_cmd_get, {"gET", "key"}) == "$5\r\nvalue\r\n");
  CHECK(submit(redis_cmd_del, {"del", "key"}) == ":1\r\n");
  CHECK(submit(redis_cmd_get, {"gET", "key"}) == "$-1\r\n");
}

TEST_CASE_METHOD(fixture, "failed get") {
  const auto result = submit(redis_cmd_get, {"gET", "key"});
  CHECK(result == "$-1\r\n");
}

TEST_CASE_METHOD(fixture, "expired get") {
  using namespace std::literals;

  auto [type, value] = GENERATE(table<const char *, const char *>({
      {"EX", "2"},
      {"PX", "2000"},
      {"EXAT", "2"},
      {"PXAT", "2000"},
  }));

  const auto set_result =
      submit(redis_cmd_set, {"SeT", "key", "value", type, value});
  CHECK(set_result == "+OK\r\n");

  now_ += std::chrono::seconds(1);

  const auto get_result1 = submit(redis_cmd_get, {"gET", "key"});
  CHECK(get_result1 == "$5\r\nvalue\r\n");

  now_ += std::chrono::seconds(1);

  const auto get_result2 = submit(redis_cmd_get, {"gET", "key"});
  CHECK(get_result2 == "$-1\r\n");
}

TEST_CASE_METHOD(fixture, "exists") {
  submit(redis_cmd_set, {"SeT", "key1", "value1"});
  submit(redis_cmd_set, {"SeT", "key3", "value3"});
  const auto result =
      submit(redis_cmd_exists, {"eXiSts", "key1", "key2", "key3"});
  CHECK(result == ":2\r\n");
}

TEST_CASE_METHOD(fixture, "incr/decr") {
  CHECK(submit(redis_cmd_incr, {"iNCR", "key"}) == ":1\r\n");
  CHECK(submit(redis_cmd_get, {"gEt", "key"}) == "$1\r\n1\r\n");
  CHECK(submit(redis_cmd_incr, {"iNCR", "key"}) == ":2\r\n");
  CHECK(submit(redis_cmd_get, {"gEt", "key"}) == "$1\r\n2\r\n");

  CHECK(submit(redis_cmd_decr, {"DECR", "key"}) == ":1\r\n");
  CHECK(submit(redis_cmd_get, {"gEt", "key"}) == "$1\r\n1\r\n");
  CHECK(submit(redis_cmd_decr, {"deCR", "key"}) == ":0\r\n");
  CHECK(submit(redis_cmd_get, {"gEt", "key"}) == "$1\r\n0\r\n");
}

TEST_CASE_METHOD(fixture, "rpush") {
  CHECK(submit(redis_cmd_rpush, {"RpUsH", "key", "a", "b", "c"}) == ":3\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "key", "0", "2"}) ==
        "*3\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n");
  CHECK(submit(redis_cmd_get, {"get", "key"}) == "-WRONGTYPE\r\n");
  CHECK(submit(redis_cmd_del, {"del", "key"}) == ":1\r\n");
  CHECK(submit(redis_cmd_get, {"get", "key"}) == "$-1\r\n");
}

TEST_CASE_METHOD(fixture, "lrange arguments") {
  CHECK(submit(redis_cmd_rpush, {"RpUsH", "key", "a", "b", "c"}) == ":3\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "key", "0", "2"}) ==
        "*3\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "key", "0", "1"}) ==
        "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "key", "1", "2"}) ==
        "*2\r\n$1\r\nb\r\n$1\r\nc\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "key", "-2", "-1"}) ==
        "*2\r\n$1\r\nb\r\n$1\r\nc\r\n");
  CHECK(submit(redis_cmd_get, {"get", "key"}) == "-WRONGTYPE\r\n");
  CHECK(submit(redis_cmd_del, {"del", "key"}) == ":1\r\n");
  CHECK(submit(redis_cmd_get, {"get", "key"}) == "$-1\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "missing", "-2", "-1"}) ==
        "*0\r\n");
}

TEST_CASE_METHOD(fixture, "save / load") {
  submit(redis_cmd_rpush, {"rpush", "list", "some", "list"});
  submit(redis_cmd_set, {"set", "string", "some string"});
  CHECK(submit(redis_cmd_save, {"save"}) == "+OK\r\n");
  db_.clear();
  REQUIRE(submit(redis_cmd_get, {"get", "string"}) == "$-1\r\n");
  REQUIRE(submit(redis_cmd_get, {"get", "list"}) == "$-1\r\n");
  CHECK(submit(redis_cmd_load, {"load"}) == "+OK\r\n");
  CHECK(submit(redis_cmd_get, {"get", "string"}) == "$11\r\nsome string\r\n");
  CHECK(submit(redis_cmd_lrange, {"lrange", "list", "0", "-1"}) ==
        "*2\r\n$4\r\nsome\r\n$4\r\nlist\r\n");
}
