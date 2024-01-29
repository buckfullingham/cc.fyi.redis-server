#ifndef REDIS_SERVER_IO_HPP
#define REDIS_SERVER_IO_HPP

#include <cstdint>

#include <functional>
#include <stdexcept>
#include <streambuf>
#include <system_error>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace redis::io {

/**
 * Call a posix style function under TEMP_FAILURE_RETRY and throw if there's an
 * error condition.
 * @tparam Function
 * @tparam Args
 * @param function
 * @param args
 * @return
 */
template <typename Function, typename... Args>
inline auto posix_call(Function function, Args... args)
    -> decltype(function(std::forward<Args>(args)...)) {
  using result_t = decltype(function(std::forward<Args>(args)...));
  result_t result{};
  TEMP_FAILURE_RETRY(result = function(std::forward<Args>(args)...));
  if (result == (result_t)-1)
    throw std::system_error(errno, std::generic_category());
  return result;
}

/**
 * RAII wrapper around a file descriptor that calls ::close() on destruction.
 */
class file_descriptor {
  friend void swap(file_descriptor &, file_descriptor &) noexcept;

public:
  template <typename Function, typename... Args>
  explicit file_descriptor(Function function, Args... args);

  file_descriptor(const file_descriptor &) = delete;
  file_descriptor &operator=(const file_descriptor &) = delete;
  file_descriptor(file_descriptor &&that) noexcept;
  file_descriptor &operator=(file_descriptor &&that) noexcept;

  int release() noexcept;
  void reset() noexcept;

  ~file_descriptor() noexcept;

  [[nodiscard]] int value() const noexcept;

private:
  int value_;
};

void swap(file_descriptor &lhs, file_descriptor &rhs) noexcept;

/**
 * RAII wrapper around a memory mapping that calls ::munmap on destruction.
 */
class memory_map {
  friend void swap(memory_map &, memory_map &) noexcept;

public:
  template <typename... Args>
  memory_map(void *ptr, std::size_t len, Args... args);
  memory_map(const memory_map &) = delete;
  memory_map &operator=(const memory_map &) = delete;
  memory_map(memory_map &&) noexcept;
  memory_map &operator=(memory_map &&) noexcept;

  [[nodiscard]] std::tuple<void *, std::size_t> value() const noexcept;

  std::tuple<void *, std::size_t> release() noexcept;

  void reset() noexcept;

  ~memory_map() noexcept;

private:
  void *ptr_;
  std::size_t len_;
};

void swap(memory_map &lhs, memory_map &rhs) noexcept;

class ring_buffer {
public:
  explicit ring_buffer(std::size_t len);
  ring_buffer(const ring_buffer &) = delete;
  ring_buffer &operator=(const ring_buffer &) = delete;

  [[nodiscard]] char *addr(std::uint64_t i) const { return ptr_ + i % len_; }

  [[nodiscard]] std::size_t size() const { return len_; };

private:
  std::size_t len_;
  file_descriptor fd_;
  memory_map region_;
  char *const ptr_;
};

/**
 * A streambuf for outputting to a file_descriptor.
 */
class ofstreambuf : public std::streambuf {
public:
  ofstreambuf(file_descriptor fd, std::size_t size);
  ofstreambuf(const ofstreambuf &) = delete;
  ofstreambuf &operator=(const ofstreambuf &) = delete;

private:
  int sync() override;
  std::streamsize xsputn(const char_type *, std::streamsize) override;
  int overflow(int_type) override;

  file_descriptor fd_;
  ring_buffer buf_;
  std::size_t read_index_;
  std::size_t write_index_;
};

} // namespace redis::io

template <typename Function, typename... Args>
redis::io::file_descriptor::file_descriptor(Function function, Args... args)
    : value_(posix_call(function, std::forward<Args>(args)...)) {}

template <typename... Args>
redis::io::memory_map::memory_map(void *ptr, std::size_t len, Args... args)
    : ptr_(posix_call(::mmap, ptr, len, args...)), len_(len) {}

#endif // REDIS_SERVER_IO_HPP
