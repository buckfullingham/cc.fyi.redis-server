#ifndef REDIS_SERVER_COMMAND_HANDLER_HPP
#define REDIS_SERVER_COMMAND_HANDLER_HPP

#include "database.hpp"
#include "resp.hpp"
#include "util.hpp"

#include <ankerl/unordered_dense.h>

#include <charconv>
#include <chrono>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace redis {

class command_handler : public redis::resp::handler {
public:
  using command_t = void (*)(const std::vector<std::string_view> &,
                             redis::database &, redis::resp::handler &);

  explicit command_handler(database &dict, resp::handler &handler);

  command_handler(const command_handler &) = delete;
  command_handler &operator=(const command_handler &) = delete;

private:
  void begin_simple_string() override;
  void end_simple_string() override;
  void begin_error() override;
  void end_error() override;
  void begin_integer() override;
  void end_integer() override;
  void begin_array(std::int64_t) override;
  void end_array() override;
  void begin_bulk_string(std::int64_t len) override;
  void end_bulk_string() override;
  void chars(const char *begin, const char *end) override;

  database &dict_;
  std::string buf_;
  std::vector<std::size_t> ends_;
  std::vector<std::string_view> args_;
  ankerl::unordered_dense::map<std::string_view, command_t,
                               redis::util::ci_hash, redis::util::ci_equal>
      cmds_;
  resp::handler &output_;
};

} // namespace redis

#endif // REDIS_SERVER_COMMAND_HANDLER_HPP
