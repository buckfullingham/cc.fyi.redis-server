#include "command_handler.hpp"
#include "commands.hpp"
#include "database.hpp"
#include "io.hpp"
#include "resp.hpp"

#include <array>
#include <iostream>
#include <list>

#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace {

template <typename T>
void set_socket_option(int fd, int level, int opt_name, T opt_val) {
  redis::io::posix_call(::setsockopt, fd, level, opt_name, &opt_val,
                        sizeof(opt_val));
}

template <typename T>
auto bind(int fd, T address)
    -> decltype(::bind(fd, reinterpret_cast<sockaddr *>(&address),
                       sizeof(address)),
                void()) {
  redis::io::posix_call(::bind, fd, reinterpret_cast<sockaddr *>(&address),
                        sizeof(address));
}

void epoll_add(int epollfd, int fd, decltype(::epoll_event::events) events,
               decltype(::epoll_event::data) data) {
  ::epoll_event ev{};
  ev.events = events;
  ev.data = data;
  redis::io::posix_call(::epoll_ctl, epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void epoll_del(int epollfd, int fd) {
  ::epoll_event ev{};
  redis::io::posix_call(::epoll_ctl, epollfd, EPOLL_CTL_DEL, fd, &ev);
}

int fcntl_get_flags(int fd) {
  return redis::io::posix_call(::fcntl, fd, F_GETFL, 0);
}

void fcntl_set_flags(int fd, int flags) {
  redis::io::posix_call(::fcntl, fd, F_SETFL, fcntl_get_flags(fd) | flags);
}

void install_sig_handlers() {
  // don't want to use send(), don't want to exit on SIGPIPE
  struct sigaction sa {};
  sa.sa_handler = SIG_IGN;
  ::sigaction(SIGPIPE, &sa, nullptr);
}

class client {
public:
  explicit client(redis::io::file_descriptor fd, redis::database &dict)
      : in_fd_(std::move(fd)), dict_(dict) {
    set_socket_option(in_fd_.value(), SOL_SOCKET, SO_SNDBUF, 1 << 20);
  }

  client(const client &) = delete;
  client &operator=(const client &) = delete;

  void on_readable() {
    for (;;) {
      const auto len = in_.size() - (in_write_index_ - in_read_index_);

      if (len == 0)
        throw std::runtime_error("input buffer overflow");

      const auto n = ::read(in_fd_.value(), in_.addr(in_write_index_), len);

      switch (n) {
      case -1:
        if (errno == EINTR) {
          continue;
        } else if (errno == EWOULDBLOCK) {
          ostream_.flush();
          return;
        } else
          throw std::system_error(errno, std::generic_category());
      case 0:
        throw std::runtime_error("socket hung up");
      default:
        in_write_index_ += n;
      }

      const char *const begin = in_.addr(in_read_index_);
      const char *const end =
          in_.addr(in_read_index_) + (in_write_index_ - in_read_index_);

      in_read_index_ += parser_.parse(begin, end) - begin;

      if (ostream_.bad())
        throw std::runtime_error("slow consumer");

      if (n < len) {
        ostream_.flush();
        return;
      }
    }
  }

  [[nodiscard]] int fd() const { return in_fd_.value(); }

public:
  redis::io::file_descriptor in_fd_;
  redis::database &dict_;
  redis::io::ring_buffer in_{1 << 13};
  std::size_t in_read_index_{};
  std::size_t in_write_index_{};
  redis::io::ofstreambuf ofstreambuf_{
      redis::io::file_descriptor{::dup, in_fd_.value()}, 1 << 13};
  std::ostream ostream_{&ofstreambuf_};
  redis::resp::writer writer_{ostream_};
  redis::command_handler server_{dict_, writer_};
  redis::resp::parser parser_{server_};
};

void load(redis::database &db) {
  redis::resp::null_handler null_handler;
  redis_cmd_load({"load"}, db, null_handler);
}

} // namespace

int main(int, char *[]) {
  namespace ns = redis;
  using ns::io::posix_call;

  install_sig_handlers();

  ns::io::file_descriptor epollfd(::epoll_create, 1);

  sockaddr_in listen_address{
      .sin_family = AF_INET,
      .sin_port = htons(6379),
  };

  listen_address.sin_addr.s_addr = INADDR_ANY;

  ns::io::file_descriptor sockfd(::socket, AF_INET, SOCK_STREAM, 0);
  fcntl_set_flags(sockfd.value(), O_NONBLOCK);

  set_socket_option(sockfd.value(), SOL_SOCKET, SO_REUSEADDR, 1);

  bind(sockfd.value(), listen_address);

  posix_call(::listen, sockfd.value(), 128);

  epoll_add(epollfd.value(), sockfd.value(), EPOLLIN, {});

  redis::database db;

  std::list<client> clients;
  std::array<epoll_event, 128> events{};

  for (;;) {
    auto n = TEMP_FAILURE_RETRY(
        ::epoll_wait(epollfd.value(), events.begin(), events.size(), -1));
    if (n == -1 && errno != ETIMEDOUT)
      throw std::system_error(errno, std::generic_category());
    for (auto &event : std::span(events.begin(), events.begin() + n)) {
      if (!event.data.ptr) {
        ns::io::file_descriptor clientfd(::accept, sockfd.value(), nullptr,
                                         nullptr);
        fcntl_set_flags(clientfd.value(), O_NONBLOCK);
        clients.emplace_back(std::move(clientfd), db);
        epoll_add(epollfd.value(), clients.back().fd(), EPOLLIN | EPOLLET,
                  {.ptr = &clients.back()});
      } else if (event.events & EPOLLIN | EPOLLHUP | EPOLLERR) {
        try {
          static_cast<client *>(event.data.ptr)->on_readable();
        } catch (const std::exception &e) {
          auto pos = std::find_if(
              clients.begin(), clients.end(), [&event](const auto &c) {
                return &c == static_cast<const client *>(event.data.ptr);
              });
          if (pos != clients.end()) {
            epoll_del(epollfd.value(), pos->fd());
            clients.erase(pos);
          }
        }
      }
    }
  }
}
