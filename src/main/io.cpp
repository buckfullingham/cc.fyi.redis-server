#include "io.hpp"

#include <cassert>
#include <utility>

#include <sys/socket.h>

namespace ns = redis::io;

ns::file_descriptor::file_descriptor(file_descriptor &&that) noexcept
    : value_(-1) {
  swap(*this, that);
}

ns::file_descriptor &ns::file_descriptor::file_descriptor::operator=(
    file_descriptor &&that) noexcept {
  reset();
  swap(*this, that);
  return *this;
}

int ns::file_descriptor::release() noexcept {
  return std::exchange(value_, -1);
}

void ns::file_descriptor::reset() noexcept {
  if (auto released = release(); released != -1)
    ::close(released);
}

ns::file_descriptor::~file_descriptor() noexcept { reset(); }

[[nodiscard]] int ns::file_descriptor::value() const noexcept { return value_; }

inline void ns::swap(ns::file_descriptor &lhs,
                     ns::file_descriptor &rhs) noexcept {
  using std::swap;
  swap(lhs.value_, rhs.value_);
}

ns::memory_map::memory_map(memory_map &&that) noexcept : ptr_(), len_() {
  swap(*this, that);
}

ns::memory_map &ns::memory_map::operator=(memory_map &&that) noexcept {
  reset();
  swap(*this, that);
  return *this;
}

[[nodiscard]] std::tuple<void *, std::size_t>
ns::memory_map::value() const noexcept {
  return std::make_tuple(ptr_, len_);
}

std::tuple<void *, std::size_t> ns::memory_map::release() noexcept {
  auto result = value();
  ptr_ = nullptr;
  len_ = 0;
  return result;
}

void ns::memory_map::reset() noexcept {
  auto [ptr, len] = release();
  if (ptr)
    ::munmap(ptr, len);
}

ns::memory_map::~memory_map() noexcept { reset(); }

void ns::swap(memory_map &lhs, memory_map &rhs) noexcept {
  using std::swap;
  swap(lhs.ptr_, rhs.ptr_);
  swap(lhs.len_, rhs.len_);
}

namespace {

std::size_t validated_len(std::size_t len) {
  if (!len || len % sysconf(_SC_PAGESIZE) != 0)
    throw std::runtime_error("bad size");
  return len;
}

ns::file_descriptor make_memfd(std::size_t len) {
  ns::file_descriptor result(::memfd_create, "redis::io::ring_buffer", 0);
  ns::posix_call(::ftruncate, result.value(), len);
  return result;
}

ns::memory_map make_region(const ns::file_descriptor &fd, std::size_t len) {
  return {nullptr, 2 * len, PROT_READ | PROT_WRITE, MAP_SHARED, fd.value(), 0};
}

} // namespace

/**
 * Map a region in memory twice the length of the buffer then remap the second
 * half to mirror the first half.
 * @param len
 */
ns::ring_buffer::ring_buffer(std::size_t len)
    : len_(validated_len(len)), fd_(make_memfd(len_)),
      region_(make_region(fd_, len_)),
      ptr_(static_cast<char *>(std::get<0>(region_.value()))) {
  posix_call(::mmap, ptr_ + len_, len_, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_FIXED, fd_.value(), 0);
}

ns::ofstreambuf::ofstreambuf(file_descriptor fd, std::size_t size)
    : fd_(std::move(fd)), buf_(size), read_index_(), write_index_() {}

int ns::ofstreambuf::sync() {
  auto len = write_index_ - read_index_;
  if (len == 0)
    return 0;
  int result{};
  TEMP_FAILURE_RETRY(result =
                         ::write(fd_.value(), buf_.addr(read_index_), len));
  if (result != len)
    return EOF;
  read_index_ += len;
  return 0;
}

std::streamsize ns::ofstreambuf::xsputn(const char_type *s,
                                        const std::streamsize n_) {
  assert(n_ >= 0);
  for (std::size_t n = n_; n > 0;) {
    const std::size_t len =
        std::min(buf_.size() - (write_index_ - read_index_), n);
    std::copy(s, s + len, buf_.addr(write_index_));
    s += len;
    write_index_ += len;
    n -= std::streamsize(len);
    if (n && sync() == EOF)
      return EOF;
  }
  return n_;
}

int ns::ofstreambuf::overflow(int_type ch) {
  if (ch != EOF) {
    sync();
    auto c = static_cast<char>(static_cast<unsigned char>(ch));
    return xsputn(&c, 1) == EOF ? EOF : 0;
  }
  return 0;
}
