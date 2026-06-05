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

    // Select a backend explicitly instead of relying on prism_registry_acquire_best().
    //
    // acquire_best() returns the first backend (in descending priority) whose
    // initialize() succeeds - it never consults get_features(). PRISM v0.16.5 ships an
    // NVDA backend (priority 103, the highest on Windows) whose initialize() wrongly
    // succeeds even when NVDA is not running (upstream Issue 49, fixed after v0.16.5 but
    // not yet in any release). That non-functional NVDA then shadows the screen reader
    // that IS running - e.g. JAWS at priority 100 - so acquire_best() reports "NVDA" and
    // every Speak() call is silently dropped: a JAWS user gets no speech at all.
    //
    // get_features() is reliable (the bug is only in initialize()), so pick the
    // highest-priority backend that actually reports IS_SUPPORTED_AT_RUNTIME. The
    // registry is already sorted by descending priority, so the first live match wins.
    // This also selects the system TTS (OneCore/SAPI report supported when present) when
    // no screen reader is running.
    for (size_t i = 0; i < count; ++i) {
        const PrismBackendId id = prism_registry_id_at(mContext, i);
        // A throwaway instance just to query runtime availability; get_features() does
        // its own liveness probe and does not need the backend initialized.
        PrismBackend* probe = prism_registry_create(mContext, id);
        if (probe == nullptr) {
            continue;
        }
        const bool supportedAtRuntime =
            (prism_backend_get_features(probe) & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0;
        prism_backend_free(probe);
        if (!supportedAtRuntime) {
            continue;
        }
        if (TryUseBackend(prism_registry_acquire(mContext, id))) {
            return true;
        }
    }

    SPDLOG_WARN("[Accessibility] No screen reader reported itself available at runtime.");
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
