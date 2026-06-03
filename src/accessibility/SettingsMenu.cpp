#include "SettingsMenu.h"

#include "ScreenReaderService.h"
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
}

namespace {

// Controller bit masks (mirror of CONT_* in libultra os.h, to keep defines.h out
// of this C++ TU). The caller passes buttonPressed|stickPressed (edge-triggered).
constexpr unsigned short kBtnA = 0x8000;
constexpr unsigned short kBtnB = 0x4000;
constexpr unsigned short kBtnUp = 0x0800;
constexpr unsigned short kBtnDown = 0x0400;
constexpr unsigned short kBtnLeft = 0x0200;
constexpr unsigned short kBtnRight = 0x0100;

// Audio sequence players (mirror of SEQ_PLAYER_* in audio/external.h).
constexpr unsigned char kSeqLevel = 0; // background music
constexpr unsigned char kSeqEnv = 1;   // environment
constexpr unsigned char kSeqSfx = 2;   // sound effects

// How many option rows are visible at once (the rest scroll).
constexpr int kMaxVisible = 9;

enum class OptKind { Toggle, IntSlider, Enum, FloatSlider };

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
};

struct Category {
    const char* name;
    const Option* options;
    int count;
};

// --- Sound-category side effects / non-CVar accessors -----------------------

void ApplyMusicVolume() {
    audio_set_player_volume(kSeqLevel, CVarGetFloat("gMainMusicVolume", 1.0f));
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

// --- Option label tables ----------------------------------------------------

const char* const kPanModeLabels[] = { "Curve direction", "Racing line" };
const char* const kSoundModeLabels[] = { "Stereo", "Headphones", "Surround", "Mono" };

// --- Accessibility category (mirrors DrawAccessibilityMenu metadata) ---------

const Option kAccessibility[] = {
    { .label = "Enable accessibility", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_ENABLED,
      .idefault = CVAR_ACCESS_ENABLED_DEFAULT },
    { .label = "Screen reader", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_SCREEN_READER,
      .idefault = CVAR_ACCESS_SCREEN_READER_DEFAULT },
    { .label = "Narrate menus", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_MENU_NARRATION,
      .idefault = CVAR_ACCESS_MENU_NARRATION_DEFAULT },
    { .label = "Narrate races", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_RACE_NARRATION,
      .idefault = CVAR_ACCESS_RACE_NARRATION_DEFAULT },
    { .label = "Off-road cue", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_OFFROAD_CUE,
      .idefault = CVAR_ACCESS_OFFROAD_CUE_DEFAULT },
    { .label = "Blind drive assist", .kind = OptKind::Toggle, .cvar = CVAR_ACCESS_DRIVE_ASSIST,
      .idefault = CVAR_ACCESS_DRIVE_ASSIST_DEFAULT },
    { .label = "Pan mode", .kind = OptKind::Enum, .cvar = CVAR_ACCESS_DRIVE_PAN_MODE,
      .idefault = CVAR_ACCESS_DRIVE_PAN_MODE_DEFAULT, .labels = kPanModeLabels, .labelCount = 2 },
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

#define ARRAY_LEN(a) (static_cast<int>(sizeof(a) / sizeof((a)[0])))

const Category kCategories[] = {
    { "Accessibility", kAccessibility, ARRAY_LEN(kAccessibility) },
    { "Sound", kSound, ARRAY_LEN(kSound) },
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
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", v);
            return buf;
        }
    }
    return "";
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
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::lround(v * 100.0f)));
            return buf;
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
        Speak(std::string(cat.name) + ". " + first.label + ": " + SpeechValue(first));
    }

    int HandleInput(unsigned short b) {
        const Category& cat = kCategories[mCategory];

        if (b & kBtnB) {
            return 0x1 | 0x8; // exit + go-back SFX (MenuNarrator re-announces the row)
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
            Adjust(cat.options[mCursor], -1);
            Speak(SpeechValue(cat.options[mCursor]));
            return 0x2;
        }
        if ((b & kBtnRight) || (b & kBtnA)) {
            Adjust(cat.options[mCursor], +1);
            Speak(SpeechValue(cat.options[mCursor]));
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

  private:
    SettingsMenu() = default;

    void Speak(const std::string& text) {
        ScreenReaderService::Instance().Speak(text, true);
    }
    void SpeakCursor() {
        const Option& o = kCategories[mCategory].options[mCursor];
        Speak(std::string(o.label) + ": " + SpeechValue(o));
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
