#ifndef REDIS_SERVER_IDENTITY_HANDLER_HPP
#define REDIS_SERVER_IDENTITY_HANDLER_HPP

#include <catch2/catch_all.hpp>
#include <resp.hpp>

#include <cstdint>
#include <sstream>
#include <stack>
#include <string>

namespace redis::test {

class identity_handler : public resp::handler {
public:
  void begin_simple_string() override {
    result_ += '+';
    stack_.push('+');
  }

  void end_simple_string() override {
    result_ += "\r\n";
    CHECK(stack_.top() == '+');
    stack_.pop();
  }

  void begin_error() override {
    result_ += '-';
    stack_.push('-');
  }

  void end_error() override {
    result_ += "\r\n";
    CHECK(stack_.top() == '-');
    stack_.pop();
  }

  void begin_integer() override {
    result_ += ':';
    stack_.push(':');
  }

  void end_integer() override {
    result_ += "\r\n";
    CHECK(stack_.top() == ':');
    stack_.pop();
  }

  void begin_bulk_string(std::int64_t len) override {
    std::ostringstream oss;
    oss << '$' << len;
    result_ += oss.str();
    if (len != -1)
      result_ += "\r\n";
    stack_.push('$');
  }

  void end_bulk_string() override {
    result_ += "\r\n";
    CHECK(stack_.top() == '$');
    stack_.pop();
  }

  void begin_array(std::int64_t len) override {
    std::ostringstream oss;
    oss << '*' << len << "\r\n";
    result_ += oss.str();
    stack_.push('*');
  }

  void end_array() override {
    CHECK(stack_.top() == '*');
    stack_.pop();
  }

  void chars(const char *begin, const char *end) override {
    result_.append(begin, end);
  }

  std::string result_;
  std::stack<char> stack_;
};

} // namespace redis::test

#endif // REDIS_SERVER_IDENTITY_HANDLER_HPP
