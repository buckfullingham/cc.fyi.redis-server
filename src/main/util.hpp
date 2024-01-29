#ifndef REDIS_SERVER_UTIL_HPP
#define REDIS_SERVER_UTIL_HPP

#include <ankerl/unordered_dense.h>

#include <cassert>
#include <cstdint>
#include <string_view>
#include <utility>

namespace redis::util {

// yields a hash that's approx 6x faster than using toupper
static constexpr unsigned char ucase_lookup[] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    90,  91,  92,  93,  94,  95,  96,  65,  66,  67,  68,  69,  70,  71,  72,
    73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,
    88,  89,  90,  123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255};

class cs_hash {
public:
  using is_transparent = void;
  using is_avalanching = void;

  auto operator()(const std::string_view s) const noexcept {
    return ankerl::unordered_dense::hash<std::string_view>{}(s);
  }
};

class ci_hash {
public:
  using is_transparent = void;

  template <typename T>
  auto operator()(const T &t) const noexcept
      -> decltype(std::string_view(t), std::size_t{}) {
    std::uint64_t result{};
    for (const unsigned char c : std::string_view(t))
      result = 17000069 * result + ucase_lookup[c];
    return result;
  }
};

class ci_equal {
public:
  using is_transparent = void;

  bool operator()(const std::string_view lhs,
                  const std::string_view rhs) const noexcept {
    if (lhs.size() != rhs.size())
      return false;

    for (auto i = lhs.begin(), e = lhs.end(), j = rhs.begin(); i != e;
         ++i, ++j) {
      if (ucase_lookup[*i] != ucase_lookup[*j]) {
        return false;
      }
    }

    assert(ci_hash()(lhs) == ci_hash()(rhs));
    return true;
  }
};

template <typename Visitor>
auto tokenize(const char *begin, const char *end, Visitor visitor)
    -> decltype(bool(visitor(begin, end)), void()) {

  const auto is_space = [](const unsigned char c) -> bool {
    return ::isspace(c);
  };

  const char *token_begin{};

  for (auto i = begin; i != end; ++i) {
    if (token_begin && is_space(*i)) {
      if (!visitor(std::exchange(token_begin, nullptr), i)) {
        return;
      }
    } else if (!token_begin && !is_space(*i)) {
      token_begin = i;
    }
  }

  if (token_begin)
    visitor(token_begin, end);
}

template <typename... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

} // namespace redis::util

#endif // REDIS_SERVER_UTIL_HPP
