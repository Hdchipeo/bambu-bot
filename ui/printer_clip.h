#pragma once
#include <cstddef>
#include <cstdint>

namespace bambu::ui {
// Native-resolution, geometry-only frames. Palette + independent PackBits frames in flash.
struct PrinterClip {
    static constexpr uint32_t kFrameMs=50, kIntroFrames=24, kLoopFrames=64;
    static constexpr uint32_t kFrames=kIntroFrames+kLoopFrames;
    static uint32_t FrameAt(uint32_t elapsed_ms);
    static int X();
    static int Y();
    static int Width();
    static int Height();
    static size_t FlashBytes();
    static bool Decode(bool green,uint32_t frame,uint16_t* output,size_t pixels);
};
}
