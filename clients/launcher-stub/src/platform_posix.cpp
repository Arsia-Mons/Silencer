// Shared macOS/Linux backend: fork/exec processes and HTTP via libcurl
// resolved with dlopen. dlopen (not a link-time dependency) so a machine
// without libcurl still runs the stub - the update check degrades to "no
// update" and the existing payload launches, which is the contract.

#include "stub.h"

#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

std::string stub::env_str(const char *name) {
  const char *v = getenv(name);
  return v ? v : "";
}

std::string stub::own_exe_path() {
#ifdef __APPLE__
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) != 0)
    return "";
  char real[PATH_MAX];
  if (realpath(buf, real))
    return real;
  return buf;
#else
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0)
    return "";
  buf[n] = 0;
  return buf;
#endif
}

// ---------------- processes ----------------
namespace {

pid_t spawn_argv(const std::vector<std::string> &argv, const char *workdir) {
  std::vector<char *> av;
  for (auto &a : argv)
    av.push_back(const_cast<char *>(a.c_str()));
  av.push_back(nullptr);
  pid_t pid = fork();
  if (pid == 0) {
    if (workdir && *workdir && chdir(workdir) != 0)
      _exit(127);
    execvp(av[0], av.data());
    _exit(127);
  }
  return pid;
}

} // namespace

bool stub::spawn(const std::string &exe, const std::string &workdir, Child *out) {
  pid_t pid = spawn_argv({exe}, workdir.c_str());
  if (pid <= 0)
    return false;
  out->handle = pid;
  return true;
}

int stub::try_wait(const Child &c, int timeout_ms) {
  if (!c.valid())
    return -1;
  pid_t pid = (pid_t)c.handle;
  for (int waited = 0;; waited += 30) {
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid) {
      if (WIFEXITED(status))
        return WEXITSTATUS(status);
      if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
      return 1;
    }
    if (r < 0 || waited >= timeout_ms)
      return -1;
    usleep(30 * 1000);
  }
}

