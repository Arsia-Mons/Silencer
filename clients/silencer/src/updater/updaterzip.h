#ifndef UPDATERZIP_H
#define UPDATERZIP_H

#include <string>

// Whole-archive zip extractor into destination_dir (caller creates it).
// Windows/Linux use a minizip read loop. macOS shells out to `ditto -x -k`
// so a `ditto -ck` archive of a Developer-ID-signed bundle round-trips
// losslessly (symlinks, resource forks, xattrs) and the code-signature
// seal stays valid — a generic unzip breaks the seal and Gatekeeper then
// rejects the relaunched app as "damaged".
class UpdaterZip {
public:
    enum Result { OK = 0, OPEN_FAIL, IO_FAIL, CORRUPT };
    static Result Extract(const std::string &zippath,
                          const std::string &destination_dir);
};

#endif
