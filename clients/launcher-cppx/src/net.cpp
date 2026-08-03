#include "net.h"

#include "updatersha256.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

extern char **environ;

namespace launcher {

int tcp_ping(const std::string &host, int port, int timeout_ms) {
  char portstr[16];
  snprintf(portstr, sizeof(portstr), "%d", port);

  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0 || !res)
    return -1;

  timeval start;
  gettimeofday(&start, nullptr);

  int result_ms = -1;
  for (addrinfo *ai = res; ai; ai = ai->ai_next) {
    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
      continue;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
    bool connected = false;
    if (rc == 0) {
      connected = true;
    } else if (errno == EINPROGRESS) {
      fd_set wset;
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      if (select(fd + 1, nullptr, &wset, nullptr, &tv) > 0) {
        int soerr = 0;
        socklen_t len = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) == 0 && soerr == 0)
          connected = true;
      }
    }
    close(fd);

    if (connected) {
      timeval end;
      gettimeofday(&end, nullptr);
      long ms = (end.tv_sec - start.tv_sec) * 1000 +
                (end.tv_usec - start.tv_usec) / 1000;
      result_ms = (int)(ms < 0 ? 0 : ms);
      break;
    }
  }
  freeaddrinfo(res);
  return result_ms;
}

bool spawn_detached(const std::string &binary, const std::vector<std::string> &args) {
  if (!is_executable_file(binary))
    return false;

  std::vector<char *> argv;
  argv.push_back(const_cast<char *>(binary.c_str()));
  for (const auto &a : args)
    argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(nullptr);

  pid_t pid = 0;
  int rc = posix_spawn(&pid, binary.c_str(), nullptr, nullptr, argv.data(), environ);
  if (rc != 0) {
    fprintf(stderr, "[launcher] posix_spawn(%s) failed: %s\n", binary.c_str(), strerror(rc));
    return false;
  }
  // Detached: never waitpid. The child is reparented to launchd when we exit.
  return true;
}

std::string sha256_file_hex(const std::string &path) {
  FILE *fp = fopen(path.c_str(), "rb");
  if (!fp)
    return "";
  SHA256 h;
  uint8_t buf[8192];
  for (;;) {
    size_t n = fread(buf, 1, sizeof(buf), fp);
    if (n == 0)
      break;
    h.Update(buf, n);
  }
  fclose(fp);
  uint8_t out[32];
  h.Final(out);
  static const char *hex = "0123456789abcdef";
  std::string s;
  s.reserve(64);
  for (int i = 0; i < 32; ++i) {
    s += hex[out[i] >> 4];
    s += hex[out[i] & 0xf];
  }
  return s;
}

bool is_executable_file(const std::string &path) {
  if (path.empty())
    return false;
  struct stat st;
  if (stat(path.c_str(), &st) != 0)
    return false;
  return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
}

} // namespace launcher
