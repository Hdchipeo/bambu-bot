#include "coffee_clip.h"
#include <algorithm>
#include "assets/coffee_clip.inc"

namespace bambu::ui {
uint32_t CoffeeClip::FrameAt(uint32_t ms) {
    const uint32_t frame=ms/kFrameMs;
    return frame<kIntroFrames?frame:kIntroFrames+(frame-kIntroFrames)%kLoopFrames;
}
int CoffeeClip::X(){return coffee_data::kX;}
int CoffeeClip::Y(){return coffee_data::kY;}
int CoffeeClip::Width(){return coffee_data::kWidth;}
int CoffeeClip::Height(){return coffee_data::kHeight;}
size_t CoffeeClip::FlashBytes(){return sizeof(coffee_data::kStream)+sizeof(coffee_data::kPalette)+sizeof(coffee_data::kOffsets);}
bool CoffeeClip::DecodePacked(const uint8_t* data,size_t bytes,const uint16_t* palette,uint16_t* output,size_t pixels) {
    if(!data||!palette||!output||!pixels)return false;
    size_t source=0,dest=0;
    while(source<bytes && dest<pixels) {
        const uint8_t command=data[source++];const size_t count=(command&127)+1;
        if(count>pixels-dest)return false;
        if(command&128) {
            if(source>=bytes)return false;
            std::fill_n(output+dest,count,palette[data[source++]]);dest+=count;
        }else {
            if(count>bytes-source)return false;
            for(size_t i=0;i<count;++i)output[dest++]=palette[data[source++]];
        }
    }
    return source==bytes && dest==pixels;
}
bool CoffeeClip::Decode(bool green,uint32_t frame,uint16_t* output,size_t pixels) {
    if(frame>=kFrames || pixels!=static_cast<size_t>(Width()*Height()))return false;
    const uint32_t index=(green?kFrames:0)+frame;
    const auto start=coffee_data::kOffsets[index],end=coffee_data::kOffsets[index+1];
    if(end<start||end>sizeof(coffee_data::kStream))return false;
    return DecodePacked(coffee_data::kStream+start,end-start,coffee_data::kPalette+(green?256:0),output,pixels);
}
}
