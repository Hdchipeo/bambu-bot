#include "voice_prompt.h"
#include <cstring>
namespace bambu {
namespace {uint32_t Read(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}}
bool VoicePrompt::Load(const uint8_t* data,size_t size){
    Clear();
    if(!data || size<24 || size>98304 || std::memcmp(data,"BBVOICE1",8) || Read(data+8)!=16000 || Read(data+12)!=60 || Read(data+20)!=size)return false;
    const size_t count=Read(data+16);if(count==0 || count>512)return false;
    size_t pos=24;
    for(size_t i=0;i<count;++i){
        if(pos+2>size)return false;
        const size_t length=size_t(data[pos])|(size_t(data[pos+1])<<8);pos+=2;
        if(!length || length>1275 || length>size-pos)return false;
        pos+=length;
    }
    if(pos!=size)return false;
    bytes_.assign(data,data+size);remaining_=count;return true;
}
bool VoicePrompt::Next(const uint8_t*& data,size_t& size){
    if(!remaining_)return false;
    size=size_t(bytes_[cursor_])|(size_t(bytes_[cursor_+1])<<8);cursor_+=2;
    data=bytes_.data()+cursor_;cursor_+=size;--remaining_;return true;
}
}
