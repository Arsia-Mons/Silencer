// macOS GUI (bare AppKit window + NSProgressIndicator) and the Developer ID
// payload check. HTTP and processes come from platform_posix.cpp.

#import <Cocoa/Cocoa.h>

#include <filesystem>
#include <string>
#include <unistd.h>

#include "stub.h"

void stub::gui_run(GuiState &st) {
  GuiState *stp = &st;
  if (env_str("SILENCER_STUB_NO_GUI") == "1") {
    while (!stp->done.load())
      usleep(30 * 1000);
    return;
  }
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSWindow *win =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 110)
                                    styleMask:NSWindowStyleMaskTitled
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    win.title = @"Silencer Launcher";
    NSTextField *label = [NSTextField labelWithString:@"Checking for updates..."];
    label.frame = NSMakeRect(20, 62, 380, 24);
    NSProgressIndicator *bar =
        [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 30, 380, 20)];
    bar.style = NSProgressIndicatorStyleBar;
    bar.indeterminate = YES;
    [bar startAnimation:nil];
    [win.contentView addSubview:label];
    [win.contentView addSubview:bar];
    [win center];
    [win makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    __block BOOL downloading = NO;
    NSTimer *timer = [NSTimer
        scheduledTimerWithTimeInterval:0.03
                               repeats:YES
                                 block:^(NSTimer *) {
                                   if (stp->done.load()) {
                                     [NSApp stop:nil];
                                     // stop: only takes effect after an event
                                     NSEvent *e = [NSEvent
                                         otherEventWithType:NSEventTypeApplicationDefined
                                                   location:NSZeroPoint
                                              modifierFlags:0
                                                  timestamp:0
                                               windowNumber:0
                                                    context:nil
                                                    subtype:0
                                                      data1:0
                                                      data2:0];
                                     [NSApp postEvent:e atStart:YES];
                                     return;
                                   }
                                   if (!downloading &&
                                       stp->phase.load() == kDownloading) {
                                     downloading = YES;
                                     label.stringValue =
                                         @"Updating Silencer Launcher...";
                                   }
                                   if (downloading) {
                                     int pc = stp->percent.load();
                                     if (pc >= 0) {
                                       bar.indeterminate = NO;
                                       bar.minValue = 0;
                                       bar.maxValue = 100;
                                       bar.doubleValue = pc;
                                     }
                                   }
                                 }];
    [NSApp run];
    [timer invalidate];
    [win orderOut:nil];
  }
}

void stub::error_box(const std::string &msg) {
  logf("%s", msg.c_str());
  if (env_str("SILENCER_STUB_NO_GUI") == "1")
    return;
  @autoreleasepool {
    [NSApplication sharedApplication];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"Silencer Launcher";
    alert.informativeText = [NSString stringWithUTF8String:msg.c_str()];
    [alert runModal];
  }
}

// Developer ID check: the staged payload must be signed by the same team as
// the running stub's own bundle. An unsigned stub (dev build, the e2e) has
// no team to enforce and skips the check - a signed release enforces it
// automatically, which is the payload-signing property that closes the
// compromised-manifest-host gap sha256-over-TLS leaves open.
bool stub::verify_payload(const std::string &extracted_dir) {
  std::string exe = own_exe_path();
  size_t p = exe.rfind("/Contents/MacOS/");
  if (p == std::string::npos)
    return true; // not running from a bundle (dev binary)
  std::string bundle = exe.substr(0, p);

  std::string team;
  {
    std::string cmd = "/usr/bin/codesign -dvv \"" + bundle + "\" 2>&1";
    FILE *f = popen(cmd.c_str(), "r");
    if (f) {
      char line[512];
      while (fgets(line, sizeof line, f)) {
        std::string s = line;
        const std::string key = "TeamIdentifier=";
        size_t k = s.find(key);
        if (k != std::string::npos) {
          team = s.substr(k + key.size());
          while (!team.empty() && (team.back() == '\n' || team.back() == '\r'))
            team.pop_back();
        }
      }
      pclose(f);
    }
  }
  if (team.empty() || team == "not set")
    return true; // unsigned stub: nothing to enforce

  std::filesystem::path app =
      std::filesystem::u8path(extracted_dir) / "Silencer Launcher.app";
  std::string requirement = "=anchor apple generic and certificate "
                            "leaf[subject.OU] = \"" + team + "\"";
  return run_tool({"/usr/bin/codesign", "--verify", "--strict", "--deep", "-R",
                   requirement, app.u8string()});
}
