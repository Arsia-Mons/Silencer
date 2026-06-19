#ifndef UPDATER_H
#define UPDATER_H

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

// High-level updater state machine. Wraps UpdaterDownload + UpdaterZip,
// runs the work on a background thread, exposes progress to the UI.
class Updater {
public:
    enum State {
        IDLE,           // no update requested
        PROMPTING,      // ready, waiting for user consent
        DOWNLOADING,
        VERIFYING,
        STAGING,        // spawning stage-2 child; UI should tear down SDL
        FAILED,
        DONE            // stage-2 launched; main should exit
    };

    // Static helper — exposed for unit tests.
    static bool VerifyFile(const std::string &path, const uint8_t expected[32]);

    Updater();
    ~Updater();

    // Called by the lobby code when it sees a reject-with-update.
    // Transitions IDLE → PROMPTING.
    void PresentUpdate(const std::string &url,
                       const uint8_t sha256[32]);

    // Called by the UI when the user clicks Update.
    // Transitions PROMPTING → DOWNLOADING, kicks off worker thread.
    void Consent();

    // Called by the UI when the user clicks Cancel.
    void Cancel();

    // Called by the UI when the user clicks Retry in a failure dialog.
    void Retry();

    State GetState();
    float GetProgress();              // 0.0-1.0 during DOWNLOADING
    std::string GetErrorMessage();    // non-empty in FAILED
    int  GetRetryCount();             // starts at 0, bumped on Retry()
    std::string GetDownloadURL();     // for the "open download page" escape hatch

    // Test-only seam: force the state machine + a synthetic prompt/error so the
    // control socket can surface the update modal headlessly (mirrors the
    // show_*_modal test ops). Does NOT touch the worker thread; intended only
    // for the static phases the modal renders (Prompting/Downloading/Failed).
    void ForceState(State s);

    // Called every frame by Game::Loop on the main thread. When the worker has
    // reached STAGING (download + verify complete), spawns the stage-2 child via
    // UpdaterStage2::Launch — once. The worker can't do this itself: the spawn
    // must run on the main thread while we still own SDL. On success
    // IsStage2Spawned() latches true, so Game::Loop returns false and main()
    // unwinds, letting ~Game tear down SDL/audio before the replacement client
    // opens the device (skipping that teardown pops the audio device). On a
    // spawn failure the state machine transitions to FAILED. No-op otherwise.
    void PumpStage2();
    bool IsStage2Spawned() const;

    // Trampolines into the private atomic state from the progress callback.
    friend void UpdaterSetProgress(Updater &u, uint64_t got, uint64_t total);
    friend void UpdaterCheckCancel(Updater &u, bool *out);

private:
    void Run();                       // worker thread entry

    std::mutex mu;
    State state;
    std::string url;
    std::array<uint8_t,32> sha;
    std::string error;
    std::atomic<uint64_t> bytes_got;
    std::atomic<uint64_t> bytes_total;
    std::atomic<bool> cancel_flag;
    std::thread worker;
    int retries;
    std::string stage2zip;                     // verified update zip, set when entering STAGING
    std::atomic<bool> stage2spawned{false};    // stage-2 child launched; Game::Loop unwinds
    std::atomic<bool> stage2attempted{false};  // launch tried (one-shot; success or failure)
};

void UpdaterSetProgress(Updater &u, uint64_t got, uint64_t total);
void UpdaterCheckCancel(Updater &u, bool *out);

#endif
