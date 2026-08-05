#ifndef LAUNCHER_NET_H
#define LAUNCHER_NET_H

#include <string>
#include <vector>

namespace launcher {

// TCP connect-time latency to host:port. Returns milliseconds on success, or
// -1 if the connection failed or did not complete within timeout_ms (offline).
int tcp_ping(const std::string &host, int port, int timeout_ms);

// Launch `binary` detached with `args` (argv[1..]). Returns false if the spawn
// failed (e.g. binary missing/not executable). Does not wait for the child.
bool spawn_detached(const std::string &binary, const std::vector<std::string> &args);

// Lowercase hex SHA-256 of a file's contents, or "" on read failure.
std::string sha256_file_hex(const std::string &path);

// True if `path` is a regular file with the executable bit set (Play gate).
bool is_executable_file(const std::string &path);

} // namespace launcher

#endif
