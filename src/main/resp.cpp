#include "resp.hpp"
#include "util.hpp"

#include <cassert>
#include <charconv>
#include <regex>
#include <span>

namespace {

namespace ns = redis::resp;

constexpr std::string_view crlf = "\r\n";
constexpr char cr = '\r';
constexpr char lf = '\n';

inline void end(std::ostream &os_) { os_ << "\r\n"; }

} // namespace

void ns::writer::begin_simple_string() { os_ << '+'; }

void ns::writer::end_simple_string() { end(os_); }

void ns::writer::begin_error() { os_ << '-'; }

void ns::writer::end_error() { end(os_); }

void ns::writer::begin_integer() { os_ << ':'; }

void ns::writer::end_integer() { end(os_); }

void ns::writer::begin_bulk_string(std::int64_t len) {
  os_ << '$' << len;
  if (len != -1)
    os_ << "\r\n";
}

void ns::writer::end_bulk_string() { end(os_); }

void ns::writer::begin_array(std::int64_t len) { os_ << '*' << len << "\r\n"; }

void ns::writer::end_array() {}

void ns::writer::chars(const char *begin, const char *end) {
  os_ << std::string_view(begin, end);
}

ns::writer::writer(std::ostream &os) : os_(os), depth_() {}

ns::parser_state_base::parser_state_base(parser &self) : self(self) {}

ns::simple_state::simple_state(parser &self, void (handler::*end_callback)())
    : parser_state_base(self), end_callback_(end_callback) {}

ns::length_state::length_state(
    parser &self, void (handler::*const begin_callback)(std::int64_t),
    void (*const factory)(parser &, std::int64_t))
    : parser_state_base(self), begin_callback_(begin_callback),
      factory_(factory) {}

ns::bulk_string_state::bulk_string_state(parser &self, std::int64_t length)
    : parser_state_base(self), length_(length) {}

ns::array_state::array_state(parser &self, std::int64_t length)
    : parser_state_base(self), length_(length) {}

ns::parser::parser(handler &handler) : stack_(), handler_(handler) {
  stack_.emplace_back(std::in_place_type<init_state<true>>, *this);
}

const char *ns::parser::parse(const char *begin, const char *end) {
  for (bool keep_going = true; keep_going;) {
    std::tie(keep_going, begin) = stack_.back().parse(begin, end);
  }

  return begin;
}

template <bool is_terminal>
std::tuple<bool, const char *>
ns::init_state<is_terminal>::parse(const char *begin, const char *end) {
  if (begin == end)
    return {false, begin};

  if constexpr (!is_terminal) {
    self.stack_.pop_back();
  }

  switch (*begin) {
  case '+':
    self.handler_.begin_simple_string();
    self.stack_.emplace_back(std::in_place_type<simple_state>, self,
                             &handler::end_simple_string);
    break;
  case '-':
    self.handler_.begin_error();
    self.stack_.emplace_back(std::in_place_type<simple_state>, self,
                             &handler::end_error);
    break;
  case ':':
    self.handler_.begin_integer();
    self.stack_.emplace_back(std::in_place_type<simple_state>, self,
                             &handler::end_integer);
    break;
  case '$':
    self.stack_.emplace_back(
        std::in_place_type<length_state>, self, &handler::begin_bulk_string,
        [](auto &self, auto len) {
          self.stack_.pop_back();
          self.stack_.emplace_back(std::in_place_type<bulk_string_state>, self,
                                   len);
        });
    break;
  case '*':
    self.stack_.emplace_back(std::in_place_type<length_state>, self,
                             &handler::begin_array, [](auto &self, auto len) {
                               self.stack_.pop_back();
                               self.stack_.emplace_back(
                                   std::in_place_type<array_state>, self, len);
                             });
    break;
  default:
    self.stack_.emplace_back(std::in_place_type<inline_command_state>, self);
    return {true, begin};
  }
  return {true, begin + 1};
}

std::tuple<bool, const char *> ns::simple_state::parse(const char *begin,
                                                       const char *end) {
  if (begin == end)
    return {false, begin};

  const auto pos = std::find(begin, end, cr);

  self.handler_.chars(begin, pos);

  if (end - pos < 2) {
    return {false, pos};
  } else if (pos[1] == lf) {
    (self.handler_.*end_callback_)();
    self.stack_.pop_back();
    return {true, pos + 2};
  } else {
    throw resp_error("carriage return without newline");
  }
}

std::tuple<bool, const char *> ns::length_state::parse(const char *begin,
                                                       const char *end) {
  if (begin == end)
    return {false, begin};

  if (auto pos = std::search(begin, end, crlf.begin(), crlf.end());
      pos != end) {
    std::int64_t length{};
    auto [ptr, ec] = std::from_chars(begin, end, length);
    if (ec != std::errc() || ptr != pos)
      throw std::runtime_error("bad length");
    (self.handler_.*begin_callback_)(length);
    factory_(self, length);
    return {true, ptr + 2};
  } else {
    return {false, begin};
  }
}

std::tuple<bool, const char *> ns::bulk_string_state::parse(const char *begin,
                                                            const char *end) {
  if (length_ == -1) {
    self.handler_.end_bulk_string();
    self.stack_.pop_back();
    return {true, begin};
  }

  if (begin == end)
    return {false, begin};

  const std::int64_t input_length = end - begin;
  const std::int64_t output_length = std::min(length_, input_length);
  self.handler_.chars(begin, begin + output_length);
  length_ -= output_length;

  if (length_ == 0 && input_length >= output_length + 2) {
    self.handler_.end_bulk_string();
    self.stack_.pop_back();
    return {true, begin + output_length + 2};
  } else {
    return {false, begin + output_length};
  }
}

std::tuple<bool, const char *> ns::array_state::parse(const char *begin,
                                                      const char *) {
  if (length_ == 0 || length_ == -1) {
    self.handler_.end_array();
    self.stack_.pop_back();
    return {true, begin};
  } else if (length_ > 0) {
    --length_;
    self.stack_.emplace_back(std::in_place_type<init_state<false>>, self);
    return {true, begin};
  } else {
    throw resp_error("bad array length");
  }
}

std::tuple<bool, const char *>
ns::inline_command_state::parse(const char *const begin,
                                const char *const end) {
  if (auto pos = std::search(begin, end, crlf.begin(), crlf.end());
      pos != end) {
    std::int64_t len{};
    redis::util::tokenize(begin, pos, [&](auto...) { return ++len; });
    self.handler_.begin_array(len);
    redis::util::tokenize(begin, pos, [&](auto begin, auto end) {
      self.handler_.begin_bulk_string(end - begin);
      self.handler_.chars(begin, end);
      self.handler_.end_bulk_string();
      return true;
    });
    self.handler_.end_array();
    self.stack_.pop_back();
    return {true, pos + 2};
  } else {
    return {false, begin};
  }
}
