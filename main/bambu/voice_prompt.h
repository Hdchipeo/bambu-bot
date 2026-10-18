#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace bambu {
// Owned copy: asset OTA/unmapping cannot invalidate an in-flight prompt.
class VoicePrompt {
public:
    bool Load(const uint8_t* data,size_t size);
    bool Next(const uint8_t*& data,size_t& size);
    void Clear(){bytes_.clear();cursor_=24;remaining_=0;}
    size_t Remaining() const {return remaining_;}
private:
    std::vector<uint8_t> bytes_;
    size_t cursor_=24,remaining_=0;
};
}
