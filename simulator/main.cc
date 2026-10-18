#include "bambu_face.h"
#include "scene3d.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using bambu::ui::Describe;
using bambu::ui::Emotion;
using bambu::ui::FaceView;
using bambu::ui::Telemetry;
using bambu::ui::kBambuGreen;
using bambu::ui::Theme;
using bambu::studio::Scene;
using bambu::studio::SceneView;
using bambu::studio::DescribeScene;
namespace fs = std::filesystem;
static_assert(LVGL_VERSION_MAJOR==9 && LVGL_VERSION_MINOR==5 && LVGL_VERSION_PATCH==0,
              "Bambu Bot Studio is tested with LVGL 9.5.0");

namespace {
constexpr int kWidth=1160, kHeight=760, kCount=static_cast<int>(Emotion::Count), kScenes=static_cast<int>(Scene::Count);
uint32_t ticks=0;
lv_obj_t* Box(lv_obj_t* parent,int x,int y,int w,int h,uint32_t color,int radius=16) {
    auto obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_radius(obj,radius,0);
    lv_obj_remove_flag(obj,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    return obj;
}
lv_obj_t* Label(lv_obj_t* p,int x,int y,const char* text,const lv_font_t* font,uint32_t color) {
    auto label=lv_label_create(p);
    lv_label_set_text(label,text);
    lv_obj_set_pos(label,x,y);
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    return label;
}
void Border(lv_obj_t* obj,uint32_t color,int width=1) {
    lv_obj_set_style_border_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_border_width(obj,width,0);
}
lv_obj_t* Button(lv_obj_t* p,int x,int y,int w,int h,const char* text) {
    auto obj=Box(p,x,y,w,h,0x17251F,10);
    Border(obj,0x2A3A31);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(obj,lv_color_hex(0x2A4938),LV_STATE_PRESSED);
    auto label=Label(obj,12,0,text,&lv_font_montserrat_14,0xC3D1C7);
    lv_obj_align(label,LV_ALIGN_LEFT_MID,12,0);
    return obj;
}

struct Canvas {
    std::vector<uint32_t> draw=std::vector<uint32_t>(kWidth*kHeight);
    std::vector<uint32_t> pixels=std::vector<uint32_t>(kWidth*kHeight);
    lv_display_t* display=nullptr;
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    lv_indev_t* mouse=nullptr;
    int mouse_x=0,mouse_y=0;
    bool pressed=false;
    explicit Canvas(bool headless) {
        if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_EVENTS|(headless?0:SDL_INIT_VIDEO))<0) throw std::runtime_error(SDL_GetError());
        lv_init();
        lv_tick_set_cb([](){return ticks;});
        display=lv_display_create(kWidth,kHeight);
        lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
        lv_display_set_user_data(display,this);
        lv_display_set_buffers(display,draw.data(),nullptr,draw.size()*sizeof(uint32_t),LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t* area,uint8_t* data) {
            auto self=static_cast<Canvas*>(lv_display_get_user_data(d));
            auto src=reinterpret_cast<const uint32_t*>(data);
            int stride=lv_area_get_width(area);
            for (int y=area->y1;y<=area->y2;++y) {
                std::copy_n(src+(y-area->y1)*stride,stride,self->pixels.data()+y*kWidth+area->x1);
            }
            lv_display_flush_ready(d);
        });
        lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x0B120F),0);
        lv_obj_set_style_text_font(lv_screen_active(),&lv_font_montserrat_14,0);
        lv_obj_remove_flag(lv_screen_active(),LV_OBJ_FLAG_SCROLLABLE);
        if (!headless) {
            window=SDL_CreateWindow("Bambu Bot | Emotion Studio " BAMBU_STUDIO_VERSION,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                kWidth,kHeight,SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI);
            if (!window) throw std::runtime_error(SDL_GetError());
            renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
            if (!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
            if (!renderer) throw std::runtime_error(SDL_GetError());
            SDL_RenderSetLogicalSize(renderer,kWidth,kHeight);
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,kWidth,kHeight);
            if (!texture) throw std::runtime_error(SDL_GetError());
        }
        mouse=lv_indev_create();
        lv_indev_set_type(mouse,LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(mouse,display);
        lv_indev_set_mode(mouse,LV_INDEV_MODE_EVENT);
        lv_indev_set_user_data(mouse,this);
        lv_indev_set_read_cb(mouse,[](lv_indev_t* input,lv_indev_data_t* data) {
            auto self=static_cast<Canvas*>(lv_indev_get_user_data(input));
            data->point.x=self->mouse_x; data->point.y=self->mouse_y;
            data->state=self->pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
        });
    }
    void HandlePointerEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_MOUSEMOTION:
                // SDL_RenderSetLogicalSize already converts native/Retina coordinates.
                mouse_x=event.motion.x;mouse_y=event.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (event.button.button!=SDL_BUTTON_LEFT) return;
                mouse_x=event.button.x;mouse_y=event.button.y;
                pressed=event.type==SDL_MOUSEBUTTONDOWN;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event!=SDL_WINDOWEVENT_FOCUS_LOST) return;
                // Cancel an interrupted press instead of generating a click on focus loss.
                lv_indev_reset(mouse,nullptr);
                pressed=false;
                break;
            default:return;
        }
        // Preserve every edge, including down/up queued together between rendered frames.
        lv_indev_read(mouse);
    }
    void Frame(uint32_t now) {
        ticks=now;
        lv_timer_handler();
        if (renderer) {
            SDL_UpdateTexture(texture,nullptr,pixels.data(),kWidth*4);
            SDL_RenderClear(renderer); SDL_RenderCopy(renderer,texture,nullptr,nullptr); SDL_RenderPresent(renderer);
        }
    }
    void Save(const fs::path& file) {
        lv_obj_invalidate(lv_screen_active()); lv_refr_now(display);
        auto surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),kWidth,kHeight,32,kWidth*4,SDL_PIXELFORMAT_RGB888);
        if (!surface) throw std::runtime_error(SDL_GetError());
        if (SDL_SaveBMP(surface,file.string().c_str())<0) { SDL_FreeSurface(surface); throw std::runtime_error(SDL_GetError()); }
        SDL_FreeSurface(surface);
    }
    void SaveLcd(lv_obj_t* root,const fs::path& file) {
        lv_obj_invalidate(lv_screen_active());lv_refr_now(display);
        lv_area_t area;lv_obj_get_coords(root,&area);
        auto surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data()+area.y1*kWidth+area.x1,
            lv_area_get_width(&area),lv_area_get_height(&area),32,kWidth*4,SDL_PIXELFORMAT_RGB888);
        if (!surface) throw std::runtime_error(SDL_GetError());
        if (SDL_SaveBMP(surface,file.string().c_str())<0) {SDL_FreeSurface(surface);throw std::runtime_error(SDL_GetError());}
        SDL_FreeSurface(surface);
    }
    ~Canvas() {
        lv_deinit();
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

class Studio {
public:
    Studio() {
        root_=Box(lv_screen_active(),0,0,kWidth,kHeight,0x0B120F,0);
        auto logo=Box(root_,32,27,40,40,kBambuGreen,20);
        auto mark=Box(logo,9,11,22,17,kBambuGreen,5); Border(mark,0xFFFFFF,3);
        Box(logo,5,16,4,7,0xFFFFFF,1); Box(logo,31,16,4,7,0xFFFFFF,1);
        Box(logo,12,7,3,6,0xFFFFFF,1); Box(logo,16,32,9,2,0xFFFFFF,1);
        Box(logo,15,17,3,6,0xFFFFFF,1); Box(logo,23,17,3,6,0xFFFFFF,1);
        Label(root_,87,26,"Bambu Bot",&lv_font_montserrat_28,0xEFF6EE);
        Label(root_,88,60,"A little companion for every print.",&lv_font_montserrat_12,0x819D8C);
        auto tag=Box(root_,917,37,211,30,0x13241B,15);
        Box(tag,13,11,7,7,kBambuGreen,4);
        Label(tag,29,8,"EMOTION STUDIO  /  " BAMBU_STUDIO_VERSION,&lv_font_montserrat_12,0xB8D2BE);

        auto left=Box(root_,32,108,640,585,0x101D16,22); Border(left,0x22362A);
        Label(left,24,22,"DEVICE PREVIEW",&lv_font_montserrat_12,0x88A18D);
        round_button_=Button(left,291,14,142,32,"Round 466 x 466");
        rect_button_=Button(left,445,14,170,32,"Desk-bot 280 x 240");
        lv_obj_add_event_cb(round_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetRound(true);},LV_EVENT_CLICKED,this);
        lv_obj_add_event_cb(rect_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetRound(false);},LV_EVENT_CLICKED,this);
        black_button_=Button(root_,480,37,144,32,"Black / Green");
        green_button_=Button(root_,636,37,144,32,"Green / White");
        lv_obj_add_event_cb(black_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetTheme(Theme::Black);},LV_EVENT_CLICKED,this);
        lv_obj_add_event_cb(green_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetTheme(Theme::Green);},LV_EVENT_CLICKED,this);
        mode_button_=Button(root_,800,37,104,32,"3D scenes");
        lv_obj_add_event_cb(mode_button_,[](lv_event_t* e){auto self=static_cast<Studio*>(lv_event_get_user_data(e));self->Set3D(!self->mode3d_);},LV_EVENT_CLICKED,this);
        preview_panel_=left;
        caption_=Label(left,0,550,"",&lv_font_montserrat_12,0x8CA391);

        auto right=Box(root_,696,108,432,585,0x101D16,22); Border(right,0x22362A);
        heading_=Label(right,24,22,"Meet the moods",&lv_font_montserrat_24,0xEAF3E8);
        hint_=Label(right,24,57,"Pick a feeling. Watch the little details.",&lv_font_montserrat_12,0x8CA391);
        for (int i=0;i<kCount;++i) {
            auto& action=actions_[i]; action={this,i};
            auto button=Button(right,24+(i%2)*198,90+(i/2)*45,186,36,Describe(static_cast<Emotion>(i)).name);
            buttons_[i]=button;
            auto dot=Box(button,163,15,6,6,Describe(static_cast<Emotion>(i)).accent,3);
            dots_[i]=dot;
            lv_obj_add_event_cb(button,[](lv_event_t* e) {
                auto action=static_cast<Action*>(lv_event_get_user_data(e)); action->studio->Select(action->index);
            },LV_EVENT_CLICKED,&action);
        }
        for (int i=0;i<kScenes;++i) {
            scene_actions_[i]={this,i};
            scene_buttons_[i]=Button(right,24+(i%2)*198,90+(i/2)*36,186,32,DescribeScene(static_cast<Scene>(i)).name);
            lv_obj_add_flag(scene_buttons_[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_event_cb(scene_buttons_[i],[](lv_event_t* e){auto action=static_cast<Action*>(lv_event_get_user_data(e));action->studio->SelectScene(action->index);},LV_EVENT_CLICKED,&scene_actions_[i]);
        }
        Label(right,24,425,"PRINT PROGRESS",&lv_font_montserrat_12,0x88A18D);
        progress_text_=Label(right,350,422,"42%",&lv_font_montserrat_16,0xC4EACB);
        slider_=lv_slider_create(right);
        lv_obj_set_pos(slider_,28,461); lv_obj_set_size(slider_,376,5);
        lv_slider_set_range(slider_,0,100); lv_slider_set_value(slider_,42,LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider_,lv_color_hex(0x2D4535),LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider_,lv_color_hex(kBambuGreen),LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider_,lv_color_hex(0xDBF8DE),LV_PART_KNOB);
        lv_obj_set_style_pad_all(slider_,5,LV_PART_KNOB);
        lv_obj_add_event_cb(slider_,[](lv_event_t* e) {
            auto self=static_cast<Studio*>(lv_event_get_user_data(e));
            self->progress_=lv_slider_get_value(self->slider_); self->UpdateTelemetry();
        },LV_EVENT_VALUE_CHANGED,this);
        play_button_=Button(right,24,493,186,38,"Play sequence");
        motion_button_=Button(right,222,493,186,38,"Motion on");
        lv_obj_add_event_cb(play_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->ToggleAuto();},LV_EVENT_CLICKED,this);
        lv_obj_add_event_cb(motion_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->ToggleMotion();},LV_EVENT_CLICKED,this);
        Label(right,24,548,"DEMO data / No printer connection.",&lv_font_montserrat_12,0x728D7B);
        description_=Label(root_,48,710,"",&lv_font_montserrat_12,0x8CA391);
        Label(root_,680,710,"D 2D/3D / B Replay / SPACE Play / R / T / M",&lv_font_montserrat_12,0x728D7B);
        SetRound(true);
        Select(0);
    }
    ~Studio() { scene_.reset();face_.reset(); lv_obj_delete(root_); }
    void SetRound(bool round) {
        const auto scene_time=scene_?scene_->elapsed():0;
        round_=round;
        scene_.reset();
        face_.reset();
        if (bezel_) lv_obj_delete(bezel_);
        int w=round?466:280,h=round?466:240;
        bezel_=Box(preview_panel_,(640-w-18)/2,(585-h-18)/2+5,w+18,h+18,0x060D09,round?LV_RADIUS_CIRCLE:22);
        Border(bezel_,0x354A3B,2);
        face_=std::make_unique<FaceView>(bezel_,w,h,round);
        lv_obj_set_pos(face_->root(),9,9);
        face_->SetTheme(theme_);
        face_->SetEmotion(static_cast<Emotion>(selected_));
        scene_=std::make_unique<SceneView>(bezel_,w,h,round);
        lv_obj_set_pos(scene_->root(),9,9);
        scene_->SetScene(static_cast<Scene>(selected_scene_));
        face_->SetAnimated(motion_ && !mode3d_);
        scene_->SetAnimated(motion_ && mode3d_);
        scene_->RenderAt(scene_time);
        lv_obj_add_flag(mode3d_?face_->root():scene_->root(),LV_OBJ_FLAG_HIDDEN);
        Border(round_button_,round?kBambuGreen:0x2A3A31);
        Border(rect_button_,round?0x2A3A31:kBambuGreen);
        lv_label_set_text(caption_,round?"466 x 466 AMOLED  /  Circular safe area":"280 x 240 LCD  /  Desk-bot test hardware");
        lv_obj_align(caption_,LV_ALIGN_BOTTOM_MID,0,-17);
        SetTheme(theme_);
        UpdateTelemetry();
    }
    void SetTheme(Theme theme) {
        theme_=theme; face_->SetTheme(theme);scene_->SetTheme(theme);
        Border(black_button_,theme==Theme::Black?kBambuGreen:0x2A3A31);
        Border(green_button_,theme==Theme::Green?kBambuGreen:0x2A3A31);
    }
    void Set3D(bool enabled) {
        mode3d_=enabled;
        for (auto button:buttons_) {if(enabled)lv_obj_add_flag(button,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(button,LV_OBJ_FLAG_HIDDEN);}
        for (auto button:scene_buttons_) {if(enabled)lv_obj_remove_flag(button,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(button,LV_OBJ_FLAG_HIDDEN);}
        lv_obj_add_flag(enabled?face_->root():scene_->root(),LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(enabled?scene_->root():face_->root(),LV_OBJ_FLAG_HIDDEN);
        face_->SetAnimated(motion_ && !enabled);scene_->SetAnimated(motion_ && enabled);
        lv_label_set_text(heading_,enabled?"Little 3D adventures":"Meet the moods");
        lv_label_set_text(hint_,enabled?"Pick a story. Watch Bam come to life.":"Pick a feeling. Watch the little details.");
        SetButtonText(mode_button_,enabled?"2D moods":"3D scenes");
        if(enabled)SelectScene(selected_scene_);else Select(selected_);
    }
    void SelectScene(int index) {
        selected_scene_=(index+kScenes)%kScenes;scene_->SetScene(static_cast<Scene>(selected_scene_));
        for(int i=0;i<kScenes;++i)Border(scene_buttons_[i],i==selected_scene_?kBambuGreen:0x2A3A31);
        lv_label_set_text(description_,DescribeScene(static_cast<Scene>(selected_scene_)).description);last_cycle_=ticks;
    }
    void Next(int step) {if(mode3d_)SelectScene(selected_scene_+step);else Select(selected_+step);}
    void Replay() {if(mode3d_)scene_->Restart();}
    void Toggle3D() {Set3D(!mode3d_);}
    void ToggleTheme() { SetTheme(theme_==Theme::Black?Theme::Green:Theme::Black); }
    void Select(int index) {
        int previous=selected_;
        selected_=(index+kCount)%kCount;
        auto emotion=static_cast<Emotion>(selected_);
        if (emotion==Emotion::Completed) progress_=100;
        else if (previous==static_cast<int>(Emotion::Completed)) progress_=42;
        face_->SetEmotion(emotion);
        for (int i=0;i<kCount;++i) {
            bool active=i==selected_;
            lv_obj_set_style_bg_color(buttons_[i],lv_color_hex(active?0x263E2D:0x17251F),0);
            Border(buttons_[i],active?Describe(emotion).accent:0x2A3A31);
            lv_obj_set_style_opa(dots_[i],active?LV_OPA_COVER:LV_OPA_40,0);
        }
        lv_label_set_text(description_,Describe(emotion).description);
        UpdateTelemetry();
        last_cycle_=ticks;
    }
    void UpdateTelemetry() {
        Telemetry t;
        t.connected=selected_!=static_cast<int>(Emotion::Offline);
        t.progress=progress_; t.remaining_min=(100-progress_)*3/4;
        t.nozzle_c=220;t.bed_c=60;
        face_->SetTelemetry(t);scene_->SetTelemetry(t);
        char percent[12]; snprintf(percent,sizeof(percent),"%d%%",progress_);
        lv_label_set_text(progress_text_,percent);
        lv_slider_set_value(slider_,progress_,LV_ANIM_OFF);
    }
    void ToggleAuto() { auto_=!auto_; SetButtonText(play_button_,auto_?"Pause sequence":"Play sequence"); last_cycle_=ticks; }
    void ToggleMotion() { motion_=!motion_;face_->SetAnimated(motion_ && !mode3d_);scene_->SetAnimated(motion_ && mode3d_);SetButtonText(motion_button_,motion_?"Motion on":"Motion off"); }
    void Tick() { if (auto_ && ticks-last_cycle_>=(mode3d_?9000u:3500u)) Next(1); }
    SceneView& scene() {return *scene_;}
    lv_obj_t* mode_button() {return mode_button_;}
    lv_obj_t* scene_button(int i) {return scene_buttons_[i];}
    bool mode3d() const {return mode3d_;}
    int selected_scene() const {return selected_scene_;}
    lv_obj_t* black_button() {return black_button_;}
    lv_obj_t* green_button() {return green_button_;}
    int selected() const {return selected_;}
    bool round() const {return round_;}
    FaceView& face() {return *face_;}
    lv_obj_t* button(int i) {return buttons_[i];}
    lv_obj_t* round_button() {return round_button_;}
    lv_obj_t* rect_button() {return rect_button_;}
    lv_obj_t* slider() {return slider_;}
    lv_obj_t* play_button() {return play_button_;}
    lv_obj_t* motion_button() {return motion_button_;}
    int progress() const {return progress_;}
    bool playing() const {return auto_;}
private:
    struct Action {Studio* studio=nullptr;int index=0;};
    static void SetButtonText(lv_obj_t* button,const char* text) {lv_label_set_text(lv_obj_get_child(button,0),text);}
    lv_obj_t *root_=nullptr,*preview_panel_=nullptr,*bezel_=nullptr,*caption_=nullptr,*description_=nullptr;
    lv_obj_t *round_button_=nullptr,*rect_button_=nullptr,*progress_text_=nullptr,*slider_=nullptr,*play_button_=nullptr,*motion_button_=nullptr;
    lv_obj_t *black_button_=nullptr,*green_button_=nullptr;
    lv_obj_t *mode_button_=nullptr,*heading_=nullptr,*hint_=nullptr;
    bool mode3d_=false;
    int selected_scene_=0;
    std::array<lv_obj_t*,kScenes> scene_buttons_{};
    std::array<Action,kScenes> scene_actions_{};
    std::unique_ptr<SceneView> scene_;
    Theme theme_=Theme::Black;
    std::array<lv_obj_t*,kCount> buttons_{},dots_{};
    std::array<Action,kCount> actions_{};
    std::unique_ptr<FaceView> face_;
    int selected_=0,progress_=42;
    bool round_=true,motion_=true,auto_=false;
    uint32_t last_cycle_=0;
};

std::string AllText(lv_obj_t* obj) {
    std::string result;
    if (lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return result;
    if (lv_obj_check_type(obj,&lv_label_class)) result+=lv_label_get_text(obj);
    for (uint32_t i=0;i<lv_obj_get_child_count(obj);++i) result+='\n'+AllText(lv_obj_get_child(obj,static_cast<int>(i)));
    return result;
}
void Require(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}

void QueueButton(Uint32 type,int x,int y,Uint8 button=SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type=type;event.button.button=button;event.button.x=x;event.button.y=y;
    Require(SDL_PushEvent(&event)==1,"Could not queue SDL mouse event");
}
void DrainPointerEvents(Canvas& canvas) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) canvas.HandlePointerEvent(event);
}
void FastClick(Canvas& canvas,lv_obj_t* button) {
    lv_obj_update_layout(button);
    lv_area_t area;lv_obj_get_coords(button,&area);
    int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
    // Both edges enter the same SDL event batch, before any LVGL timer/frame runs.
    QueueButton(SDL_MOUSEBUTTONDOWN,x,y);QueueButton(SDL_MOUSEBUTTONUP,x,y);
    DrainPointerEvents(canvas);
    canvas.Frame(ticks+50);
}
void SelfTest(Canvas& canvas) {
    Studio studio;
    canvas.Frame(ticks+100);
    FastClick(canvas,studio.button(static_cast<int>(Emotion::Happy)));
    Require(studio.selected()==static_cast<int>(Emotion::Happy),"Fast SDL click lost before LVGL timer read");
    FastClick(canvas,studio.rect_button());
    Require(!studio.round(),"Fast click did not select Desk-bot display");
    FastClick(canvas,studio.round_button());
    Require(studio.round(),"Fast click did not select Round display");
    for (bool round : {false,true}) {
        studio.SetRound(round);canvas.Frame(ticks+50);
        for (Theme theme : {Theme::Green,Theme::Black}) {
            FastClick(canvas,theme==Theme::Green?studio.green_button():studio.black_button());
            Require(studio.face().theme()==theme,"Theme button did not change theme");
            const auto background=lv_color_to_u32(lv_obj_get_style_bg_color(studio.face().root(),LV_PART_MAIN)) & 0xFFFFFF;
            Require(background==(theme==Theme::Green?kBambuGreen:0),"Theme background color mismatch");
            for (int i=0;i<kCount;++i) {
                studio.Select(i);canvas.Frame(ticks+350);
                Require(studio.face().theme()==theme,"Emotion changed theme");
                if (!round) Require(AllText(studio.face().root()).find("DEMO")==std::string::npos,"Desk-bot must hide DEMO label");
            }
            studio.SetRound(!round);studio.SetRound(round);
            Require(studio.face().theme()==theme,"Display switch lost selected theme");
        }
    }
    studio.Select(0);
    FastClick(canvas,studio.motion_button());
    Require(!studio.face().animated(),"Mouse motion toggle failed");
    FastClick(canvas,studio.motion_button());
    Require(studio.face().animated(),"Mouse motion toggle did not resume");
    FastClick(canvas,studio.play_button());
    Require(studio.playing(),"Mouse sequence toggle failed");
    FastClick(canvas,studio.play_button());
    Require(!studio.playing(),"Mouse sequence toggle did not stop");
    lv_obj_update_layout(studio.slider());
    lv_area_t slider_area;lv_obj_get_coords(studio.slider(),&slider_area);
    const int slider_y=(slider_area.y1+slider_area.y2)/2;
    QueueButton(SDL_MOUSEBUTTONDOWN,slider_area.x1+lv_area_get_width(&slider_area)/4,slider_y);
    SDL_Event move{};move.type=SDL_MOUSEMOTION;
    move.motion.x=slider_area.x1+lv_area_get_width(&slider_area)*3/4;move.motion.y=slider_y;
    move.motion.state=SDL_BUTTON_LMASK;
    Require(SDL_PushEvent(&move)==1,"Could not queue SDL drag");
    QueueButton(SDL_MOUSEBUTTONUP,move.motion.x,move.motion.y);
    DrainPointerEvents(canvas);canvas.Frame(ticks+50);
    Require(studio.progress()>=70 && studio.progress()<=80,"Batched SDL slider drag lost");
    lv_area_t other_button;lv_obj_get_coords(studio.button(static_cast<int>(Emotion::Error)),&other_button);
    const int click_x=(other_button.x1+other_button.x2)/2,click_y=(other_button.y1+other_button.y2)/2;
    const int before_cancel=studio.selected();
    QueueButton(SDL_MOUSEBUTTONDOWN,click_x,click_y);
    SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    Require(SDL_PushEvent(&focus)==1,"Could not queue focus loss");
    QueueButton(SDL_MOUSEBUTTONUP,click_x,click_y);
    DrainPointerEvents(canvas);canvas.Frame(ticks+50);
    Require(studio.selected()==before_cancel,"Interrupted press activated a button");
    QueueButton(SDL_MOUSEBUTTONDOWN,click_x,click_y,SDL_BUTTON_RIGHT);
    QueueButton(SDL_MOUSEBUTTONUP,click_x,click_y,SDL_BUTTON_RIGHT);
    DrainPointerEvents(canvas);canvas.Frame(ticks+50);
    Require(studio.selected()==before_cancel,"Right click activated a left-click button");
    const auto nodes=lv_obj_get_child_count(studio.face().root());
    for (int pass=0;pass<3;++pass) {
        for (int i=0;i<kCount;++i) {
            FastClick(canvas,studio.button(i));
            Require(studio.selected()==i,"Emotion button failed");
            Require(studio.face().emotion()==static_cast<Emotion>(i),"Emotion state mismatch");
            for (int j=0;j<4;++j) canvas.Frame(ticks+100);
            Require(lv_obj_get_child_count(studio.face().root())==nodes,"Animation allocated unexpected objects");
        }
        studio.SetRound(!studio.round());
        canvas.Frame(ticks+100);
    }
    studio.Select(0);
    auto temperatures=AllText(studio.face().root());
    Require(temperatures.find("220 °C")!=std::string::npos && temperatures.find("60 °C")!=std::string::npos,"Temperature values/units missing");
    Require(temperatures.find("Nozzle")==std::string::npos && temperatures.find("Bed")==std::string::npos,"Temperature words should be replaced by icons");
    Telemetry unknown;unknown.connected=false;
    studio.face().SetTelemetry(unknown);
    auto text=AllText(studio.face().root());
    Require(text.find("disconnected")!=std::string::npos,"Offline status missing");
    Require(text.find("220")==std::string::npos,"Offline data leaked into UI");
    unknown.connected=true;unknown.stale=true;
    studio.face().SetTelemetry(unknown);
    Require(AllText(studio.face().root()).find("fresh data")!=std::string::npos,"Stale status missing");
    unknown.stale=false;
    studio.face().SetTelemetry(unknown);
    Require(AllText(studio.face().root()).find("--%")!=std::string::npos,"Missing progress not marked unknown");
    Require(AllText(studio.face().root()).find("-- °C")!=std::string::npos,"Missing temperature not marked unknown");
    studio.face().SetMessage("Activation code: 123456");
    Require(AllText(studio.face().root()).find("123456")!=std::string::npos,"Message missing");
    Require(AllText(studio.face().root()).find("°C")==std::string::npos,"Message must replace the temperature row");
    studio.face().SetMessage("");
    Require(AllText(studio.face().root()).find("-- °C")!=std::string::npos,"Temperature row did not return after message");
    studio.face().SetMinimal(true); studio.face().SetEmotion(Emotion::Happy);
    Require(AllText(studio.face().root()).find("°C")==std::string::npos,"Minimal face exposed temperatures after emotion change");
    studio.face().SetMinimal(false);
    studio.ToggleMotion();canvas.Frame(ticks+1000);studio.ToggleMotion();
    Require(bambu::ui::FromXiaozhi("neutral")==Emotion::Idle,"XiaoZhi alias mismatch");
    Require(bambu::ui::FromXiaozhi("shocked")==Emotion::Error,"XiaoZhi error alias mismatch");
    Require(bambu::ui::FromXiaozhi("unrecognized")==Emotion::Idle,"Unknown emotion fallback mismatch");
    const int before_cycle=studio.selected();
    studio.ToggleAuto();canvas.Frame(ticks+3600);studio.Tick();
    Require(studio.selected()==(before_cycle+1)%kCount,"Auto sequence failed");
    studio.ToggleAuto(); // Stop the old emotion sequence before testing 3D.
    FastClick(canvas,studio.mode_button());Require(studio.mode3d(),"3D mode button failed");
    for(bool round:{false,true})for(Theme theme:{Theme::Black,Theme::Green}) {
        studio.SetRound(round);studio.SetTheme(theme);studio.scene().SetAnimated(false);
        const auto nodes=lv_obj_get_child_count(studio.scene().root());
        for(int i=0;i<kScenes;++i) {
            FastClick(canvas,studio.scene_button(i));Require(studio.selected_scene()==i,"Scene button failed");
            for(uint32_t time:{0u,600u,1200u,2500u,5000u,9000u,17000u}) {
                studio.scene().RenderAt(time);canvas.Frame(ticks+33);
                Require(!studio.scene().overflowed(),"3D primitive pool exhausted");
                Require(studio.scene().primitive_count()>50,"3D mesh missing");
                Require(lv_obj_get_child_count(studio.scene().root())==nodes,"Scene created transient LVGL objects");
            }
            studio.Replay();Require(studio.scene().elapsed()==0,"2D to 3D replay failed");
        }
    }
    studio.SelectScene(1);
    Telemetry hud;hud.progress=42;hud.nozzle_c=220;hud.bed_c=60;studio.scene().SetTelemetry(hud);
    auto hud_text=AllText(studio.scene().root());
    Require(hud_text.find("42%")!=std::string::npos&&hud_text.find("220 °C")!=std::string::npos&&hud_text.find("60 °C")!=std::string::npos,"3D telemetry HUD values missing");
    hud.stale=true;studio.scene().SetTelemetry(hud);hud_text=AllText(studio.scene().root());
    Require(hud_text.find("220")==std::string::npos&&hud_text.find("-- °C")!=std::string::npos,"3D stale temperature leaked");
    hud.stale=false;hud.connected=false;studio.scene().SetTelemetry(hud);
    Require(AllText(studio.scene().root()).find("42%")==std::string::npos,"3D disconnected progress leaked");
    studio.scene().SetAnimated(false);studio.scene().RenderAt(2500);canvas.Frame(ticks+1000);
    Require(studio.scene().elapsed()==2500,"Paused scene advanced");
    studio.scene().SetAnimated(true);canvas.Frame(ticks+1000);studio.scene().SetAnimated(false);
    Require(studio.scene().elapsed()>2500,"Scene did not resume");
    const auto preserved_time=studio.scene().elapsed();studio.SetRound(!studio.round());
    Require(studio.scene().elapsed()==preserved_time,"Display switch reset scene time");
    FastClick(canvas,studio.mode_button());Require(!studio.mode3d(),"Return to 2D failed");
    std::cout<<"PASS: 18 3D scenes, replay, 2 themes, 2 displays, bounded mesh pools; fast SDL clicks on 14 emotions and both displays, batched slider drag, focus-loss cancellation, button filtering, animated transitions, bounded objects, both themes on both displays, compact layout without DEMO, stale/offline/unknown values, messages, reduced motion, aliases and sequence\n";
}

void Export(Canvas& canvas,const fs::path& output) {
    fs::create_directories(output);
    {
        Studio studio;
        studio.Select(static_cast<int>(Emotion::Printing));
        canvas.Frame(ticks+1000); canvas.Save(output/"studio-round.bmp");
        canvas.SaveLcd(studio.face().root(),output/"lcd-round.bmp");
        studio.SetRound(false); canvas.Frame(ticks+1000); canvas.Save(output/"studio-deskbot.bmp");
        canvas.SaveLcd(studio.face().root(),output/"lcd-deskbot.bmp");
        studio.SetTheme(Theme::Green); canvas.Frame(ticks+1000);
        canvas.Save(output/"studio-deskbot-green.bmp");canvas.SaveLcd(studio.face().root(),output/"lcd-deskbot-green.bmp");
        studio.SetRound(true);canvas.Frame(ticks+1000);
        canvas.Save(output/"studio-round-green.bmp");canvas.SaveLcd(studio.face().root(),output/"lcd-round-green.bmp");
        studio.SetTheme(Theme::Black);
        for (int i=0;i<kCount;++i) {
            studio.Select(i);canvas.Frame(ticks+1000);
            canvas.Save(output/(std::string("emotion-")+Describe(static_cast<Emotion>(i)).name+".bmp"));
            canvas.SaveLcd(studio.face().root(),output/(std::string("lcd-")+Describe(static_cast<Emotion>(i)).name+".bmp"));
        }
    }
    {
        Studio studio;studio.Set3D(true);studio.scene().SetAnimated(false);
        for(bool round:{true,false})for(Theme theme:{Theme::Black,Theme::Green}) {
            studio.SetRound(round);studio.SetTheme(theme);studio.scene().SetAnimated(false);
            for(int i=0;i<kScenes;++i) {
                studio.SelectScene(i);studio.scene().RenderAt(3200);canvas.Frame(ticks+33);
                std::string name="scene-"+std::to_string(i)+(round?"-round":"-deskbot")+(theme==Theme::Black?"-black":"-green");
                canvas.SaveLcd(studio.scene().root(),output/(name+".bmp"));
            }
        }
        studio.SetRound(true);studio.SetTheme(Theme::Black);studio.SelectScene(1);studio.scene().SetAnimated(false);
        studio.scene().RenderAt(3200);canvas.Frame(ticks+33);canvas.Save(output/"studio-3d.bmp");
        fs::create_directories(output/"animation");
        for(int i=0;i<80;++i) {
            studio.scene().RenderAt(i*80);canvas.Frame(ticks+1);
            char name[32];std::snprintf(name,sizeof(name),"frame-%03d.bmp",i);
            canvas.SaveLcd(studio.scene().root(),output/"animation"/name);
        }
    }
    {
        auto card=Box(lv_screen_active(),0,0,220,232,0xFFFFFF,0);
        {bambu::studio::PrinterIconView icon(card,180,0x000000,2);
         lv_obj_set_pos(icon.root(),20,20);canvas.Frame(ticks+1);canvas.SaveLcd(card,output/"printer-icon.bmp");}
        lv_obj_delete(card);
    }
    auto sheet=Box(lv_screen_active(),0,0,kWidth,kHeight,0x0B120F,0);
    Label(sheet,36,34,"One little bot. Fourteen ways to feel.",&lv_font_montserrat_28,0xEAF3E8);
    Label(sheet,36,79,"BAMBU BOT   /   CHARACTER STUDY   /   LVGL VECTOR COMPONENT",&lv_font_montserrat_12,0x88A18D);
    std::vector<std::unique_ptr<FaceView>> faces;
    for (int i=0;i<kCount;++i) {
        int x=28+(i%7)*160,y=140+(i/7)*260;
        auto face=std::make_unique<FaceView>(sheet,144,144,true);
        lv_obj_set_pos(face->root(),x,y); face->SetMinimal(true); face->SetEmotion(static_cast<Emotion>(i));face->SetAnimated(false);
        auto label=Label(sheet,x,y+160,Describe(static_cast<Emotion>(i)).name,&lv_font_montserrat_14,0xD3E5D7);
        lv_obj_set_width(label,144);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        auto dot=Box(sheet,x+67,y+190,8,3,Describe(static_cast<Emotion>(i)).accent,2);(void)dot;
        faces.push_back(std::move(face));
    }
    Label(sheet,36,692,"Bambu Green on black. White on Bambu Green. One face, fourteen emotions.",&lv_font_montserrat_14,0x88A18D);
    canvas.Frame(ticks+1000);canvas.Save(output/"emotion-sheet.bmp");
    faces.clear();lv_obj_delete(sheet);
    std::cout<<"LVGL renders exported to "<<output<<'\n';
}
}  // namespace

