#include "SettingsMenu.h"

#include "ScreenReaderService.h"
#include "AudioCueService.h"
#include "AccessibilityCVars.h"

#include <libultraship.h> // CVarGet/Set Integer/Float, CVarSave

#include <cmath>
#include <cstdio>
#include <cctype>
#include <string>

// Game C symbols consumed here. Linkage is by symbol name (same bridge pattern
// as the rest of the accessibility module), so we avoid the heavy C-only headers.
extern "C" {
extern unsigned char gSoundMode;                          // SOUND_STEREO..SOUND_MONO
void apply_sound_mode_setting(int mode);                  // defined in menus.c
void audio_set_player_volume(unsigned char player, float volume); // audio/external.c
void HMAS_RefreshMusicVolume(void);                       // port/audio/HMAS.cpp (streamed music)
}

namespace {

// Controller bit masks (mirror of CONT_* in libultra os.h, to keep defines.h out
// of this C++ TU). The caller passes buttonPressed|stickPressed (edge-triggered).
constexpr unsigned short kBtnA = 0x8000;
constexpr unsigned short kBtnB = 0x4000;
constexpr unsigned short kBtnZ = 0x2000;
constexpr unsigned short kBtnUp = 0x0800;
constexpr unsigned short kBtnDown = 0x0400;
constexpr unsigned short kBtnLeft = 0x0200;
constexpr unsigned short kBtnRight = 0x0100;

// Cue example a Help (Info) entry can play with the Z button. 0 = none.
constexpr int kCueNone = 0;
constexpr int kCueApproach = 1;
constexpr int kCueCurve = 2;
constexpr int kCueEdge = 3;

// Audio sequence players (mirror of SEQ_PLAYER_* in audio/external.h).
constexpr unsigned char kSeqLevel = 0; // background music
constexpr unsigned char kSeqEnv = 1;   // environment
constexpr unsigned char kSeqSfx = 2;   // sound effects

// How many option rows are visible at once (the rest scroll).
constexpr int kMaxVisible = 9;

enum class OptKind { Toggle, IntSlider, Enum, FloatSlider, Info };

struct Option {
    const char* label;          // spoken + (uppercased) shown
    OptKind kind;
    const char* cvar;           // null => non-CVar option (use getInt/setInt)
    int idefault;               // default for Toggle/Enum/IntSlider
    float fdefault;             // default for FloatSlider
    float fmin, fmax, fstep;    // slider bounds/step (IntSlider casts to int)
    bool asPercent;             // speak/show value as N%
    const char* const* labels;  // Enum option labels
    int labelCount;
    int (*getInt)();            // non-CVar getter (cvar == null)
    void (*setInt)(int);        // non-CVar setter
    void (*onChange)();         // post-write side effect (e.g. apply volume live)
    const char* note;           // optional spoken hint (e.g. "Restart required")
    const char* help;           // Info option: explanation spoken when A is pressed
    int cueExample;             // Info option: cue to play with Z (kCue* ), 0 = none
};

struct Category {
    const char* name;
    const Option* options;
    int count;
};

// --- Sound-category side effects / non-CVar accessors -----------------------

void ApplyMusicVolume() {
    audio_set_player_volume(kSeqLevel, CVarGetFloat("gMainMusicVolume", 1.0f)); // N64 sequence music
    HMAS_RefreshMusicVolume();                                                  // streamed (HMAS) music
}
void ApplySfxVolume() {
    audio_set_player_volume(kSeqSfx, CVarGetFloat("gSFXMusicVolume", 1.0f));
}
void ApplyEnvVolume() {
    audio_set_player_volume(kSeqEnv, CVarGetFloat("gEnvironmentVolume", 1.0f));
}
int GetSoundMode() {
    return static_cast<int>(gSoundMode);
}
void SetSoundMode(int v) {
    apply_sound_mode_setting(v);
}

// --- Graphics-category side effects (apply the change live, like PortMenu) ---

void ApplyInternalResolution() {
    Ship::Context::GetInstance()->GetWindow()->SetResolutionMultiplier(CVarGetFloat("gInternalResolution", 1.0f));
}
void ApplyMsaa() {
    Ship::Context::GetInstance()->GetWindow()->SetMsaaLevel(CVarGetInteger("gMSAAValue", 1));
}

// --- Option label tables ----------------------------------------------------

const char* const kSoundModeLabels[] = { "Stereo", "Headphones", "Surround", "Mono" };

// --- Accessibility category (mirrors DrawAccessibilityMenu metadata) ---------

const Option kAccessibility[] = {
    { .label = "Enable accessibility", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_ENABLED,
      .idefault = CVAR_ACCESS_ENABLED_DEFAULT },
    { .label = "Screen reader", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_SCREEN_READER,
      .idefault = CVAR_ACCESS_SCREEN_READER_DEFAULT, .note = "Restart required" },
    { .label = "Narrate menus", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_MENU_NARRATION,
      .idefault = CVAR_ACCESS_MENU_NARRATION_DEFAULT },
    { .label = "Narrate races", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_RACE_NARRATION,
      .idefault = CVAR_ACCESS_RACE_NARRATION_DEFAULT },
    { .label = "Off-road cue", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_OFFROAD_CUE,
      .idefault = CVAR_ACCESS_OFFROAD_CUE_DEFAULT },
    { .label = "Blind drive assist", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_DRIVE_ASSIST,
      .idefault = CVAR_ACCESS_DRIVE_ASSIST_DEFAULT },
    { .label = "Pan strength", .kind = OptKind::IntSlider, .cvar = CVAR_ACCESS_DRIVE_PAN_STRENGTH,
      .idefault = CVAR_ACCESS_DRIVE_PAN_STRENGTH_DEFAULT, .fmin = 0, .fmax = 100, .fstep = 5,
      .asPercent = true },
    { .label = "Anticipation", .kind = OptKind::IntSlider, .cvar = CVAR_ACCESS_DRIVE_LOOKAHEAD,
      .idefault = CVAR_ACCESS_DRIVE_LOOKAHEAD_DEFAULT, .fmin = 1, .fmax = 20, .fstep = 1 },
    { .label = "Invert sides", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_DRIVE_INVERT,
      .idefault = CVAR_ACCESS_DRIVE_INVERT_DEFAULT },
    { .label = "Edge proximity cue", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_EDGE_CUE,
      .idefault = CVAR_ACCESS_EDGE_CUE_DEFAULT },
    { .label = "Edge sensitivity", .kind = OptKind::IntSlider, .cvar = CVAR_ACCESS_EDGE_SENSITIVITY,
      .idefault = CVAR_ACCESS_EDGE_SENSITIVITY_DEFAULT, .fmin = 0, .fmax = 100, .fstep = 5,
      .asPercent = true },
    // Help entries: focus to hear the name, press A to hear how that cue works.
    { .label = "Help: steering guide", .kind = OptKind::Info,
      .help = "The engine sound leans left or right toward the way you should steer to follow the racing "
              "line. Drive toward the sound. Pan strength sets how strongly it leans, anticipation sets how "
              "far ahead it looks, and invert sides flips it if it feels backwards." },
    { .label = "Help: curve calls", .kind = OptKind::Info,
      .help = "Before a curve, a voice announces left, right, hard left or hard right, so you can prepare "
              "to turn." },
    { .label = "Help: approach beeps", .kind = OptKind::Info,
      .help = "As you near a curve, rising beeps count down to it. Press Z to hear an example.",
      .cueExample = kCueApproach },
    { .label = "Help: in-curve beeps", .kind = OptKind::Info,
      .help = "Inside a curve you hear an entry beep, an apex beep at the tightest point, and a higher exit "
              "beep. Press Z to hear an example.",
      .cueExample = kCueCurve },
    { .label = "Help: edge cue", .kind = OptKind::Info,
      .help = "As you drift toward a track edge, beeps pan to that side and get faster and higher, and a "
              "steady tone sounds right at the edge. It stays silent while you are centered. Edge sensitivity "
              "sets how early it starts. Press Z to hear an example.",
      .cueExample = kCueEdge },
    { .label = "Help: off-road cue", .kind = OptKind::Info,
      .help = "A voice says off road when you leave the track, and on road when you return." },
};

// --- Sound category ---------------------------------------------------------

const Option kSound[] = {
    { .label = "Master volume", .kind = OptKind::FloatSlider, .cvar = "gGameMasterVolume",
      .fdefault = 1.0f, .fmin = 0.0f, .fmax = 1.0f, .fstep = 0.05f, .asPercent = true },
    { .label = "Music volume", .kind = OptKind::FloatSlider, .cvar = "gMainMusicVolume",
      .fdefault = 1.0f, .fmin = 0.0f, .fmax = 1.0f, .fstep = 0.05f, .asPercent = true,
      .onChange = ApplyMusicVolume },
    { .label = "Sound effects volume", .kind = OptKind::FloatSlider, .cvar = "gSFXMusicVolume",
      .fdefault = 1.0f, .fmin = 0.0f, .fmax = 1.0f, .fstep = 0.05f, .asPercent = true,
      .onChange = ApplySfxVolume },
    { .label = "Environment volume", .kind = OptKind::FloatSlider, .cvar = "gEnvironmentVolume",
      .fdefault = 1.0f, .fmin = 0.0f, .fmax = 1.0f, .fstep = 0.05f, .asPercent = true,
      .onChange = ApplyEnvVolume },
    { .label = "Sound mode", .kind = OptKind::Enum, .cvar = nullptr, .idefault = 0,
      .labels = kSoundModeLabels, .labelCount = 4, .getInt = GetSoundMode, .setInt = SetSoundMode },
};

// --- Graphics category (visual settings; MSAA/resolution apply live via Ship) -

const Option kGraphics[] = {
    { .label = "Internal resolution", .kind = OptKind::FloatSlider, .cvar = "gInternalResolution",
      .fdefault = 1.0f, .fmin = 0.5f, .fmax = 4.0f, .fstep = 0.1f, .asPercent = true,
      .onChange = ApplyInternalResolution },
    { .label = "Anti-aliasing", .kind = OptKind::IntSlider, .cvar = "gMSAAValue",
      .idefault = 1, .fmin = 1, .fmax = 8, .fstep = 1, .onChange = ApplyMsaa },
    { .label = "Frame rate", .kind = OptKind::IntSlider, .cvar = "gInterpolationFPS",
      .idefault = 30, .fmin = 30, .fmax = 240, .fstep = 10 },
    { .label = "Vertical sync", .kind = OptKind::Toggle, .cvar = "gVsyncEnabled", .idefault = 1 },
    { .label = "Windowed fullscreen", .kind = OptKind::Toggle, .cvar = "gSdlWindowedFullscreen", .idefault = 0 },
};

// --- Enhancements category (all plain CVars read live by the engine) ----------

const Option kEnhancements[] = {
    { .label = "No multiplayer feature cuts", .kind = OptKind::Toggle, .cvar = "gMultiplayerNoFeatureCuts", .idefault = 0 },
    { .label = "Widescreen portrait spacing", .kind = OptKind::Toggle, .cvar = "gBetterResultPortraits", .idefault = 0 },
    { .label = "Disable level of detail", .kind = OptKind::Toggle, .cvar = "gDisableLod", .idefault = 0 },
    { .label = "Disable culling", .kind = OptKind::Toggle, .cvar = "gNoCulling", .idefault = 0 },
    { .label = "Disable rubber banding", .kind = OptKind::Toggle, .cvar = "gDisableRubberbanding", .idefault = 0 },
    { .label = "Far frustum", .kind = OptKind::FloatSlider, .cvar = "gFarFrustrum",
      .fdefault = 10000.0f, .fmin = 0.0f, .fmax = 10000.0f, .fstep = 500.0f },
    { .label = "Enable custom CC", .kind = OptKind::Toggle, .cvar = "gEnableCustomCC", .idefault = 0 },
    { .label = "Custom CC", .kind = OptKind::FloatSlider, .cvar = "gCustomCC",
      .fdefault = 150.0f, .fmin = 0.0f, .fmax = 1000.0f, .fstep = 25.0f },
    { .label = "Digital speedometer", .kind = OptKind::Toggle, .cvar = "gEnableDigitalSpeedometer", .idefault = 0 },
    { .label = "Harder CPU", .kind = OptKind::Toggle, .cvar = "gHarderCPU", .idefault = 0 },
    { .label = "Show Spaghetti version", .kind = OptKind::Toggle, .cvar = "gShowSpaghettiVersion", .idefault = 1 },
    { .label = "Look behind camera", .kind = OptKind::Toggle, .cvar = "gLookBehind", .idefault = 0 },
};

// --- Cheats category ----------------------------------------------------------

const Option kCheats[] = {
    { .label = "Moon jump", .kind = OptKind::Toggle, .cvar = "gEnableMoonJump", .idefault = 0 },
    { .label = "Disable wall collision", .kind = OptKind::Toggle, .cvar = "gNoWallColision", .idefault = 0 },
    { .label = "Minimum height", .kind = OptKind::FloatSlider, .cvar = "gMinHeight",
      .fdefault = 0.0f, .fmin = -50.0f, .fmax = 50.0f, .fstep = 5.0f },
};

// --- Rulesets category --------------------------------------------------------

const Option kRulesets[] = {
    { .label = "Unique character selections", .kind = OptKind::Toggle, .cvar = "gUniqueCharacterSelections", .idefault = 1 },
    { .label = "No item boxes", .kind = OptKind::Toggle, .cvar = "gDisableItemboxes", .idefault = 0 },
    { .label = "All thwomps are Marty", .kind = OptKind::Toggle, .cvar = "gAllThwompsAreMarty", .idefault = 0 },
    { .label = "All bomb karts chase", .kind = OptKind::Toggle, .cvar = "gAllBombKartsChase", .idefault = 0 },
    { .label = "Collect the trophies", .kind = OptKind::Toggle, .cvar = "gGoFish", .idefault = 0 },
    { .label = "Trains", .kind = OptKind::IntSlider, .cvar = "gNumTrains", .idefault = 2, .fmin = 0, .fmax = 19, .fstep = 1 },
    { .label = "Carriages", .kind = OptKind::IntSlider, .cvar = "gNumCarriages", .idefault = 5, .fmin = 0, .fmax = 74, .fstep = 1 },
    { .label = "Train has a tender", .kind = OptKind::Toggle, .cvar = "gHasTender", .idefault = 1 },
    { .label = "Trucks", .kind = OptKind::IntSlider, .cvar = "gNumTrucks", .idefault = 7, .fmin = 0, .fmax = 50, .fstep = 1 },
    { .label = "Buses", .kind = OptKind::IntSlider, .cvar = "gNumBuses", .idefault = 7, .fmin = 0, .fmax = 50, .fstep = 1 },
    { .label = "Tanker trucks", .kind = OptKind::IntSlider, .cvar = "gNumTankerTrucks", .idefault = 7, .fmin = 0, .fmax = 50, .fstep = 1 },
    { .label = "Cars", .kind = OptKind::IntSlider, .cvar = "gNumCars", .idefault = 7, .fmin = 0, .fmax = 50, .fstep = 1 },
};

#define ARRAY_LEN(a) (static_cast<int>(sizeof(a) / sizeof((a)[0])))

const Category kCategories[] = {
    { "Accessibility", kAccessibility, ARRAY_LEN(kAccessibility) },
    { "Sound", kSound, ARRAY_LEN(kSound) },
    { "Graphics", kGraphics, ARRAY_LEN(kGraphics) },
    { "Enhancements", kEnhancements, ARRAY_LEN(kEnhancements) },
    { "Cheats", kCheats, ARRAY_LEN(kCheats) },
    { "Rulesets", kRulesets, ARRAY_LEN(kRulesets) },
};
constexpr int kCategoryCount = ARRAY_LEN(kCategories);

// --- Value read / write / format --------------------------------------------

int GetIntValue(const Option& o) {
    if (o.cvar != nullptr) {
        return CVarGetInteger(o.cvar, o.idefault);
    }
    return o.getInt != nullptr ? o.getInt() : 0;
}

void SetIntValue(const Option& o, int v) {
    if (o.cvar != nullptr) {
        CVarSetInteger(o.cvar, v);
        CVarSave();
    } else if (o.setInt != nullptr) {
        o.setInt(v);
    }
    if (o.onChange != nullptr) {
        o.onChange();
    }
}

float GetFloatValue(const Option& o) {
    return CVarGetFloat(o.cvar, o.fdefault);
}

void SetFloatValue(const Option& o, float v) {
    CVarSetFloat(o.cvar, v);
    CVarSave();
    if (o.onChange != nullptr) {
        o.onChange();
    }
}

// Spoken value (nicely cased, includes %).
std::string SpeechValue(const Option& o) {
    switch (o.kind) {
        case OptKind::Toggle:
            return GetIntValue(o) != 0 ? "On" : "Off";
        case OptKind::Enum: {
            int v = GetIntValue(o);
            if (v < 0) v = 0;
            if (v >= o.labelCount) v = o.labelCount - 1;
            return o.labels[v];
        }
        case OptKind::IntSlider: {
            std::string s = std::to_string(GetIntValue(o));
            if (o.asPercent) s += "%";
            return s;
        }
        case OptKind::FloatSlider: {
            float v = GetFloatValue(o);
            if (o.asPercent) {
                return std::to_string(static_cast<int>(std::lround(v * 100.0f))) + "%";
            }
            return std::to_string(static_cast<int>(std::lround(v)));
        }
    }
    return "";
}

// Spoken value plus an optional hint (e.g. "Restart required" for the screen reader).
std::string SpokenValue(const Option& o) {
    std::string s = SpeechValue(o);
    if (o.note != nullptr) {
        s += ". ";
        s += o.note;
    }
    return s;
}

// Display value (uppercase, ASCII-safe for the game font: letters/digits/space).
std::string DisplayValue(const Option& o) {
    switch (o.kind) {
        case OptKind::Toggle:
            return GetIntValue(o) != 0 ? "ON" : "OFF";
        case OptKind::Enum: {
            int v = GetIntValue(o);
            if (v < 0) v = 0;
            if (v >= o.labelCount) v = o.labelCount - 1;
            std::string s = o.labels[v];
            for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }
        case OptKind::IntSlider:
            return std::to_string(GetIntValue(o));
        case OptKind::FloatSlider: {
            float v = GetFloatValue(o);
            if (o.asPercent) {
                return std::to_string(static_cast<int>(std::lround(v * 100.0f)));
            }
            return std::to_string(static_cast<int>(std::lround(v)));
        }
    }
    return "";
}

// Adjust the focused option by dir (-1 left, +1 right / A). Toggles flip either way.
void Adjust(const Option& o, int dir) {
    switch (o.kind) {
        case OptKind::Toggle:
            SetIntValue(o, GetIntValue(o) != 0 ? 0 : 1);
            break;
        case OptKind::Enum: {
            int n = o.labelCount;
            int v = ((GetIntValue(o) + dir) % n + n) % n;
            SetIntValue(o, v);
            break;
        }
        case OptKind::IntSlider: {
            int step = static_cast<int>(o.fstep);
            int v = GetIntValue(o) + dir * step;
            if (v < static_cast<int>(o.fmin)) v = static_cast<int>(o.fmin);
            if (v > static_cast<int>(o.fmax)) v = static_cast<int>(o.fmax);
            SetIntValue(o, v);
            break;
        }
        case OptKind::FloatSlider: {
            float v = GetFloatValue(o) + dir * o.fstep;
            if (v < o.fmin) v = o.fmin;
            if (v > o.fmax) v = o.fmax;
            SetFloatValue(o, v);
            break;
        }
    }
}

// A scripted demo of a cue: a timed sequence of beeps / held-tone events that
// mimics how the cue actually plays during a race.
enum { DEMO_BEEP = 0, DEMO_TONE_ON = 1, DEMO_TONE_OFF = 2 };
struct DemoStep {
    int wait;       // ticks to wait before firing this step
    int action;     // DEMO_BEEP / DEMO_TONE_ON / DEMO_TONE_OFF
    CueBeep beep;
    float pitch;
    float pan;
};

// Approach: three rising beeps counting down to the curve (DriveAssist kPitches).
const DemoStep kApproachDemo[] = {
    { 0,  DEMO_BEEP, CueBeep::Approach, 1.00f, 0.0f },
    { 13, DEMO_BEEP, CueBeep::Approach, 1.25f, 0.0f },
    { 12, DEMO_BEEP, CueBeep::Approach, 1.55f, 0.0f },
};

// In-curve: entry, apex (same pitch), then a higher exit beep.
const DemoStep kCurveDemo[] = {
    { 0,  DEMO_BEEP, CueBeep::Curve, 1.0f, 0.0f },
    { 20, DEMO_BEEP, CueBeep::Curve, 1.0f, 0.0f },
    { 22, DEMO_BEEP, CueBeep::Curve, 1.5f, 0.0f },
};

// Edge: beeps panned to one side, accelerating and rising as you near the edge,
// then the steady held tone right at the limit (matches the DriveAssist edge layer).
const DemoStep kEdgeDemo[] = {
    { 0,  DEMO_BEEP,     CueBeep::Edge, 0.80f, 0.9f },
    { 12, DEMO_BEEP,     CueBeep::Edge, 1.00f, 0.9f },
    { 10, DEMO_BEEP,     CueBeep::Edge, 1.20f, 0.9f },
    { 8,  DEMO_BEEP,     CueBeep::Edge, 1.40f, 0.9f },
    { 5,  DEMO_BEEP,     CueBeep::Edge, 1.60f, 0.9f },
    { 4,  DEMO_BEEP,     CueBeep::Edge, 1.80f, 0.9f },
    { 3,  DEMO_TONE_ON,  CueBeep::Edge, 1.80f, 0.9f },
    { 30, DEMO_TONE_OFF, CueBeep::Edge, 0.00f, 0.0f },
};

} // namespace