bool stub::run_tool(const std::vector<std::string> &argv) {
  pid_t pid = spawn_argv(argv, nullptr);
  if (pid <= 0)
    return false;
  int status = 0;
  if (waitpid(pid, &status, 0) != pid)
    return false;
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// ---------------- HTTP via dlopen'd libcurl ----------------
namespace {

// ABI-stable libcurl option values (from curl.h; the .so.4 ABI freezes them).
typedef void CURL;
constexpr long kOptWriteData = 10001;
constexpr long kOptUrl = 10002;
constexpr long kOptUserAgent = 10018;
constexpr long kOptLowSpeedLimit = 19;
constexpr long kOptLowSpeedTime = 20;
constexpr long kOptNoProgress = 43;
constexpr long kOptFailOnError = 45;
constexpr long kOptFollowLocation = 52;
constexpr long kOptTimeoutMs = 155;
constexpr long kOptConnectTimeoutMs = 156;
constexpr long kOptWriteFunction = 20011;
constexpr long kOptXferInfoFunction = 20219;
constexpr long kOptXferInfoData = 10057;

struct Curl {
  void *lib = nullptr;
  CURL *(*easy_init)() = nullptr;
  long (*easy_setopt)(CURL *, long, ...) = nullptr;
  long (*easy_perform)(CURL *) = nullptr;
  void (*easy_cleanup)(CURL *) = nullptr;
  const char *(*easy_strerror)(long) = nullptr;
  bool ok() const { return easy_init && easy_setopt && easy_perform && easy_cleanup; }
};

Curl &curl() {
  static Curl c = [] {
    Curl c;
#ifdef __APPLE__
    c.lib = dlopen("/usr/lib/libcurl.4.dylib", RTLD_LAZY);
#else
    c.lib = dlopen("libcurl.so.4", RTLD_LAZY);
    if (!c.lib)
      c.lib = dlopen("libcurl.so", RTLD_LAZY);
#endif
    if (!c.lib)
      return c;
    long (*global_init)(long) = (long (*)(long))dlsym(c.lib, "curl_global_init");
    if (global_init)
      global_init(3 /* CURL_GLOBAL_DEFAULT */);
    c.easy_init = (CURL * (*)()) dlsym(c.lib, "curl_easy_init");
    c.easy_setopt = (long (*)(CURL *, long, ...))dlsym(c.lib, "curl_easy_setopt");
    c.easy_perform = (long (*)(CURL *))dlsym(c.lib, "curl_easy_perform");
    c.easy_cleanup = (void (*)(CURL *))dlsym(c.lib, "curl_easy_cleanup");
    c.easy_strerror = (const char *(*)(long))dlsym(c.lib, "curl_easy_strerror");
    return c;
  }();
  return c;
}

struct Xfer {
  FILE *file = nullptr;
  std::string *mem = nullptr;
  const stub::DlProgress *progress = nullptr;
  bool write_failed = false;
};

size_t write_cb(char *ptr, size_t size, size_t nmemb, void *ud) {
  Xfer *x = (Xfer *)ud;
  size_t n = size * nmemb;
  if (x->mem)
    x->mem->append(ptr, n);
  if (x->file && fwrite(ptr, 1, n, x->file) != n) {
    x->write_failed = true;
    return 0;
  }
  return n;
}

int xferinfo_cb(void *ud, long long dltotal, long long dlnow, long long, long long) {
  Xfer *x = (Xfer *)ud;
  if (x->progress && !(*x->progress)((uint64_t)dlnow, (uint64_t)dltotal))
    return 1;
  return 0;
}

stub::HttpResult fetch(const std::string &url, int timeout_ms, Xfer &x) {
  stub::HttpResult res;
  Curl &c = curl();
  if (!c.ok()) {
    res.error = "libcurl unavailable";
    return res;
  }
  CURL *h = c.easy_init();
  if (!h) {
    res.error = "curl init failed";
    return res;
  }
  c.easy_setopt(h, kOptUrl, url.c_str());
  c.easy_setopt(h, kOptFollowLocation, 1L);
  c.easy_setopt(h, kOptFailOnError, 1L);
  c.easy_setopt(h, kOptUserAgent, "silencer-launcher-stub");
  c.easy_setopt(h, kOptWriteFunction, (void *)write_cb);
  c.easy_setopt(h, kOptWriteData, &x);
  c.easy_setopt(h, kOptConnectTimeoutMs, (long)(timeout_ms > 0 ? timeout_ms : 15000));
  if (timeout_ms > 0) {
    c.easy_setopt(h, kOptTimeoutMs, (long)timeout_ms);
  } else {
    c.easy_setopt(h, kOptLowSpeedLimit, 1L); // <1 B/s for 30s = stalled
    c.easy_setopt(h, kOptLowSpeedTime, 30L);
  }
  if (x.progress) {
    c.easy_setopt(h, kOptNoProgress, 0L);
    c.easy_setopt(h, kOptXferInfoFunction, (void *)xferinfo_cb);
    c.easy_setopt(h, kOptXferInfoData, &x);
  }
  long rc = c.easy_perform(h);
  if (rc != 0)
    res.error = c.easy_strerror ? c.easy_strerror(rc)
                                : "curl error " + std::to_string(rc);
  else
    res.ok = true;
  if (x.write_failed) {
    res.ok = false;
    res.error = "write failed";
  }
  c.easy_cleanup(h);
  return res;
}

} // namespace

stub::HttpResult stub::http_get_text(const std::string &url, int timeout_ms,
                                     std::string *out) {
  out->clear();
  Xfer x;
  x.mem = out;
  return fetch(url, timeout_ms, x);
}

stub::HttpResult stub::http_download(const std::string &url, const std::string &dest,
                                     const DlProgress &progress) {
  FILE *f = fopen(dest.c_str(), "wb");
  if (!f)
    return {false, "cannot open " + dest};
  Xfer x;
  x.file = f;
  x.progress = &progress;
  HttpResult r = fetch(url, 0, x);
  fclose(f);
  return r;
}
