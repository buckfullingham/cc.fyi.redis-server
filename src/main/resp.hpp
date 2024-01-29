#ifndef REDIS_SERVER_RESP_HPP
#define REDIS_SERVER_RESP_HPP

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace redis::resp {
class resp_error : public std::runtime_error {
public:
  using runtime_error::runtime_error;
};

class handler {
public:
  virtual void begin_simple_string() = 0;

  virtual void end_simple_string() = 0;

  virtual void begin_error() = 0;

  virtual void end_error() = 0;

  virtual void begin_integer() = 0;

  virtual void end_integer() = 0;

  virtual void begin_bulk_string(std::int64_t len) = 0;

  virtual void end_bulk_string() = 0;

  virtual void begin_array(std::int64_t len) = 0;

  virtual void end_array() = 0;

  virtual void chars(const char *begin, const char *end) = 0;

  handler() = default;

  virtual ~handler() = default;
};

class null_handler : public redis::resp::handler {
public:
  void begin_simple_string() override {}
  void end_simple_string() override {}
  void begin_error() override {}
  void end_error() override {}
  void begin_integer() override {}
  void end_integer() override {}
  void begin_bulk_string(std::int64_t len) override {}
  void end_bulk_string() override {}
  void begin_array(std::int64_t len) override {}
  void end_array() override {}
  void chars(const char *begin, const char *end) override {}
};

class writer : public handler {

public:
  void begin_simple_string() override;

  void end_simple_string() override;

  void begin_error() override;

  void end_error() override;

  void begin_integer() override;

  void end_integer() override;

  void begin_bulk_string(std::int64_t len) override;

  void end_bulk_string() override;

  void begin_array(std::int64_t len) override;

  void end_array() override;

  void chars(const char *begin, const char *end) override;

  explicit writer(std::ostream &os);
  writer(const writer &) = delete;
  writer &operator=(const writer &) = delete;

  ~writer() override = default;

private:
  std::ostream &os_;
  std::size_t depth_;
};

class parser;

class parser_state_base {
public:
  explicit parser_state_base(parser &self);

protected:
  parser &self;
};

template <bool is_terminal = false>
class init_state : public parser_state_base {
public:
  using parser_state_base::parser_state_base;

  std::tuple<bool, const char *> parse(const char *begin, const char *end);
};

class simple_state : public parser_state_base {
public:
  using parser_state_base::parser_state_base;

  simple_state(parser &self, void (handler::*end_callback)());

  std::tuple<bool, const char *> parse(const char *begin, const char *end);

private:
  void (handler::*const end_callback_)();
};

class length_state : public parser_state_base {
public:
  length_state(parser &self, void (handler::*begin_callback)(std::int64_t),
               void (*factory)(parser &, std::int64_t));

  std::tuple<bool, const char *> parse(const char *begin, const char *end);

  void (handler::*const begin_callback_)(std::int64_t);

  void (*const factory_)(parser &, std::int64_t);
};

class bulk_string_state : public parser_state_base {
public:
  bulk_string_state(parser &self, std::int64_t length);

  std::tuple<bool, const char *> parse(const char *begin, const char *end);

  std::int64_t length_;
};

class array_state : public parser_state_base {
public:
  array_state(parser &self, std::int64_t length);

  std::tuple<bool, const char *> parse(const char *begin, const char *end);

  std::int64_t length_;
};

class inline_command_state : public parser_state_base {
public:
  using parser_state_base::parser_state_base;

  std::tuple<bool, const char *> parse(const char *begin, const char *end);
};

struct parser_state {
  using parse_t = std::tuple<bool, const char *> (*)(void *, const char *,
                                                     const char *);

  template <typename T, typename... Args>
  explicit parser_state(std::in_place_type_t<T>, Args &&...ctor_args)
      : parse_([](void *ptr, auto... parse_args) {
          return static_cast<T *>(ptr)->parse(parse_args...);
        }),
        storage_{} {
    static_assert(sizeof(T) <= sizeof(storage_));
    static_assert(alignof(T) <= alignof(storage_));
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_copy_constructible_v<T>);
    new (static_cast<void *>(&storage_)) T(std::forward<Args>(ctor_args)...);
  }

  std::tuple<bool, const char *> parse(const char *begin, const char *end) {
    return parse_(static_cast<void *>(&storage_), begin, end);
  }

private:
  const parse_t parse_;
  std::aligned_storage_t<std::max({
                             sizeof(init_state<>),
                             sizeof(simple_state),
                             sizeof(length_state),
                             sizeof(bulk_string_state),
                             sizeof(array_state),
                             sizeof(inline_command_state),
                         }),
                         std::max({
                             alignof(init_state<>),
                             alignof(simple_state),
                             alignof(length_state),
                             alignof(bulk_string_state),
                             alignof(array_state),
                             alignof(inline_command_state),
                         })>
      storage_;
};

class parser {
public:
  explicit parser(handler &handler);

  const char *parse(const char *begin, const char *end);

  std::vector<parser_state> stack_;
  handler &handler_;
  std::vector<std::string_view> inline_args_;
};

} // namespace redis::resp

#endif
