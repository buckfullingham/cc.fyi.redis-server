#include "commands.hpp"
#include "command_handler.hpp"
#include "io.hpp"
#include "util.hpp"

#include <fstream>
#include <span>

namespace {

using redis::wrong_type;
using redis::util::overloaded;

class not_an_int : std::runtime_error {
public:
  using runtime_error::runtime_error;
};

void error(redis::resp::handler &output, std::string_view msg) {
  output.begin_error();
  output.chars(msg.begin(), msg.end());
  output.end_error();
};

void simple_string(redis::resp::handler &output, std::string_view value) {
  output.begin_simple_string();
  output.chars(value.data(), value.data() + value.size());
  output.end_simple_string();
}

void bulk_string(redis::resp::handler &output, std::string_view value) {
  output.begin_bulk_string(std::int64_t(value.size()));
  output.chars(value.data(), value.data() + value.size());
  output.end_bulk_string();
}

void nil_string(redis::resp::handler &output) {
  output.begin_bulk_string(-1);
  output.end_bulk_string();
}

template <typename Integer> auto to_chars(Integer i) {
  std::tuple<std::array<char, std::numeric_limits<Integer>::digits10 + 2>,
             std::size_t>
      result;
  auto &[buf, len] = result;
  auto [ptr, ec] = std::to_chars(buf.begin(), buf.end(), i);
  if (ec != std::errc())
    throw std::logic_error("can't render an integer");
  len = ptr - buf.begin();
  return result;
}

template <typename Integer>
void integer(redis::resp::handler &output, Integer i) {
  auto [buf, len] = to_chars(i);
  output.begin_integer();
  output.chars(buf.begin(), buf.begin() + len);
  output.end_integer();
}

std::int64_t parse_int(std::string_view s) {
  std::int64_t result;
  auto [ptr, ec] = std::from_chars(s.begin(), s.end(), result);
  if (ptr != s.end() || ec != std::errc())
    throw not_an_int("error in from_chars");
  return result;
}

} // namespace

void redis_cmd_ping(const redis::commands::args_t &args, redis::database &,
                    redis::resp::handler &output) {
  switch (args.size()) {
  case 1:
    return simple_string(output, "PONG");
  case 2:
    return bulk_string(output, args[1]);
  default:
    return error(output, "ERR wrong number of arguments");
  }
}

void redis_cmd_echo(const redis::commands::args_t &args, redis::database &,
                    redis::resp::handler &output) {
  if (args.size() == 2)
    bulk_string(output, args[1]);
  else
    error(output, "ERR wrong number of arguments");
}

void redis_cmd_set(const redis::commands::args_t &args, redis::database &db,
                   redis::resp::handler &output) {
  using namespace redis;

  std::optional<database::time_point> expiry;

  switch (args.size()) {
  case 3:
    break;
  case 5: {
    const auto &expiry_type = args[3];
    const auto &expiry_value = args[4];

    std::int64_t exp{};
    auto [ptr, ec] =
        std::from_chars(expiry_value.begin(), expiry_value.end(), exp);

    if (ptr != expiry_value.end() || ec != std::errc{} || exp < 0) {
      return error(output,
                   "ERR malformed expiry, which must be a positive integer");
    }

    const util::ci_equal eq;
    if (eq(expiry_type, "EX")) {
      expiry = database::ex(db.now(), exp);
    } else if (eq(expiry_type, "EXAT")) {
      expiry = database::exat(exp);
    } else if (eq(expiry_type, "PX")) {
      expiry = database::px(db.now(), exp);
    } else if (eq(expiry_type, "PXAT")) {
      expiry = database::pxat(exp);
    } else {
      return error(output, "ERR unrecognised option");
    }
  } break;
  default:
    return error(output, "ERR wrong number of arguments to SET command");
  }

  db.set(args[1], args[2], expiry);
  simple_string(output, "OK");
}

void redis_cmd_get(const redis::commands::args_t &args, redis::database &db,
                   redis::resp::handler &output) {
  try {
    if (args.size() > 1) {
      auto now =
          std::chrono::time_point_cast<std::chrono::milliseconds>(db.now());

      if (const auto opt_value = db.get_string(args[1], now)) {
        auto value = opt_value->get();
        return bulk_string(output, value);
      } else {
        return nil_string(output);
      }
    } else {
      return error(output, "ERR wrong number of arguments");
    }
  } catch (const wrong_type &) {
    return error(output, "WRONGTYPE");
  }
}

void redis_cmd_del(const redis::commands::args_t &args, redis::database &db,
                   redis::resp::handler &output) {
  const auto now =
      std::chrono::time_point_cast<std::chrono::milliseconds>(db.now());

  if (args.size() < 2)
    return error(output, "ERR expected at least one key argument");

  std::int64_t count{};

  for (auto &key : std::span(args.begin() + 1, args.end())) {
    if (db.del(key, now))
      ++count;
  }

  return integer(output, count);
}

void redis_cmd_exists(const redis::commands::args_t &args, redis::database &db,
                      redis::resp::handler &output) {
  const auto now =
      std::chrono::time_point_cast<std::chrono::milliseconds>(db.now());

  if (args.size() < 2)
    return error(output, "ERR expected at least one key argument");

  std::int64_t count{};

  for (auto &key : std::span(args.begin() + 1, args.end())) {
    if (db.get_string(key, now))
      ++count;
  }

  integer(output, count);
}

