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

bool ScreenReaderService::TryUseBackend(PrismBackend* backend) {
    if (backend == nullptr) {
        return false;
    }
    // acquire may hand back a backend that is already initialized (e.g. the NVDA
    // backend), so treat ALREADY_INITIALIZED as success, not failure.
    const PrismError err = prism_backend_initialize(backend);
    if (err != PRISM_OK && err != PRISM_ERROR_ALREADY_INITIALIZED) {
        SPDLOG_WARN("[Accessibility] PRISM backend '{}' failed to initialize (error {}).",
                    prism_backend_name(backend), static_cast<int>(err));
        return false;
    }
    mBackend = backend;
    mAvailable = true;
    SPDLOG_INFO("[Accessibility] Screen reader ready using backend: {}", prism_backend_name(backend));
    return true;
}

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

    // Log every backend PRISM compiled in / sees on this machine. This is the key
    // diagnostic when a particular screen reader (e.g. JAWS) isn't being picked up:
    // the log shows whether its backend is even present and what priority it has.
    const size_t count = prism_registry_count(mContext);
    SPDLOG_INFO("[Accessibility] PRISM registry has {} backend(s):", count);
    for (size_t i = 0; i < count; ++i) {
        const PrismBackendId id = prism_registry_id_at(mContext, i);
        const char* name = prism_registry_name(mContext, id);
        SPDLOG_INFO("[Accessibility]   backend: {} (priority {})", name != nullptr ? name : "(unnamed)",
                    prism_registry_priority(mContext, id));
    }

    // First try PRISM's auto-pick (the active screen reader for this platform).
    if (TryUseBackend(prism_registry_acquire_best(mContext))) {
        return true;
    }

    // The best pick was unusable (this is what happened with JAWS for one user). Fall
    // back to trying every registered backend in turn so a working one - the user's
    // actual screen reader, or a system TTS like SAPI - is still found.
    SPDLOG_WARN("[Accessibility] Best backend unusable; trying each registered backend.");
    for (size_t i = 0; i < count; ++i) {
        const PrismBackendId id = prism_registry_id_at(mContext, i);
        if (TryUseBackend(prism_registry_acquire(mContext, id))) {
            return true;
        }
    }

    SPDLOG_WARN("[Accessibility] No usable screen reader backend was found.");
    return false;
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
