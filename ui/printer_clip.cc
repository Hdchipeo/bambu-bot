#include "printer_clip.h"
#include "coffee_clip.h"
#include "assets/printer_clip.inc"

namespace bambu::ui {
uint32_t PrinterClip::FrameAt(uint32_t ms) {
    const uint32_t frame=ms/kFrameMs;
    return frame<kIntroFrames?frame:kIntroFrames+(frame-kIntroFrames)%kLoopFrames;
}
int PrinterClip::X(){return printer_data::kX;}
int PrinterClip::Y(){return printer_data::kY;}
int PrinterClip::Width(){return printer_data::kWidth;}
int PrinterClip::Height(){return printer_data::kHeight;}
size_t PrinterClip::FlashBytes(){return sizeof(printer_data::kStream)+sizeof(printer_data::kPalette)+sizeof(printer_data::kOffsets);}
bool PrinterClip::Decode(bool green,uint32_t frame,uint16_t* output,size_t pixels) {
    if(frame>=kFrames || pixels!=static_cast<size_t>(Width()*Height()))return false;
    const uint32_t index=(green?kFrames:0)+frame;
    const auto start=printer_data::kOffsets[index],end=printer_data::kOffsets[index+1];
    if(end<start||end>sizeof(printer_data::kStream))return false;
    return CoffeeClip::DecodePacked(printer_data::kStream+start,end-start,printer_data::kPalette+(green?256:0),output,pixels);
}
}
