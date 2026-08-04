#include "net.h"

#include "updatersha256.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

extern char **environ;
#endif

namespace launcher {

#ifdef _WIN32

int tcp_ping(const std::string &host, int port, int timeout_ms) {
  static bool wsa_ready = [] {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
  }();
  if (!wsa_ready)
    return -1;

  char portstr[16];
  snprintf(portstr, sizeof(portstr), "%d", port);

  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0 || !res)
    return -1;

  ULONGLONG start = GetTickCount64();

  int result_ms = -1;
  for (addrinfo *ai = res; ai; ai = ai->ai_next) {
    SOCKET fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == INVALID_SOCKET)
      continue;

    u_long nonblock = 1;
    ioctlsocket(fd, FIONBIO, &nonblock);

    int rc = connect(fd, ai->ai_addr, (int)ai->ai_addrlen);
    bool connected = false;
    if (rc == 0) {
      connected = true;
    } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
      fd_set wset;
      FD_ZERO(&wset);
      FD_SET(fd, &wset);
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      if (select(0, nullptr, &wset, nullptr, &tv) > 0) {
        int soerr = 0;
        int len = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&soerr, &len) == 0 && soerr == 0)
          connected = true;
      }
    }
    closesocket(fd);

    if (connected) {
      ULONGLONG ms = GetTickCount64() - start;
      result_ms = (int)ms;
      break;
    }
  }
  freeaddrinfo(res);
  return result_ms;
}

bool spawn_detached(const std::string &binary, const std::vector<std::string> &args) {
  if (!is_executable_file(binary))
    return false;

  // CreateProcess takes one command line; quote each argument.
  std::string cmdline = "\"" + binary + "\"";
  for (const auto &a : args)
    cmdline += " \"" + a + "\"";

  STARTUPINFOA si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(binary.c_str(), &cmdline[0], nullptr, nullptr, FALSE,
                      CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, nullptr, nullptr,
                      &si, &pi)) {
    fprintf(stderr, "[launcher] CreateProcess(%s) failed: %lu\n", binary.c_str(),
            GetLastError());
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

#else // !_WIN32

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

#endif // _WIN32

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
#ifdef _WIN32
  // No execute bit on Windows: a regular file is launchable.
  DWORD attrs = GetFileAttributesA(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  if (stat(path.c_str(), &st) != 0)
    return false;
  return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
#endif
}

} // namespace launcher