// ---------------------------------------------------------------------------

class SettingsMenu {
  public:
    static SettingsMenu& Instance() {
        static SettingsMenu instance;
        return instance;
    }

    void Open(int categoryId) {
        mCategory = (categoryId >= 0 && categoryId < kCategoryCount) ? categoryId : 0;
        mCursor = 0;
        mScrollTop = 0;
        const Category& cat = kCategories[mCategory];
        const Option& first = cat.options[0];
        Speak(std::string(cat.name) + ". " + first.label + ": " + SpokenValue(first));
    }

    int HandleInput(unsigned short b) {
        const Category& cat = kCategories[mCategory];

        if (b & kBtnB) {
            return 0x1 | 0x8; // exit + go-back SFX (MenuNarrator re-announces the row)
        }
        if (b & kBtnZ) {
            // Play a Help entry's cue demo (no menu SFX; the cue is the feedback).
            StartDemo(cat.options[mCursor].cueExample);
            return 0;
        }
        // Any other input stops a running demo.
        if (b != 0) {
            StopDemo();
        }
        if (b & kBtnUp) {
            if (mCursor > 0) {
                mCursor--;
                ClampScroll();
                SpeakCursor();
                return 0x2;
            }
            return 0;
        }
        if (b & kBtnDown) {
            if (mCursor < cat.count - 1) {
                mCursor++;
                ClampScroll();
                SpeakCursor();
                return 0x2;
            }
            return 0;
        }
        if (b & kBtnLeft) {
            ActivateOption(cat.options[mCursor], -1);
            return 0x2;
        }
        if ((b & kBtnRight) || (b & kBtnA)) {
            ActivateOption(cat.options[mCursor], +1);
            return 0x2;
        }
        return 0;
    }

