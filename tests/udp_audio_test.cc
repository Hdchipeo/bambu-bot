#include "udp_audio_rx.h"
#include <cassert>
#include <vector>
#include <iostream>
using namespace bambu::audio;
void Put32(std::vector<uint8_t>& data,size_t offset,uint32_t value){for(int i=0;i<4;++i)data[offset+i]=uint8_t(value>>(24-8*i));}
int main(){
    // An Opus silence frame may be only 3 bytes: the datagram is 19, not sizeof(string)+payload.
    std::vector<uint8_t> bytes(19,0);bytes[0]=1;bytes[3]=3;
    Put32(bytes,8,123456);Put32(bytes,12,1277);
    UdpAudioHeader header;assert(UdpAudioHeader::Parse(bytes.data(),bytes.size(),header));
    assert(header.timestamp==123456 && header.sequence==1277 && header.payload_bytes==3);
    std::vector<uint8_t> unaligned(1);unaligned.insert(unaligned.end(),bytes.begin(),bytes.end());
    assert(UdpAudioHeader::Parse(unaligned.data()+1,bytes.size(),header));
    assert(!UdpAudioHeader::Parse(bytes.data(),15,header));
    assert(!UdpAudioHeader::Parse(bytes.data(),18,header));
    bytes[3]=2;assert(!UdpAudioHeader::Parse(bytes.data(),bytes.size(),header));
    bytes[3]=0;assert(!UdpAudioHeader::Parse(bytes.data(),bytes.size(),header));
    bytes[3]=3;bytes[0]=2;assert(!UdpAudioHeader::Parse(bytes.data(),bytes.size(),header));
    UdpAudioSequence sequence;assert(sequence.Accept(1273));assert(sequence.Accept(1277));
    assert(sequence.gap_events==1 && sequence.gap_packets==3);
    assert(!sequence.Accept(1274));assert(!sequence.Accept(1277));
    assert(sequence.late_or_duplicate==2);assert(sequence.Accept(1278));assert(sequence.received==3);
    UdpAudioSequence wrap;assert(wrap.Accept(0xfffffffe));assert(wrap.Accept(0xffffffff));assert(wrap.Accept(0));assert(wrap.Accept(1));
    assert(wrap.gap_packets==0);assert(!wrap.Accept(0xffffffff));assert(wrap.late_or_duplicate==1);
    std::cout<<"PASS: UDP audio header bounds, short/unaligned packets, gaps, duplicate/late rejection and sequence wrap\n";
}