namespace {
void incr_or_decr(const redis::commands::args_t &args, redis::database &db,
                  redis::resp::handler &output, void (*f)(std::int64_t &)) {
  const auto now =
      std::chrono::time_point_cast<std::chrono::milliseconds>(db.now());
  const auto &key = args[1];

  if (args.size() != 2)
    return error(output, "ERR expected one key argument");

  auto &value = [&]() -> std::string & {
    if (auto opt_result = db.get_string(key, now))
      return opt_result->get();
    else
      return db.set(key, "0");
  }();

  auto i = parse_int(value);

  f(i);

  auto [buf, len] = to_chars(i);

  db.set(key, std::string_view(buf.begin(), len));

  integer(output, i);
}
} // namespace

void redis_cmd_incr(const redis::commands::args_t &args, redis::database &db,
                    redis::resp::handler &output) {
  incr_or_decr(args, db, output, [](auto &i) { ++i; });
}

void redis_cmd_decr(const redis::commands::args_t &args, redis::database &db,
                    redis::resp::handler &output) {
  incr_or_decr(args, db, output, [](auto &i) { --i; });
}

namespace {
void rpush_lpush(const redis::commands::args_t &args, redis::database &db,
                 redis::resp::handler &output,
                 void (*push)(redis::database::list_t &, const char *,
                              const char *)) try {
  const auto &key = args[1];

  if (args.size() > 2) {
    auto &list = db.get_or_create_list(key);

    for (auto &s : std::span(args.begin() + 2, args.end()))
      push(list, s.begin(), s.end());

    return integer(output, list.size());
  } else {
    return error(output, "ERR wrong number of arguments");
  }
} catch (const wrong_type &) {
  return error(output, "WRONGTYPE key refers to object of the wrong type");
}
} // namespace

void redis_cmd_rpush(const redis::commands::args_t &args, redis::database &db,
                     redis::resp::handler &output) {
  return rpush_lpush(args, db, output, [](auto &list, auto begin, auto end) {
    list.emplace_back(begin, end);
  });
}

void redis_cmd_lpush(const redis::commands::args_t &args, redis::database &db,
                     redis::resp::handler &output) {
  return rpush_lpush(args, db, output, [](auto &list, auto begin, auto end) {
    list.emplace_front(begin, end);
  });
}

void redis_cmd_lrange(const redis::commands::args_t &args, redis::database &db,
                      redis::resp::handler &output) {
  try {
    if (args.size() == 4) {
      const auto &key = args[1];
      const auto &list = db.get_list(key)->get();

      auto normalise_index = [&](std::int64_t i) -> std::int64_t {
        auto result = i < 0 ? std::int64_t(list.size()) + i : i;
        return result;
      };

      const auto start =
          std::max(std::int64_t(0), normalise_index(parse_int(args[2])));

      const auto stop = std::min(std::int64_t(list.size()),
                                 normalise_index(parse_int(args[3])) + 1);

      if (stop < start) {
        return error(output, "ERR stop before start");
      }

      auto iter = list.begin();
      std::advance(iter, start);

      output.begin_array(stop - start);

      for (std::int64_t i = start; i < stop; ++i, ++iter)
        bulk_string(output, *iter);

      output.end_array();
      return;
    } else {
      return error(output, "ERR wrong number of arguments");
    }
  } catch (const wrong_type &) {
    return error(output, "WRONGTYPE key refers to object of the wrong type");
  } catch (const not_an_int &) {
    return error(output, "ERR bad argument");
  }
}

void redis_cmd_save(const redis::commands::args_t &args, redis::database &db,
                    redis::resp::handler &output) {
  if (args.size() != 1)
    return error(output, "ERR wrong number of arguments");

  try {
    auto file = db.state_stream();
    redis::resp::writer writer(*file);

    db.visit(overloaded{
        [&](auto &key,
            const redis::database::string_with_expiry_t &elem) -> bool {
          auto &[value, expiry] = elem;
          writer.begin_array(expiry ? 5 : 3);
          bulk_string(writer, "SET");
          bulk_string(writer, key);
          bulk_string(writer, value);
          if (expiry) {
            bulk_string(writer, "PXAT");
            auto [buf, len] = to_chars(expiry->time_since_epoch().count());
            bulk_string(writer, std::string_view(buf.begin(), len));
          }
          writer.end_array();
          return true;
        },
        [&](auto &key, const redis::database::list_t &elem) -> bool {
          writer.begin_array(std::int64_t(elem.size()) + 2);
          bulk_string(writer, "RPUSH");
          bulk_string(writer, key);
          for (const auto &s : elem)
            bulk_string(writer, s);
          return true;
        },
        [](auto &, const std::monostate &) -> bool {
          assert(false);
          return true;
        },
    });
    return simple_string(output, "OK");
  } catch (const std::exception &) {
    return error(output, "ERR failed to save db state");
  }
}

void redis_cmd_load(const redis::commands::args_t &args, redis::database &db,
                    redis::resp::handler &output) {
  if (args.size() != 1)
    return error(output, "ERR wrong number of arguments");

  auto stream = db.state_stream();
  redis::resp::null_handler null_handler;
  redis::command_handler command_handler(db, null_handler);
  redis::resp::parser parser(command_handler);
  redis::io::ring_buffer ring_buffer(1 << 13);
  std::size_t read_index = 0;
  std::size_t write_index = 0;
  db.clear();

  auto readable_bytes = [&]() {
    return std::streamsize(write_index - read_index);
  };

  auto writable_bytes = [&, size = ring_buffer.size()]() {
    return std::streamsize(size - (readable_bytes()));
  };

  while (auto num_read = stream->readsome(ring_buffer.addr(write_index),
                                          writable_bytes())) {
    write_index += num_read;
    const auto read_addr = ring_buffer.addr(read_index);
    read_index +=
        parser.parse(read_addr, read_addr + readable_bytes()) - read_addr;
  }

  simple_string(output, "OK");
}
