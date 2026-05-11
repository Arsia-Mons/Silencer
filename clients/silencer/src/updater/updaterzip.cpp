#include "updaterzip.h"
#include <unzip.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0755)
#endif

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
