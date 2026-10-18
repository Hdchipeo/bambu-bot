#pragma once
#include <cstdint>
#include <string>
namespace bambu {
struct HmsFault {
    uint32_t attr=0, code=0;
    std::string Key() const;
    std::string DisplayCode() const;
    const char* Severity() const;
    bool operator==(const HmsFault& other) const { return attr==other.attr && code==other.code; }
    bool operator<(const HmsFault& other) const { return attr<other.attr || (attr==other.attr && code<other.code); }
};
}
