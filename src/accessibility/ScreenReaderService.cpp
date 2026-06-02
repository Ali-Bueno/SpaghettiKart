#include "ScreenReaderService.h"

#include <libultraship.h>

#ifdef ENABLE_PRISM
#include <prism.h>
#endif

ScreenReaderService& ScreenReaderService::Instance() {
    static ScreenReaderService instance;
    return instance;
}

ScreenReaderService::~ScreenReaderService() {
    Shutdown();
}

#ifdef ENABLE_PRISM

bool ScreenReaderService::Initialize() {
    if (mInitialized) {
        return mAvailable;
    }
    mInitialized = true;

    PrismConfig cfg = prism_config_init();
    mContext = prism_init(&cfg);
    if (mContext == nullptr) {
        SPDLOG_WARN("[Accessibility] Failed to initialize PRISM context.");
        return false;
    }

    // acquire_best picks the active screen reader / TTS for the current platform.
    mBackend = prism_registry_acquire_best(mContext);
    if (mBackend == nullptr) {
        SPDLOG_WARN("[Accessibility] No screen reader backend is currently available.");
        return false;
    }

    // acquire_best may hand back a backend that is already initialized (e.g. the
    // NVDA backend), so treat ALREADY_INITIALIZED as success, not failure.
    const PrismError err = prism_backend_initialize(mBackend);
    if (err != PRISM_OK && err != PRISM_ERROR_ALREADY_INITIALIZED) {
        SPDLOG_WARN("[Accessibility] PRISM backend '{}' failed to initialize (error {}).",
                    prism_backend_name(mBackend), static_cast<int>(err));
        mBackend = nullptr;
        return false;
    }

    mAvailable = true;
    SPDLOG_INFO("[Accessibility] Screen reader ready using backend: {}", prism_backend_name(mBackend));
    return true;
}

void ScreenReaderService::Shutdown() {
    if (!mInitialized) {
        return;
    }
    // Backends from acquire_best are owned by the context; just shut the context down.
    if (mContext != nullptr) {
        prism_shutdown(mContext);
        mContext = nullptr;
    }
    mBackend = nullptr;
    mAvailable = false;
    mInitialized = false;
}

const char* ScreenReaderService::BackendName() const {
    if (!mAvailable || mBackend == nullptr) {
        return "None";
    }
    return prism_backend_name(mBackend);
}

void ScreenReaderService::Speak(const std::string& text, bool interrupt) {
    if (!mAvailable || mBackend == nullptr || text.empty()) {
        return;
    }
    // output() drives both speech and braille displays, which is the right
    // behaviour for a screen reader integration.
    const PrismError err = prism_backend_output(mBackend, text.c_str(), interrupt);
    if (err != PRISM_OK) {
        SPDLOG_TRACE("[Accessibility] output() returned error {}.", static_cast<int>(err));
    }
}

void ScreenReaderService::Silence() {
    if (!mAvailable || mBackend == nullptr) {
        return;
    }
    (void) prism_backend_stop(mBackend);
}

#else // ENABLE_PRISM not defined: compile to safe no-ops.

bool ScreenReaderService::Initialize() {
    if (!mInitialized) {
        mInitialized = true;
        SPDLOG_INFO("[Accessibility] Built without PRISM; screen reader output disabled.");
    }
    return false;
}

void ScreenReaderService::Shutdown() {
    mAvailable = false;
    mInitialized = false;
}

const char* ScreenReaderService::BackendName() const {
    return "None (built without PRISM)";
}

void ScreenReaderService::Speak(const std::string&, bool) {
}

void ScreenReaderService::Silence() {
}

#endif // ENABLE_PRISM
