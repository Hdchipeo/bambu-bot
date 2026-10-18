#pragma once
#include "bambu_face.h"
#include "printer_icon.h"
#include <memory>
#include <array>
#include <cstddef>
#include <vector>

namespace bambu::ui {
enum class Scene { Game, Coffee, Music, Explore, Nap, Fly, Printer, Complete, Race,
    BubbleTea, Chill, Error, Dance, Plant, PaperPlane, Magic, Stretch, Wave, Count };
struct SceneInfo { const char* name; const char* description; };
const SceneInfo& DescribeScene(Scene scene);

// Experimental CPU mesh rasterizer presented as a dynamic LVGL image. No OpenGL or assets.
// Real 3D vertices, perspective projection, lighting and per-pixel depth testing.
class SceneView {
public:
    SceneView(lv_obj_t* parent,int width,int height,bool round,int render_divisor=1,uint32_t frame_period_ms=33);
    ~SceneView();
    SceneView(const SceneView&)=delete;
    SceneView& operator=(const SceneView&)=delete;
    lv_obj_t* root() const { return root_; }
    void SetScene(Scene scene);
    void SetEmotion(Emotion emotion);
    Scene scene() const { return scene_; }
    void SetTheme(ui::Theme theme);
    void SetTelemetry(const ui::Telemetry& telemetry);
    void SetAnimated(bool animated);
    bool animated() const { return animated_; }
    void Restart();
    void RenderAt(uint32_t elapsed_ms);
    uint32_t elapsed() const { return elapsed_; }
    size_t primitive_count() const { return count_; }
    size_t raster_storage_bytes() const { return pixels_.size()*sizeof(uint32_t)+depth_.size()*sizeof(float); }
    bool overflowed() const { return overflow_; }
private:
    struct V { float x,y,z; };
    struct Item { std::array<V,3> p; uint32_t color; float depth,width; uint8_t opacity; bool line; };
    V Transform(V v) const;
    void Triangle(V a,V b,V c,uint32_t color,float opacity=1,bool lit=true);
    void Segment(V a,V b,float width,uint32_t color,float opacity=1);
    void Box(V center,V size,uint32_t color,float opacity=1,bool local=false);
    void RobotBox(V center,V size,uint32_t color,float opacity=1);
    void Disc(V center,float rx,float ry,uint32_t color,float opacity=1);
    void Cup(V center,float radius,float height,bool tea);
    void Head(float morph,float time);
    void Build();
    void Draw(lv_layer_t* layer);
    void CreateTelemetryHud();
    void UpdateTelemetryHud();
    lv_obj_t *root_=nullptr,*title_=nullptr,*footer_=nullptr;
    std::unique_ptr<PrinterIconView> printer_icon_;
    lv_obj_t *hud_=nullptr,*progress_bar_=nullptr;
    std::array<lv_obj_t*,3> values_{};
    std::array<std::array<lv_point_precise_t,10>,12> hud_points_{};
    lv_timer_t* timer_=nullptr;
    lv_image_dsc_t image_{};
    std::vector<uint32_t> pixels_;
    std::vector<float> depth_;
    bool round_=false;
    std::array<Item,3072> items_{};
    size_t count_=0;
    bool overflow_=false,animated_=true;
    int width_=0,height_=0,render_width_=0,render_height_=0,render_divisor_=1;
    float scale_=1,yaw_=0,pitch_=0,shift_=0,bob_=0,morph_=0,time_=0;
    uint32_t elapsed_=0,last_tick_=0,ink_=ui::kBambuGreen,bg_=0;
    Emotion emotion_=Emotion::Idle;
    Scene scene_=Scene::Game;
    ui::Theme theme_=ui::Theme::Black;
    ui::Telemetry telemetry_;
};
}
