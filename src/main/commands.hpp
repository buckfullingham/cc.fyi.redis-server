#ifndef REDIS_SERVER_COMMANDS_HPP
#define REDIS_SERVER_COMMANDS_HPP

#include "database.hpp"
#include "resp.hpp"

#include <string_view>
#include <vector>

namespace redis::commands {
using args_t = std::vector<std::string_view>;
using cmd_t = void (*)(const redis::commands::args_t &, redis::database &,
                       redis::resp::handler &);
} // namespace redis::commands

extern "C" {
void redis_cmd_ping(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
void redis_cmd_echo(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
void redis_cmd_get(const redis::commands::args_t &, redis::database &,
                   redis::resp::handler &);
void redis_cmd_set(const redis::commands::args_t &, redis::database &,
                   redis::resp::handler &);
void redis_cmd_del(const redis::commands::args_t &, redis::database &,
                   redis::resp::handler &);
void redis_cmd_exists(const redis::commands::args_t &, redis::database &,
                      redis::resp::handler &);
void redis_cmd_incr(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
void redis_cmd_decr(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
void redis_cmd_rpush(const redis::commands::args_t &, redis::database &,
                     redis::resp::handler &);
void redis_cmd_lpush(const redis::commands::args_t &, redis::database &,
                     redis::resp::handler &);
void redis_cmd_lrange(const redis::commands::args_t &, redis::database &,
                      redis::resp::handler &);
void redis_cmd_save(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
void redis_cmd_load(const redis::commands::args_t &, redis::database &,
                    redis::resp::handler &);
}
#endif // REDIS_SERVER_COMMANDS_HPP
