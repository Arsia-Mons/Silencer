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
#include <dirent.h>
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
    return TempDir() + "\\silencer-update.log";
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    std::string dir = std::string(home ? home : "/tmp") + "/Library/Logs/Silencer";
    mkdir(dir.c_str(), 0755);
    return dir + "/update.log";
#else
    return "/tmp/silencer-update.log";
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
    if (MoveFileA(src.c_str(), dst.c_str()) != 0) return true;
    fprintf(stderr, "[stage2] MoveFileA %s -> %s failed: %lu\n",
        src.c_str(), dst.c_str(), GetLastError());
    return false;
#else
    if (rename(src.c_str(), dst.c_str()) == 0) return true;
    fprintf(stderr, "[stage2] rename %s -> %s failed: %s\n",
        src.c_str(), dst.c_str(), strerror(errno));
    return false;
#endif
}

// Best-effort recursive delete. Used to strip `__MACOSX/` siblings that
// ditto --sequesterRsrc bakes alongside the real bundle — leaving them
// in place would break FindSingleTopDir (sees 2 entries) and pollute the
// installed app.
void RemoveDirRecursive(const std::string &path) {
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    std::string pattern = path + "\\*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        std::string child = path + "\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirRecursive(child);
        } else {
            DeleteFileA(child.c_str());
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    RemoveDirectoryA(path.c_str());
#else
    DIR *d = opendir(path.c_str());
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string child = path + "/" + n;
        struct stat st;
        if (lstat(child.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            RemoveDirRecursive(child);
        } else {
            unlink(child.c_str());
        }
    }
    closedir(d);
    rmdir(path.c_str());
#endif
}

