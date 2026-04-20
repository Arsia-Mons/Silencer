#ifndef UPDATERSTAGE2_H
#define UPDATERSTAGE2_H

#include <string>

namespace UpdaterStage2 {

// Called from main() when --self-update-stage2 is present in argv.
// Returns process exit code. Never returns to the caller on the
// success path (exec replaces us).
int Run(int argc, char **argv);

// Called by the normal client when Updater reaches STAGING.
// Spawns stage-2 (the same binary, copied to a temp path, reinvoked
// with --self-update-stage2), then exits the current process.
// Does not return on success.
void Launch(const std::string &zippath);

} // namespace UpdaterStage2

#endif
