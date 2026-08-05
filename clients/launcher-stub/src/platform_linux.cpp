// Linux GUI: zenity when a display and zenity exist, silent otherwise.
// There is no native toolkit to call, and linking GTK into the
// never-changes stub would be the opposite of "least at risk" - a missing
// zenity just means the update happens without a window.
// HTTP and processes come from platform_posix.cpp.

#include "stub.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

void stub::gui_run(GuiState &st) {
  const bool headless = env_str("SILENCER_STUB_NO_GUI") == "1" ||
                        (env_str("DISPLAY").empty() &&
                         env_str("WAYLAND_DISPLAY").empty());
  FILE *z = nullptr;
  if (!headless) {
    signal(SIGPIPE, SIG_IGN); // a vanished zenity must not kill the stub
    z = popen("zenity --progress --title='Silencer Launcher' "
              "--text='Checking for updates...' --percent=0 --auto-close "
              "--no-cancel 2>/dev/null",
              "w");
  }
  bool downloading = false;
  int last_pc = -1;
  while (!st.done.load()) {
    if (z) {
      if (!downloading && st.phase.load() == kDownloading) {
        downloading = true;
        if (fprintf(z, "# Updating...\n") < 0 ||
            fflush(z) == EOF) {
          pclose(z);
          z = nullptr;
        }
      }
      int pc = st.percent.load();
      if (z && pc >= 0 && pc != last_pc && pc < 100) {
        last_pc = pc;
        if (fprintf(z, "%d\n", pc) < 0 || fflush(z) == EOF) {
          pclose(z);
          z = nullptr;
        }
      }
    }
    usleep(50 * 1000);
  }
  if (z) {
    fprintf(z, "100\n");
    fflush(z);
    pclose(z);
  }
}

void stub::error_box(const std::string &msg) {
  logf("%s", msg.c_str());
  if (env_str("SILENCER_STUB_NO_GUI") == "1")
    return;
  // argv form, no shell: message content can never become shell input.
  (void)run_tool({"zenity", "--error", "--title=Silencer Launcher",
                  "--text=" + msg});
}

bool stub::verify_payload(const std::string &) {
  return true; // sha256 already verified; no platform code signing on Linux
}
