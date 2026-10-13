// rt::set_link_single: LINK ID (byte 11) to SINGLE with the EEPROM's CRC
// (CRC-16/XMODEM of bytes 10-127, low byte first), and nothing else touched.
// Synthetic settings only: no game data.
#include "runtime/link_settings.h"

#include <cstdio>

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    const uint8_t standard[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(rt::crc16_xmodem(standard, sizeof standard) == 0x31C3, "CRC-16/XMODEM check value");

    std::array<uint8_t, 128> blank;
    blank.fill(0xff);
    auto e = blank;
    check(!rt::set_link_single(e) && e == blank, "a blank EEPROM is left alone");

    std::array<uint8_t, 128> s{};
    std::memcpy(s.data(), "SEGA", 4);
    for (size_t i = 12; i < s.size(); i++) s[i] = uint8_t(i * 7);
    s[11] = 1; // MASTER
    auto before = s;
    check(rt::set_link_single(s), "MASTER is changed");
    check(s[11] == 0, "LINK ID is SINGLE");
    const uint16_t crc = rt::crc16_xmodem(s.data() + 10, 118);
    check(s[8] == uint8_t(crc) && s[9] == uint8_t(crc >> 8), "CRC of bytes 10-127, low byte first");
    bool others = true;
    for (size_t i = 0; i < s.size(); i++)
        if (i != 8 && i != 9 && i != 11) others &= s[i] == before[i];
    check(others, "no other byte changes");
    before = s;
    check(!rt::set_link_single(s) && s == before, "SINGLE is left alone");

    if (failures) return 1;
    std::printf("test_link_settings: ok\n");
    return 0;
}
