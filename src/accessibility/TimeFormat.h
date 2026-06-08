#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace AccessibilityStrings {

// Internal value that means "no record set" (>= 100 minutes in centiseconds). Matches the
// game's MAX_TIME, used on the records screens to draw dashes instead of a time.
inline constexpr uint32_t kNoRecordTime = 0x927C0;

// Format a race time given in centiseconds (hundredths of a second) - the unit the game's
// lap timers and saved records use - as a spoken string, e.g. "1 minute 45.07" or
// "45.07 seconds". Centiseconds are zero-padded so 5 reads as "point zero five".
inline std::string FormatRaceTime(uint32_t centiseconds) {
    const uint32_t minutes = centiseconds / 6000;
    const uint32_t seconds = (centiseconds / 100) % 60;
    const uint32_t cc = centiseconds % 100;
    char buf[48];
    if (minutes > 0) {
        std::snprintf(buf, sizeof(buf), "%u minute%s %u.%02u", minutes, (minutes == 1) ? "" : "s", seconds, cc);
    } else {
        std::snprintf(buf, sizeof(buf), "%u.%02u seconds", seconds, cc);
    }
    return buf;
}

} // namespace AccessibilityStrings