int main(int argc,char** argv) {
    bool headless=false,self_test=false;
    fs::path output;
    for (int i=1;i<argc;++i) {
        if (!std::strcmp(argv[i],"--self-test")) {headless=true;self_test=true;}
        else if (!std::strcmp(argv[i],"--headless")) headless=true;
        else if (!std::strcmp(argv[i],"--output") && i+1<argc) output=argv[++i];
        else {std::cerr<<"Usage: bambu_studio [--self-test] [--headless --output DIRECTORY]\n";return 2;}
    }
    if (headless && !self_test && output.empty()) {std::cerr<<"Headless mode requires --self-test or --output\n";return 2;}
    try {
        Canvas canvas(headless);
        if (self_test) SelfTest(canvas);
        if (!output.empty()) Export(canvas,output);
        if (headless) return 0;
        Studio studio;
        bool running=true;
        while (running) {
            ticks=SDL_GetTicks();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type==SDL_QUIT) running=false;
                canvas.HandlePointerEvent(event);
                if (event.type==SDL_KEYDOWN && !event.key.repeat) {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:running=false;break;
                        case SDLK_RIGHT:studio.Next(1);break;
                        case SDLK_LEFT:studio.Next(-1);break;
                        case SDLK_d:studio.Toggle3D();break;
                        case SDLK_b:studio.Replay();break;
                        case SDLK_SPACE:studio.ToggleAuto();break;
                        case SDLK_t:studio.ToggleTheme();break;
                        case SDLK_r:studio.SetRound(!studio.round());break;
                        case SDLK_m:studio.ToggleMotion();break;
                        default:break;
                    }
                }
            }
            canvas.Frame(SDL_GetTicks());studio.Tick();SDL_Delay(8);
        }
    } catch (const std::exception& error) {std::cerr<<"Bambu Bot Studio: "<<error.what()<<'\n';return 1;}
    return 0;
}