    int RowCount() const {
        int remaining = kCategories[mCategory].count - mScrollTop;
        return remaining < kMaxVisible ? remaining : kMaxVisible;
    }
    bool RowSelected(int i) const {
        return (mScrollTop + i) == mCursor;
    }
    const char* Title() const {
        return kCategories[mCategory].name;
    }
    void RowText(int i, char* out, int outSize) const {
        if (outSize <= 0) {
            return;
        }
        const Category& cat = kCategories[mCategory];
        int idx = mScrollTop + i;
        if (idx < 0 || idx >= cat.count) {
            out[0] = '\0';
            return;
        }
        const Option& o = cat.options[idx];
        std::string label = o.label;
        for (char& c : label) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::snprintf(out, outSize, "%s  %s", label.c_str(), DisplayValue(o).c_str());
    }
    int OpenCategoryId() const {
        return mCategory;
    }

    // Advance the running cue demo by one tick (called once per frame). Fires the
    // next scripted beep / tone event when its delay elapses.
    void TickDemo() {
        if (mDemo == nullptr) {
            return;
        }
        if (mDemoTimer > 0) {
            mDemoTimer--;
            return;
        }
        const DemoStep& s = mDemo[mDemoIndex];
        switch (s.action) {
            case DEMO_BEEP:
                AudioCueService::Instance().PlayBeep(s.beep, s.pitch, s.pan);
                break;
            case DEMO_TONE_ON:
                AudioCueService::Instance().SetEdgeTone(true, s.pitch, s.pan);
                break;
            case DEMO_TONE_OFF:
                AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f);
                break;
        }
        mDemoIndex++;
        if (mDemoIndex >= mDemoLen) {
            mDemo = nullptr; // done (an edge demo's last step already stopped the tone)
            return;
        }
        mDemoTimer = mDemo[mDemoIndex].wait;
    }
    bool DemoActive() const {
        return mDemo != nullptr;
    }

  private:
    SettingsMenu() = default;

    // Start a scripted demo for the given cue (kCue*), replacing any running one.
    void StartDemo(int cueExample) {
        StopDemo();
        switch (cueExample) {
            case kCueApproach: mDemo = kApproachDemo; mDemoLen = ARRAY_LEN(kApproachDemo); break;
            case kCueCurve:    mDemo = kCurveDemo;    mDemoLen = ARRAY_LEN(kCurveDemo);    break;
            case kCueEdge:     mDemo = kEdgeDemo;     mDemoLen = ARRAY_LEN(kEdgeDemo);     break;
            default:           return; // no example for this entry
        }
        mDemoIndex = 0;
        mDemoTimer = mDemo[0].wait;
    }
    void StopDemo() {
        if (mDemo != nullptr) {
            AudioCueService::Instance().SetEdgeTone(false, 0.0f, 0.0f); // kill any held tone
        }
        mDemo = nullptr;
        mDemoLen = 0;
        mDemoIndex = 0;
        mDemoTimer = 0;
    }

    void Speak(const std::string& text) {
        ScreenReaderService::Instance().Speak(text, true);
    }
    void SpeakCursor() {
        const Option& o = kCategories[mCategory].options[mCursor];
        const std::string v = SpokenValue(o);
        Speak(v.empty() ? std::string(o.label) : (std::string(o.label) + ": " + v));
    }
    // Left/Right/A on the focused option: speak an Info option's explanation, or
    // change a setting's value and speak the new value.
    void ActivateOption(const Option& o, int dir) {
        if (o.kind == OptKind::Info) {
            Speak(o.help != nullptr ? o.help : o.label);
            return;
        }
        Adjust(o, dir);
        Speak(SpokenValue(o));
    }
    void ClampScroll() {
        int count = kCategories[mCategory].count;
        if (mCursor < mScrollTop) {
            mScrollTop = mCursor;
        }
        if (mCursor >= mScrollTop + kMaxVisible) {
            mScrollTop = mCursor - kMaxVisible + 1;
        }
        int maxTop = count - kMaxVisible;
        if (maxTop < 0) maxTop = 0;
        if (mScrollTop > maxTop) mScrollTop = maxTop;
        if (mScrollTop < 0) mScrollTop = 0;
    }

    int mCategory = 0;
    int mCursor = 0;
    int mScrollTop = 0;

    // Running cue demo (Help entries, Z button).
    const DemoStep* mDemo = nullptr;
    int mDemoLen = 0;
    int mDemoIndex = 0;
    int mDemoTimer = 0;
};

// --- C API ------------------------------------------------------------------

extern "C" void SettingsMenu_Open(int categoryId) {
    SettingsMenu::Instance().Open(categoryId);
}
extern "C" int SettingsMenu_HandleInput(unsigned short buttons) {
    return SettingsMenu::Instance().HandleInput(buttons);
}
extern "C" int SettingsMenu_RowCount(void) {
    return SettingsMenu::Instance().RowCount();
}
extern "C" int SettingsMenu_IsRowSelected(int i) {
    return SettingsMenu::Instance().RowSelected(i) ? 1 : 0;
}
extern "C" const char* SettingsMenu_Title(void) {
    return SettingsMenu::Instance().Title();
}
extern "C" void SettingsMenu_RowText(int i, char* out, int outSize) {
    SettingsMenu::Instance().RowText(i, out, outSize);
}
extern "C" int SettingsMenu_OpenCategoryId(void) {
    return SettingsMenu::Instance().OpenCategoryId();
}
extern "C" void SettingsMenu_TickDemo(void) {
    SettingsMenu::Instance().TickDemo();
}
extern "C" int SettingsMenu_DemoActive(void) {
    return SettingsMenu::Instance().DemoActive() ? 1 : 0;
}
