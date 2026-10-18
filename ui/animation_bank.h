#pragma once
#include <cstddef>
#include <cstdint>

namespace bambu::ui {
// 18 activity clips followed by 14 head-only emotion clips; black/green themes.
// Source memory must outlive playback. Call SetData/Decode on the LVGL thread/lock.
class AnimationBank {
public:
    struct Clip { uint32_t id=0,theme=0,x=0,y=0,width=0,height=0,frames=0,intro=0,frame_ms=0,table=0; };
    static constexpr uint32_t kSceneCount=18,kEmotionCount=14,kClipCount=32;
    static bool SetData(const uint8_t* data,size_t size);
    static bool Get(uint32_t id,bool green,Clip& clip);
    static uint32_t FrameAt(const Clip& clip,uint32_t elapsed_ms);
    static bool Decode(const Clip& clip,uint32_t frame,uint16_t* output,size_t pixels);
    static size_t Size();
    static size_t MaxPixels();
    static uint32_t Generation();
};
}
