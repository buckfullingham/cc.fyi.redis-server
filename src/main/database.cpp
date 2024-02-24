#include "database.hpp"

using redis::util::overloaded;

redis::database::database(
    std::function<now_t()> now,
    std::function<std::unique_ptr<std::istream>()> state_istream,
    std::function<std::unique_ptr<std::ostream>()> state_ostream)
    : now_(std::move(now)), state_istream_(std::move(state_istream)), state_ostream_(std::move(state_ostream)) {
  map_.reserve(1 << 20);
}

std::optional<std::reference_wrapper<std::string>>
redis::database::get_string(std::string_view key, time_point now) {
  if (const auto pos = map_.find(key); pos != map_.end()) {
    return std::visit(
        overloaded{
            [&](string_with_expiry_t &x)
                -> std::optional<std::reference_wrapper<std::string>> {
              auto &[value, opt_expiry] = x;
              if (opt_expiry) {
                if (now < *opt_expiry) {
                  return std::ref(value);
                } else {
                  map_.erase(pos);
                  return {};
                }
              } else {
                return std::ref(value);
              }
            },
            [](auto &) -> std::optional<std::reference_wrapper<std::string>> {
              throw wrong_type();
            }},
        pos->second);
  }
  return {};
}

std::string &redis::database::set(std::string_view key, std::string_view value,
                                  std::optional<time_point> expiry) {
  if (auto pos = map_.find(key); pos == map_.end()) {
    return std::get<0>(std::get<string_with_expiry_t>(
        map_.emplace(
                std::piecewise_construct, std::forward_as_tuple(key),
                std::forward_as_tuple(std::in_place_type<string_with_expiry_t>,
                                      value, expiry))
            .first->second));
  } else {
    pos->second = string_with_expiry_t(value, expiry);
    return std::get<0>(std::get<string_with_expiry_t>(pos->second));
  }
}

bool redis::database::del(std::string_view key, const time_point now) {
  if (const auto pos = map_.find(key); pos != map_.end()) {
    const auto expired =
        std::visit(overloaded{[&](string_with_expiry_t &x) -> bool {
                                auto &[value, opt_expiry] = x;
                                return opt_expiry && *opt_expiry < now;
                              },
                              [](auto &) -> bool { return false; }},
                   pos->second);
    map_.erase(pos);
    return !expired;
  }
  return false;
}

redis::database::time_point
redis::database::ex(decltype(std::chrono::system_clock::now()) now,
                    std::int64_t seconds) {
  return std::chrono::time_point_cast<std::chrono::milliseconds>(now) +
         std::chrono::seconds(seconds);
}

redis::database::time_point redis::database::exat(std::int64_t seconds) {
  return time_point() + std::chrono::seconds(seconds);
}

redis::database::time_point
redis::database::px(decltype(std::chrono::system_clock::now()) now,
                    std::int64_t milliseconds) {
  return std::chrono::time_point_cast<std::chrono::milliseconds>(now) +
         std::chrono::milliseconds(milliseconds);
}

redis::database::time_point redis::database::pxat(std::int64_t milliseconds) {
  return time_point() + std::chrono::milliseconds(milliseconds);
}

redis::database::now_t redis::database::now() { return now_(); }

std::optional<std::reference_wrapper<redis::database::list_t>>
redis::database::get_list(std::string_view key) {
  if (const auto pos = map_.find(key); pos == map_.end()) {
    return {};
  } else {
    return std::visit(overloaded{
                          [](list_t &l) -> list_t & { return l; },
                          [](auto &) -> list_t & { throw wrong_type(); },
                      },
                      pos->second);
  }
}

redis::database::list_t &redis::database::create_list(std::string_view key,
                                                      list_t list) {
  if (const auto pos = map_.find(key); pos == map_.end()) {
    return std::ref(std::get<list_t>(
        map_.emplace(std::piecewise_construct, std::forward_as_tuple(key),
                     std::forward_as_tuple(std::in_place_type<list_t>,
                                           std::move(list)))
            .first->second));

  } else {
    throw redis::would_clobber();
  }
}

redis::database::list_t &
redis::database::get_or_create_list(std::string_view key, list_t list) {
  if (const auto pos = map_.find(key); pos == map_.end()) {
    return create_list(key, std::move(list));
  } else {
    return std::visit(overloaded{
                          [](list_t &l) -> list_t & { return l; },
                          [](auto &) -> list_t & { throw wrong_type(); },
                      },
                      pos->second);
  }
}

std::unique_ptr<std::istream> redis::database::state_istream() {
  return state_istream_();
}

std::unique_ptr<std::ostream> redis::database::state_ostream() {
  return state_ostream_();
}

void redis::database::clear() {
  map_.clear();
}
