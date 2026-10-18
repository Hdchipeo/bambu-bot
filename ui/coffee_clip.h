#pragma once
#include <cstddef>
#include <cstdint>

namespace bambu::ui {
// Native-resolution, geometry-only frames. Palette + independent PackBits frames in flash.
struct CoffeeClip {
    static constexpr uint32_t kFrameMs=50, kIntroFrames=24, kLoopFrames=126;
    static constexpr uint32_t kFrames=kIntroFrames+kLoopFrames;
    static uint32_t FrameAt(uint32_t elapsed_ms);
    static int X();
    static int Y();
    static int Width();
    static int Height();
    static size_t FlashBytes();
    static bool Decode(bool green,uint32_t frame,uint16_t* output,size_t pixels);
    // Bounds-checked pure decoder, also used by host corruption tests.
    static bool DecodePacked(const uint8_t* data,size_t bytes,const uint16_t* palette,
                             uint16_t* output,size_t pixels);
};
}
