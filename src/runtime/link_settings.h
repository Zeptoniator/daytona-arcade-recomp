// Revision A's LINK ID setting, for a cabinet that is not linked. Its factory
// settings (written by the game into a blank EEPROM, by frame 4) are LINK ID
// MASTER, CABINET TWIN: it then waits at "NETWORK CHECKING" for a second
// cabinet, for ever when link play is off. Frontends without an F2 key leave
// players there, so with link play off the frontend sets LINK ID to SINGLE,
// as an operator would in test mode (GAME SYSTEM).
//
// Measured with m2run (--save-nvram, --nvram) on daytona: test mode's LINK ID
// cycles MASTER, SLAVE, SINGLE and is byte 11 of the EEPROM: MASTER 1, SINGLE 0
// (SLAVE not recorded); CABINET (TWIN 1, UPLIGHT 2) is bytes 26 and 124.
// Bytes 8-9 are the CRC-16/XMODEM (poly 0x1021, init 0) of bytes 10-127, low
// byte first: it matches the game's own for factory and single-cabinet
// settings, and the game keeps an EEPROM patched this way (with a wrong CRC it
// would write its factory settings again). Only LINK ID needs to change: with
// CABINET left TWIN the game reaches its attract mode before frame 1000.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace rt {

// The EEPROM holds the game's settings (the game writes "SEGA" first).
inline bool eeprom_has_settings(const std::array<uint8_t, 128> &e) { return !std::memcmp(e.data(), "SEGA", 4); }

inline uint16_t crc16_xmodem(const uint8_t *p, size_t n) {
    uint16_t crc = 0;
    while (n--) {
        crc ^= uint16_t(*p++ << 8);
        for (int i = 0; i < 8; i++) crc = crc & 0x8000 ? uint16_t(crc << 1 ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}

// Sets LINK ID to SINGLE and the CRC with it. False (nothing changed) when the
// EEPROM has no settings yet or LINK ID already is SINGLE.
inline bool set_link_single(std::array<uint8_t, 128> &e) {
    constexpr size_t kLinkId = 11, kCrc = 8, kFrom = 10;
    if (!eeprom_has_settings(e) || e[kLinkId] == 0) return false;
    e[kLinkId] = 0;
    const uint16_t crc = crc16_xmodem(e.data() + kFrom, e.size() - kFrom);
    e[kCrc] = uint8_t(crc);
    e[kCrc + 1] = uint8_t(crc >> 8);
    return true;
}

} // namespace rt
