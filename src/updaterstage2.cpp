#include "updaterstage2.h"
#include "updaterzip.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace {

std::string TempDir() {
#ifdef _WIN32
    const char *t = getenv("TEMP");
    return t ? t : "C:\\Temp";
#else
    return "/tmp";
#endif
}

std::string LogPath() {
#ifdef _WIN32
    return TempDir() + "\\zsilencer-update.log";
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    std::string dir = std::string(home ? home : "/tmp") + "/Library/Logs/zSILENCER";
    mkdir(dir.c_str(), 0755);
    return dir + "/update.log";
#else
    return "/tmp/zsilencer-update.log";
#endif
}

void Logf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[stage2] %s\n", buf);
    FILE *fp = fopen(LogPath().c_str(), "a");
    if (fp) { fprintf(fp, "[stage2] %s\n", buf); fclose(fp); }
}

bool CopyFile_(const std::string &from, const std::string &to) {
    FILE *in = fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE *out = fopen(to.c_str(), "wb");
    if (!out) { fclose(in); return false; }
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return false; }
    }
    fclose(in); fclose(out);
#ifndef _WIN32
    chmod(to.c_str(), 0755);
#endif
    return true;
}

std::string MySelfPath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    return buf;
#elif defined(__APPLE__)
    char buf[1024];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) return buf;
    return "";
#else
    char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = 0; return buf; }
    return "";
#endif
}

// On macOS, if the binary is inside <Name>.app/Contents/MacOS/, the
// "install unit" is the .app bundle. Otherwise, it's the directory
// containing the binary.
std::string ResolveInstallDir(const std::string &exe) {
    size_t slash = exe.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    std::string parent = exe.substr(0, slash);
#ifdef __APPLE__
    const std::string want = "/Contents/MacOS";
    if (parent.size() >= want.size() &&
        parent.compare(parent.size() - want.size(), want.size(), want) == 0) {
        return parent.substr(0, parent.size() - want.size());
    }
#endif
    return parent;
}

bool WaitForPidExit(int pid, int timeout_ms) {
    int waited = 0;
    while (waited < timeout_ms) {
#ifdef _WIN32
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!h) return true;
        DWORD r = WaitForSingleObject(h, 100);
        CloseHandle(h);
        if (r == WAIT_OBJECT_0) return true;
#else
        if (kill(pid, 0) != 0 && errno == ESRCH) return true;
        usleep(100 * 1000);
#endif
        waited += 100;
    }
    return false;
}

bool RenameDir(const std::string &src, const std::string &dst) {
#ifdef _WIN32
    return MoveFileA(src.c_str(), dst.c_str()) != 0;
#else
    return rename(src.c_str(), dst.c_str()) == 0;
#endif
}

} // namespace

namespace UpdaterStage2 {

int Run(int argc, char **argv) {
    std::string zip, install_dir, exe_to_relaunch;
    int parent_pid = 0;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if      (a.rfind("--zip=",         0) == 0) zip            = a.substr(6);
        else if (a.rfind("--install-dir=", 0) == 0) install_dir    = a.substr(14);
        else if (a.rfind("--pid=",         0) == 0) parent_pid     = atoi(a.c_str() + 6);
        else if (a.rfind("--relaunch=",    0) == 0) exe_to_relaunch = a.substr(11);
    }

    Logf("start: zip=%s install=%s pid=%d relaunch=%s",
        zip.c_str(), install_dir.c_str(), parent_pid, exe_to_relaunch.c_str());

    if (zip.empty() || install_dir.empty() || parent_pid == 0) {
        Logf("missing args");
        return 1;
    }

    if (!WaitForPidExit(parent_pid, 10000)) {
        Logf("parent %d still alive after 10s; proceeding anyway", parent_pid);
    }

    // Extract to <install_dir>.new (sibling).
    std::string staging = install_dir + ".new";
#ifdef _WIN32
    RemoveDirectoryA(staging.c_str());
    _mkdir(staging.c_str());
#else
    mkdir(staging.c_str(), 0755);
#endif

    UpdaterZip::Result zr = UpdaterZip::Extract(zip, staging);
    if (zr != UpdaterZip::OK) {
        Logf("extract failed: %d", (int)zr);
        if (!exe_to_relaunch.empty()) {
            Logf("relaunching old exe: %s", exe_to_relaunch.c_str());
#ifdef _WIN32
            STARTUPINFOA si{}; si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            CreateProcessA(exe_to_relaunch.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#else
            if (fork() == 0) execl(exe_to_relaunch.c_str(), exe_to_relaunch.c_str(), (char*)nullptr);
#endif
        }
        return 2;
    }

    std::string old_path = install_dir + ".old";
#ifdef _WIN32
    RemoveDirectoryA(old_path.c_str());
#endif
    if (!RenameDir(install_dir, old_path)) {
        Logf("rename install→old failed");
        return 3;
    }
    if (!RenameDir(staging, install_dir)) {
        Logf("rename new→install failed; rolling back");
        RenameDir(old_path, install_dir);
        return 4;
    }

    std::string new_exe = exe_to_relaunch;
    Logf("relaunching: %s", new_exe.c_str());
#ifdef _WIN32
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(new_exe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        Logf("CreateProcess failed: %lu", GetLastError());
        return 5;
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return 0;
#else
    if (fork() == 0) {
        execl(new_exe.c_str(), new_exe.c_str(), (char*)nullptr);
        Logf("execl failed: %s", strerror(errno));
        _exit(99);
    }
    return 0;
#endif
}

void Launch(const std::string &zippath) {
    std::string self = MySelfPath();
    std::string install = ResolveInstallDir(self);
    std::string temp = TempDir() +
#ifdef _WIN32
        "\\zsilencer-stage2.exe";
#else
        "/zsilencer-stage2";
#endif

    if (!CopyFile_(self, temp)) {
        Logf("copy self → %s failed", temp.c_str());
        exit(1);
    }

#ifdef _WIN32
    int pid = (int)GetCurrentProcessId();
#else
    int pid = (int)getpid();
#endif
    char pidarg[64], ziparg[512], instarg[1024], relarg[1024];
    snprintf(pidarg,  sizeof(pidarg),  "--pid=%d", pid);
    snprintf(ziparg,  sizeof(ziparg),  "--zip=%s", zippath.c_str());
    snprintf(instarg, sizeof(instarg), "--install-dir=%s", install.c_str());
    snprintf(relarg,  sizeof(relarg),  "--relaunch=%s", self.c_str());

    fprintf(stderr, "[updater] launching stage2: %s %s %s %s %s\n",
        temp.c_str(), ziparg, instarg, pidarg, relarg);

#ifdef _WIN32
    std::string cmdline = "\"" + temp + "\" --self-update-stage2 " +
        ziparg + " " + instarg + " " + pidarg + " " + relarg;
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    CreateProcessA(NULL, (LPSTR)cmdline.c_str(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    exit(0);
#else
    pid_t f = fork();
    if (f == 0) {
        execl(temp.c_str(), temp.c_str(), "--self-update-stage2",
              ziparg, instarg, pidarg, relarg, (char*)nullptr);
        _exit(99);
    }
    exit(0);
#endif
}

} // namespace UpdaterStage2
