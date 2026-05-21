#include "updaterzip.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

#ifndef __APPLE__
#include <unzip.h>

static void mkdir_p(const std::string &path) {
    // Create each intermediate directory, ignoring EEXIST.
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        char c = path[i];
        cur += c;
        if (c == '/' || c == '\\') {
            if (cur.size() > 1) MKDIR(cur.c_str());
        }
    }
    MKDIR(path.c_str());
}
#endif

#ifdef __APPLE__
extern char **environ;

// macOS release zips are produced by `ditto -ck --sequesterRsrc --keepParent
// Silencer.app` and contain a Developer-ID-signed, notarized + stapled
// bundle. The signature seals the *exact* on-disk layout: symlinks (dylib
// version aliases), resource forks, and xattrs all contribute to the
// bundle's CodeResources hash. A generic zip extractor (minizip's
// unzReadCurrentFile loop) writes every entry as a plain regular file —
// symlinks become files, AppleDouble forks land as junk siblings — so the
// reconstructed bundle's seal mismatches and the relaunched app is rejected
// by Gatekeeper with "Silencer is damaged and can't be opened". `ditto`
// is Apple's archive tool and is the only thing that round-trips a
// `ditto -ck` archive losslessly, preserving the signature seal intact.
UpdaterZip::Result UpdaterZip::Extract(const std::string &zippath,
                                       const std::string &destination_dir) {
    const char *argv[] = {
        "/usr/bin/ditto", "-x", "-k",
        zippath.c_str(), destination_dir.c_str(), nullptr
    };
    pid_t pid;
    int rc = posix_spawn(&pid, argv[0], nullptr, nullptr,
                         const_cast<char *const *>(argv), environ);
    if (rc != 0) {
        fprintf(stderr, "[updater] posix_spawn(ditto) failed: %s\n",
                strerror(rc));
        return OPEN_FAIL;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "[updater] waitpid(ditto) failed\n");
        return IO_FAIL;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[updater] ditto -x -k %s -> %s exited %d\n",
                zippath.c_str(), destination_dir.c_str(),
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return CORRUPT;
    }
    return OK;
}
#else
UpdaterZip::Result UpdaterZip::Extract(const std::string &zippath,
                                       const std::string &destination_dir) {
    unzFile zf = unzOpen(zippath.c_str());
    if (!zf) {
        fprintf(stderr, "[updater] unzOpen failed: %s\n", zippath.c_str());
        return OPEN_FAIL;
    }

    if (unzGoToFirstFile(zf) != UNZ_OK) {
        unzClose(zf);
        fprintf(stderr, "[updater] unzGoToFirstFile failed\n");
        return CORRUPT;
    }

    do {
        unz_file_info info;
        char namebuf[2048];
        if (unzGetCurrentFileInfo(zf, &info, namebuf, sizeof(namebuf),
                                  nullptr, 0, nullptr, 0) != UNZ_OK) {
            unzClose(zf);
            return CORRUPT;
        }

        std::string rel = namebuf;
        // Path traversal guard.
        if (rel.find("..") != std::string::npos) {
            fprintf(stderr, "[updater] rejecting suspicious path: %s\n", rel.c_str());
            unzClose(zf);
            return CORRUPT;
        }

        std::string out = destination_dir + "/" + rel;
        if (!rel.empty() && (rel.back() == '/' || rel.back() == '\\')) {
            mkdir_p(out);
            continue;
        }

        // Ensure parent dir exists.
        size_t slash = out.find_last_of("/\\");
        if (slash != std::string::npos) mkdir_p(out.substr(0, slash));

        if (unzOpenCurrentFile(zf) != UNZ_OK) {
            unzClose(zf);
            return CORRUPT;
        }
        FILE *fp = fopen(out.c_str(), "wb");
        if (!fp) {
            unzCloseCurrentFile(zf);
            unzClose(zf);
            fprintf(stderr, "[updater] cannot write %s\n", out.c_str());
            return IO_FAIL;
        }
        char buf[8192];
        int n;
        while ((n = unzReadCurrentFile(zf, buf, sizeof(buf))) > 0) {
            if (fwrite(buf, 1, n, fp) != (size_t)n) {
                fclose(fp);
                unzCloseCurrentFile(zf);
                unzClose(zf);
                fprintf(stderr, "[updater] short write to %s\n", out.c_str());
                return IO_FAIL;
            }
        }
        fclose(fp);
        unzCloseCurrentFile(zf);
        if (n < 0) {
            unzClose(zf);
            return CORRUPT;
        }
        // Preserve executable bit on POSIX (minizip stores it in external_fa).
#ifndef _WIN32
        if ((info.external_fa >> 16) & 0111) {
            chmod(out.c_str(), 0755);
        }
#endif
    } while (unzGoToNextFile(zf) == UNZ_OK);

    unzClose(zf);
    return OK;
}
#endif
