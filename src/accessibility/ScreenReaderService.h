#pragma once

#include <string>

/**
 * Thin wrapper around the PRISM screen reader library
 * (https://github.com/ethindp/prism).
 *
 * PRISM auto-detects whatever screen reader / TTS backend is active on the host
 * platform (NVDA, JAWS, SAPI, VoiceOver, Orca, Speech-Dispatcher, ...), so the
 * rest of the accessibility code never needs to know which one is in use.
 *
 * When the project is built without PRISM (ENABLE_PRISM undefined), every method
 * becomes a safe no-op so the game still compiles and runs on every platform.
 */
class ScreenReaderService {
  public:
    static ScreenReaderService& Instance();

    // Acquire and initialize the best available backend. Safe to call once;
    // returns true if a usable screen reader was found.
    bool Initialize();
    void Shutdown();

    bool IsAvailable() const {
        return mAvailable;
    }
    const char* BackendName() const;

    // Speak text. interrupt=true cancels current speech (use it on context
    // changes such as entering a new menu); interrupt=false queues the text.
    void Speak(const std::string& text, bool interrupt = true);
    // Stop any ongoing speech.
    void Silence();

  private:
    ScreenReaderService() = default;
    ~ScreenReaderService();
    ScreenReaderService(const ScreenReaderService&) = delete;
    ScreenReaderService& operator=(const ScreenReaderService&) = delete;

    bool mInitialized = false;
    bool mAvailable = false;

#ifdef ENABLE_PRISM
    struct PrismContext* mContext = nullptr;
    struct PrismBackend* mBackend = nullptr;
#endif
};
