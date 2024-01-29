#include "catch2/catch_all.hpp"

#include "identity_handler.hpp"

#include <resp.hpp>

#include <stack>
#include <string>
#include <vector>

namespace {
using namespace std::literals;
namespace ns = redis::resp;

} // namespace

TEST_CASE("simple string") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = "+hello world\r\n"s;
  const char *begin = s.c_str();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == s);
  CHECK(h.stack_.empty());
}

TEST_CASE("simple error") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = "-hello world\r\n"s;
  const char *begin = s.c_str();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == s);
  CHECK(h.stack_.empty());
}

TEST_CASE("integer") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = ":12345\r\n"s;
  const char *begin = s.c_str();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == s);
  CHECK(h.stack_.empty());
}

TEST_CASE("bulk_string") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = "$5\r\nabcde\r\n"s;
  const char *begin = s.c_str();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == s);
  CHECK(h.stack_.empty());
}

TEST_CASE("array") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = "*1\r\n*1\r\n+a string\r\n"s;
  const char *begin = s.c_str();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == s);
  CHECK(h.stack_.empty());
}

TEST_CASE("inline command") {
  redis::test::identity_handler h;
  ns::parser p(h);

  const auto s = "SET KEY VALUE\r\n"sv;
  const char *begin = s.data();
  const char *end = begin + s.size();

  CHECK(end == p.parse(begin, end));
  CHECK(h.result_ == "*3\r\n$3\r\nSET\r\n$3\r\nKEY\r\n$5\r\nVALUE\r\n");
}

TEST_CASE("example tests") {
  std::vector<std::string> strings{
      "$-1\r\n",
      "*-1\r\n",
      "*1\r\n$4\r\nping\r\n",
      "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n",
      "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n",
      "+OK\r\n",
      "-Error message\r\n",
      "$0\r\n\r\n",
      "+hello world\r\n",
  };

  for (auto &s : strings) {
    redis::test::identity_handler h;
    ns::parser p(h);

    std::deque<char> q;

    for (auto &c : s) {
      q.push_back(c);

      std::string s2(q.begin(), q.end());

      std::size_t n = p.parse(s2.data(), s2.data() + s2.size()) - s2.data();

      for (std::size_t i = 0; i < n; ++i) {
        q.pop_front();
      }
    }

    CHECK(q.empty());
    CHECK(h.result_ == s);
    CHECK(h.stack_.empty());
  }
}

TEST_CASE("writing resp") {
  std::ostringstream os;
  ns::writer writer(os);

  auto chars = [&](std::string_view s) {
    writer.chars(s.data(), s.data() + s.size());
  };

  writer.begin_array(2);
  writer.begin_simple_string();
  chars("OK");
  writer.end_simple_string();
  writer.begin_integer();
  chars("42");
  writer.end_integer();
  writer.end_array();
  writer.begin_error();
  chars("ERR");
  writer.end_error();

  CHECK(os.str() == "*2\r\n+OK\r\n:42\r\n-ERR\r\n");
}