// Production release zips wrap their contents in a single top-level
// directory: on macOS ditto --keepParent produces `Silencer.app/`, on
// Windows Compress-Archive of `build/package/silencer` produces
// `silencer/`. After extracting to staging/, we need to hoist that
// inner dir into place — otherwise the atomic rename puts the install
// one level too deep and breaks the bundle / relaunch path.
//
// Returns the wrapper entry name if `dir` contains exactly one entry
// and that entry is a directory, else "".
std::string FindSingleTopDir(const std::string &dir) {
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    std::string pattern = dir + "\\*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "";
    int count = 0;
    std::string name;
    bool is_dir = false;
    do {
        std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        count++;
        if (count > 1) { FindClose(h); return ""; }
        name = n;
        is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return (count == 1 && is_dir) ? name : std::string();
#else
    DIR *d = opendir(dir.c_str());
    if (!d) return "";
    int count = 0;
    std::string name;
    bool is_dir = false;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        count++;
        if (count > 1) { closedir(d); return ""; }
        name = n;
        struct stat st;
        if (stat((dir + "/" + n).c_str(), &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        }
    }
    closedir(d);
    return (count == 1 && is_dir) ? name : std::string();
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

    // Extract to <install_dir>.new (sibling). Remove any leftover staging
    // or rollback dirs from a previous failed stage-2 run — otherwise the
    // rename-install-to-old step below hits ENOTEMPTY and the user is stuck
    // until they manually clean up the install dir.
    std::string staging = install_dir + ".new";
    RemoveDirRecursive(staging);
    RemoveDirRecursive(install_dir + ".old");
#ifdef _WIN32
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

    // ditto --sequesterRsrc emits a sibling `__MACOSX/` directory of
    // AppleDouble files next to the real bundle. We don't want any of
    // that in the installed app, and leaving it in place fools
    // FindSingleTopDir into thinking the zip has multiple top entries.
    RemoveDirRecursive(staging + "/__MACOSX");

    // Unwrap single-top-dir zips (ditto --keepParent on macOS,
    // Compress-Archive of a staging dir on Windows). If the zip had a
    // single top-level directory, the "new install" is that inner dir,
    // not the staging container.
    std::string wrapper = FindSingleTopDir(staging);
    std::string effective_new = wrapper.empty()
        ? staging
        : staging + "/" + wrapper;
    if (!wrapper.empty()) {
        Logf("detected zip wrapper dir '%s'; hoisting contents", wrapper.c_str());
    }

    std::string old_path = install_dir + ".old";
#ifdef _WIN32
    RemoveDirectoryA(old_path.c_str());
#endif
    if (!RenameDir(install_dir, old_path)) {
        Logf("rename install→old failed");
        return 3;
    }
    if (!RenameDir(effective_new, install_dir)) {
        Logf("rename new→install failed; rolling back");
        RenameDir(old_path, install_dir);
        return 4;
    }
    // If we unwrapped, the staging container is now empty; best-effort remove.
    if (!wrapper.empty()) {
#ifdef _WIN32
        RemoveDirectoryA(staging.c_str());
#else
        rmdir(staging.c_str());
#endif
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

bool Launch(const std::string &zippath) {
    std::string self = MySelfPath();
    std::string install = ResolveInstallDir(self);
    std::string tempdir = TempDir();

#ifdef _WIN32
    std::string temp = tempdir + "\\silencer-stage2.exe";
    if (!CopyFile_(self, temp)) {
        Logf("copy self → %s failed", temp.c_str());
        return false;
    }
    // Windows resolves the main exe's import table from the exe's own
    // directory before our code runs. Since stage-2 lives in %TEMP% but
    // its DLLs (zlib1, SDL2, libcurl, etc.) live in the install dir, the
    // loader fails with "zlib1.dll was not found". Copy every *.dll from
    // the install dir next to silencer-stage2.exe so imports resolve.
    {
        std::string pattern = install + "\\*.dll";
        WIN32_FIND_DATAA fd{};
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string from = install + "\\" + fd.cFileName;
                std::string to = tempdir + "\\" + fd.cFileName;
                if (!CopyFile_(from, to)) {
                    Logf("copy dll %s -> %s failed", from.c_str(), to.c_str());
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        } else {
            Logf("no DLLs found at %s (FindFirstFile err=%lu); stage-2 may fail to load",
                pattern.c_str(), GetLastError());
        }
    }
#elif defined(__APPLE__)
    // The shipped binary is signed with hardened runtime + library
    // validation and references its dylibs via @rpath, with LC_RPATH set
    // to @executable_path/../Frameworks. A flat copy at /tmp/silencer-stage2
    // resolves @executable_path to /tmp, so dyld looks in /Frameworks/ and
    // bails before main() runs — silently, because hardened runtime
    // suppresses dyld errors when no TTY is attached. Mirror just enough
    // bundle structure under /tmp that Frameworks/ sits next to the binary.
    std::string stage2_bundle = tempdir + "/silencer-stage2.app";
    std::string stage2_contents = stage2_bundle + "/Contents";
    std::string stage2_macos = stage2_contents + "/MacOS";
    std::string stage2_fw = stage2_contents + "/Frameworks";
    RemoveDirRecursive(stage2_bundle);
    mkdir(stage2_bundle.c_str(), 0755);
    mkdir(stage2_contents.c_str(), 0755);
    mkdir(stage2_macos.c_str(), 0755);
    mkdir(stage2_fw.c_str(), 0755);

    std::string temp = stage2_macos + "/silencer-stage2";
    if (!CopyFile_(self, temp)) {
        Logf("copy self → %s failed", temp.c_str());
        return false;
    }

    // Mirror Frameworks/ from the source bundle so @rpath dylib refs
    // (libSDL3, libSDL3_mixer, libminizip, …) resolve. Local dev builds
    // link against absolute /opt/homebrew paths and have an empty (or
    // missing) Frameworks/ — that's fine, the absolute paths resolve
    // straight from /tmp without help.
    std::string src_fw = install + "/Contents/Frameworks";
    DIR *fwd = opendir(src_fw.c_str());
    if (fwd) {
        struct dirent *e;
        while ((e = readdir(fwd)) != NULL) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            std::string from = src_fw + "/" + n;
            struct stat st;
            if (lstat(from.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                Logf("skipping framework dir %s (flat-dylib layout expected)", n.c_str());
                continue;
            }
            std::string to = stage2_fw + "/" + n;
            if (!CopyFile_(from, to)) {
                Logf("copy framework %s -> %s failed", from.c_str(), to.c_str());
            }
        }
        closedir(fwd);
    } else {
        Logf("no Frameworks/ at %s; assuming dev build with absolute dylib paths",
            src_fw.c_str());
    }
#else
    std::string temp = tempdir + "/silencer-stage2";
    if (!CopyFile_(self, temp)) {
        Logf("copy self → %s failed", temp.c_str());
        return false;
    }
#endif

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
    // CreateProcessA feeds one string to MSVCRT's argv tokenizer, which
    // splits on unquoted whitespace. Paths like "C:\Users\Space Command\..."
    // split in half — seen in stage-2 logs as install=C:\Users\Space,
    // extract then fails with ERROR_FILE_NOT_FOUND. Quote every --key=value
    // whose value is a path. pidarg has no spaces so it stays bare.
    std::string cmdline = "\"" + temp + "\" --self-update-stage2"
        + " \"--zip=" + zippath + "\""
        + " \"--install-dir=" + install + "\""
        + " " + pidarg
        + " \"--relaunch=" + self + "\"";
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(NULL, (LPSTR)cmdline.c_str(), NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        Logf("CreateProcess(stage2) failed: %lu", GetLastError());
        return false;
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
#else
    pid_t f = fork();
    if (f < 0) {
        Logf("fork(stage2) failed: %s", strerror(errno));
        return false;
    }
    if (f == 0) {
        execl(temp.c_str(), temp.c_str(), "--self-update-stage2",
              ziparg, instarg, pidarg, relarg, (char*)nullptr);
        _exit(99);
    }
    return true;
#endif
}

} // namespace UpdaterStage2
