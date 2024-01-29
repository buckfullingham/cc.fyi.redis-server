#include "command_handler.hpp"
#include "commands.hpp"

namespace {
[[noreturn]] void unimplemented() { throw std::runtime_error("unimplemented"); }
} // namespace

redis::command_handler::command_handler(database &dict, resp::handler &handler)
    : dict_(dict), output_(handler) {
  cmds_["PING"] = redis_cmd_ping;
  cmds_["ECHO"] = redis_cmd_echo;
  cmds_["SET"] = redis_cmd_set;
  cmds_["GET"] = redis_cmd_get;
  cmds_["EXISTS"] = redis_cmd_exists;
  cmds_["DEL"] = redis_cmd_del;
  cmds_["INCR"] = redis_cmd_incr;
  cmds_["DECR"] = redis_cmd_decr;
  cmds_["RPUSH"] = redis_cmd_rpush;
  cmds_["LPUSH"] = redis_cmd_lpush;
  cmds_["LRANGE"] = redis_cmd_lrange;
  cmds_["SAVE"] = redis_cmd_save;
}

void redis::command_handler::begin_simple_string() { unimplemented(); }

void redis::command_handler::end_simple_string() { unimplemented(); }

void redis::command_handler::begin_error() { unimplemented(); }

void redis::command_handler::end_error() { unimplemented(); }

void redis::command_handler::begin_integer() { unimplemented(); }

void redis::command_handler::end_integer() { unimplemented(); }

void redis::command_handler::begin_array(std::int64_t len) {
  buf_.clear();
  args_.clear();
  ends_.clear();
  args_.reserve(len);
  ends_.reserve(len);
}

void redis::command_handler::end_array() {
  std::accumulate(ends_.begin(), ends_.end(), std::size_t(0),
                  [this](auto begin, auto end) {
                    args_.emplace_back(&buf_[begin], &buf_[end]);
                    return end;
                  });
  if (auto pos = cmds_.find(args_[0]); pos != cmds_.end()) {
    pos->second(args_, dict_, output_);
  } else {
    std::string_view msg = "ERR unknown command";
    output_.begin_error();
    output_.chars(msg.begin(), msg.end());
    output_.end_error();
  }
}

void redis::command_handler::begin_bulk_string(std::int64_t len) {
  buf_.reserve(buf_.size() + len);
}

void redis::command_handler::end_bulk_string() { ends_.push_back(buf_.size()); }

void redis::command_handler::chars(const char *begin, const char *end) {
  buf_.append(begin, end);
}
