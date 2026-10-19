#pragma once
#include <cstddef>
#include <cstdint>

namespace bambu::audio {
struct UdpAudioHeader {
    static constexpr size_t kBytes=16;
    uint32_t timestamp=0,sequence=0;
    uint16_t payload_bytes=0;
    static uint32_t Read32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|uint32_t(p[3]);}
    static bool Parse(const uint8_t* data,size_t size,UdpAudioHeader& out){
        if(!data || size<=kBytes || data[0]!=1)return false;
        const auto payload=uint16_t(uint16_t(data[2])<<8|data[3]);
        if(!payload || size-kBytes!=payload)return false;
        out={Read32(data+8),Read32(data+12),payload};return true;
    }
};
// Tracks observed gaps, not proven network loss: late packets may arrive afterwards.
class UdpAudioSequence {
public:
    bool Accept(uint32_t sequence){
        if(!initialized_){initialized_=true;last_=sequence;++received;return true;}
        const uint32_t delta=sequence-last_;
        if(delta==0 || delta>=0x80000000u){++late_or_duplicate;return false;}
        if(delta>1){++gap_events;gap_packets+=delta-1;}
        last_=sequence;++received;return true;
    }
    uint32_t received=0,gap_events=0,gap_packets=0,late_or_duplicate=0;
private:
    bool initialized_=false;
    uint32_t last_=0;
};
}
