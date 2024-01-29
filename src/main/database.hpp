#ifndef REDIS_SERVER_DATABASE_HPP
#define REDIS_SERVER_DATABASE_HPP

#include "util.hpp"

#include <ankerl/unordered_dense.h>

#include <chrono>
#include <fstream>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>

namespace redis {

class wrong_type : std::runtime_error {
public:
  wrong_type() : std::runtime_error("wrong type") {}
};

class would_clobber : std::runtime_error {
public:
  would_clobber() : std::runtime_error("attempt to clobber existing key") {}
};

class database {
public:
  using time_point = std::chrono::sys_time<std::chrono::milliseconds>;
  using string_with_expiry_t =
      std::tuple<std::string, std::optional<time_point>>;
  using list_t = std::list<std::string>;
  using value_t = std::variant<std::monostate, string_with_expiry_t, list_t>;
  using map_t = ankerl::unordered_dense::map<std::string, value_t,
                                             util::cs_hash, std::equal_to<>>;
  using now_t = std::remove_cvref_t<decltype(std::chrono::system_clock::now())>;

  explicit database(
      std::function<now_t()> = std::chrono::system_clock::now,
      std::function<std::unique_ptr<std::iostream>()> = []() {
        return std::make_unique<std::fstream>(
            "state.db", std::fstream::in | std::fstream::out);
      });

  std::optional<std::reference_wrapper<std::string>>
  get_string(std::string_view key, time_point now);

  std::optional<std::reference_wrapper<list_t>> get_list(std::string_view key);
  list_t &create_list(std::string_view key, list_t list = {});
  list_t &get_or_create_list(std::string_view key, list_t list = {});

  std::string &set(std::string_view key, std::string_view value,
                   std::optional<time_point> = {});

  bool del(std::string_view key, time_point now);

  void clear();

  static time_point ex(decltype(std::chrono::system_clock::now()) now,
                       std::int64_t seconds);

  static time_point exat(std::int64_t seconds);

  static time_point px(decltype(std::chrono::system_clock::now()) now,
                       std::int64_t milliseconds);

  static time_point pxat(std::int64_t milliseconds);

  template <typename Visitor> void visit(Visitor visitor) {
    auto visit = [visitor = std::move(visitor)](auto &elem) -> bool {
      auto &key = std::get<0>(elem);
      return std::visit(
          [&](auto &value) -> bool { return visitor(key, value); },
          std::get<1>(elem));
    };

    for (const auto &elem : map_) {
      if (!visit(elem))
        break;
    }
  }

  now_t now();

  std::unique_ptr<std::iostream> state_stream();

private:
  map_t map_;
  std::function<now_t()> now_;
  std::function<std::unique_ptr<std::iostream>()> state_stream_;
};

} // namespace redis

#endif // REDIS_SERVER_DATABASE_HPP
