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
// with --self-update-stage2). Returns true on a successful spawn — the
// caller is then responsible for exiting cleanly via main-return so
// ~Game() runs (otherwise the audio device is left open, producing a
// pop when the new process re-opens it). Returns false if the spawn
// failed and the caller should fall back.
bool Launch(const std::string &zippath);

} // namespace UpdaterStage2

#endif
