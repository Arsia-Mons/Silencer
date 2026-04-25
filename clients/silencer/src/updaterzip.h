#ifndef UPDATERZIP_H
#define UPDATERZIP_H

#include <string>

// Minizip-backed zip extractor. Extracts the whole archive into
// destination_dir, which must already exist (caller creates it).
class UpdaterZip {
public:
    enum Result { OK = 0, OPEN_FAIL, IO_FAIL, CORRUPT };
    static Result Extract(const std::string &zippath,
                          const std::string &destination_dir);
};

#endif
