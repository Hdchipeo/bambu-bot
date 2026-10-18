#pragma once
#include <cstdint>
#include <string>
namespace bambu {
struct HmsFault {
    uint32_t attr=0, code=0;
    // Only the AMS A firmware mismatch is expected with this BMCU setup.
    bool IgnoredForBmcu(bool enabled) const { return enabled && attr==0x05000400 && code==0x00010044; }
    std::string Key() const;
    std::string DisplayCode() const;
    const char* Severity() const;
    bool operator==(const HmsFault& other) const { return attr==other.attr && code==other.code; }
    bool operator<(const HmsFault& other) const { return attr<other.attr || (attr==other.attr && code<other.code); }
};
}
