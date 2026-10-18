#include "bambu_face.h"
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
namespace fs = std::filesystem;
static_assert(LVGL_VERSION_MAJOR==9 && LVGL_VERSION_MINOR==5 && LVGL_VERSION_PATCH==0,
              "Bambu Bot Studio is tested with LVGL 9.5.0");

namespace {
constexpr int kWidth=1160, kHeight=760, kCount=static_cast<int>(Emotion::Count);
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
        if (SDL_Init(headless?SDL_INIT_TIMER:(SDL_INIT_TIMER|SDL_INIT_VIDEO))<0) throw std::runtime_error(SDL_GetError());
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
            window=SDL_CreateWindow("Bambu Bot | Emotion Studio",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                kWidth,kHeight,SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI);
            if (!window) throw std::runtime_error(SDL_GetError());
            renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
            if (!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
            if (!renderer) throw std::runtime_error(SDL_GetError());
            SDL_RenderSetLogicalSize(renderer,kWidth,kHeight);
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,kWidth,kHeight);
            if (!texture) throw std::runtime_error(SDL_GetError());
            mouse=lv_indev_create();
            lv_indev_set_type(mouse,LV_INDEV_TYPE_POINTER);
            lv_indev_set_user_data(mouse,this);
            lv_indev_set_read_cb(mouse,[](lv_indev_t* input,lv_indev_data_t* data) {
                auto self=static_cast<Canvas*>(lv_indev_get_user_data(input));
                data->point.x=self->mouse_x; data->point.y=self->mouse_y;
                data->state=self->pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
            });
        }
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
        auto logo=Box(root_,32,27,40,40,0x98E7AF,20);
        auto mark=Box(logo,9,11,22,17,0x98E7AF,5); Border(mark,0x103624,3);
        Box(logo,5,16,4,7,0x103624,1); Box(logo,31,16,4,7,0x103624,1);
        Box(logo,12,7,3,6,0x103624,1); Box(logo,16,32,9,2,0x103624,1);
        Box(logo,15,17,3,6,0x103624,1); Box(logo,23,17,3,6,0x103624,1);
        Label(root_,87,26,"Bambu Bot",&lv_font_montserrat_28,0xEFF6EE);
        Label(root_,88,60,"A little companion for every print.",&lv_font_montserrat_12,0x819D8C);
        auto tag=Box(root_,917,37,211,30,0x13241B,15);
        Box(tag,13,11,7,7,0x9DE5A9,4);
        Label(tag,29,8,"EMOTION STUDIO  /  01",&lv_font_montserrat_12,0xB8D2BE);

        auto left=Box(root_,32,108,640,585,0x101D16,22); Border(left,0x22362A);
        Label(left,24,22,"DEVICE PREVIEW",&lv_font_montserrat_12,0x88A18D);
        round_button_=Button(left,291,14,142,32,"Round 466 x 466");
        rect_button_=Button(left,445,14,170,32,"Desk-bot 280 x 240");
        lv_obj_add_event_cb(round_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetRound(true);},LV_EVENT_CLICKED,this);
        lv_obj_add_event_cb(rect_button_,[](lv_event_t* e){static_cast<Studio*>(lv_event_get_user_data(e))->SetRound(false);},LV_EVENT_CLICKED,this);
        preview_panel_=left;
        caption_=Label(left,0,550,"",&lv_font_montserrat_12,0x8CA391);

        auto right=Box(root_,696,108,432,585,0x101D16,22); Border(right,0x22362A);
        Label(right,24,22,"Meet the moods",&lv_font_montserrat_24,0xEAF3E8);
        Label(right,24,57,"Pick a feeling. Watch the little details.",&lv_font_montserrat_12,0x8CA391);
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
        Label(right,24,425,"PRINT PROGRESS",&lv_font_montserrat_12,0x88A18D);
        progress_text_=Label(right,350,422,"42%",&lv_font_montserrat_16,0xC4EACB);
        slider_=lv_slider_create(right);
        lv_obj_set_pos(slider_,28,461); lv_obj_set_size(slider_,376,5);
        lv_slider_set_range(slider_,0,100); lv_slider_set_value(slider_,42,LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider_,lv_color_hex(0x2D4535),LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider_,lv_color_hex(0xA6E5AF),LV_PART_INDICATOR);
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
        Label(right,24,548,"Simulated telemetry. No printer connection.",&lv_font_montserrat_12,0x728D7B);
        description_=Label(root_,48,710,"",&lv_font_montserrat_12,0x8CA391);
        Label(root_,744,710,"SPACE  Play   /   R  Display   /   M  Motion",&lv_font_montserrat_12,0x728D7B);
        SetRound(true);
        Select(0);
    }
    ~Studio() { face_.reset(); lv_obj_delete(root_); }
    void SetRound(bool round) {
        round_=round;
        face_.reset();
        if (bezel_) lv_obj_delete(bezel_);
        int w=round?466:280,h=round?466:240;
        bezel_=Box(preview_panel_,(640-w-18)/2,(585-h-18)/2+5,w+18,h+18,0x060D09,round?LV_RADIUS_CIRCLE:22);
        Border(bezel_,0x354A3B,2);
        face_=std::make_unique<FaceView>(bezel_,w,h,round);
        lv_obj_set_pos(face_->root(),9,9);
        face_->SetEmotion(static_cast<Emotion>(selected_));
        face_->SetAnimated(motion_);
        Border(round_button_,round?0x8DB996:0x2A3A31);
        Border(rect_button_,round?0x2A3A31:0x8DB996);
        lv_label_set_text(caption_,round?"466 x 466 AMOLED  /  Circular safe area":"280 x 240 LCD  /  Desk-bot test hardware");
        lv_obj_align(caption_,LV_ALIGN_BOTTOM_MID,0,-17);
        UpdateTelemetry();
    }
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
        face_->SetTelemetry(t);
        char percent[12]; snprintf(percent,sizeof(percent),"%d%%",progress_);
        lv_label_set_text(progress_text_,percent);
        lv_slider_set_value(slider_,progress_,LV_ANIM_OFF);
    }
    void ToggleAuto() { auto_=!auto_; SetButtonText(play_button_,auto_?"Pause sequence":"Play sequence"); last_cycle_=ticks; }
    void ToggleMotion() { motion_=!motion_;face_->SetAnimated(motion_);SetButtonText(motion_button_,motion_?"Motion on":"Motion off"); }
    void Tick() { if (auto_ && ticks-last_cycle_>=3500) Select(selected_+1); }
    int selected() const {return selected_;}
    bool round() const {return round_;}
    FaceView& face() {return *face_;}
    lv_obj_t* button(int i) {return buttons_[i];}
private:
    struct Action {Studio* studio=nullptr;int index=0;};
    static void SetButtonText(lv_obj_t* button,const char* text) {lv_label_set_text(lv_obj_get_child(button,0),text);}
    lv_obj_t *root_=nullptr,*preview_panel_=nullptr,*bezel_=nullptr,*caption_=nullptr,*description_=nullptr;
    lv_obj_t *round_button_=nullptr,*rect_button_=nullptr,*progress_text_=nullptr,*slider_=nullptr,*play_button_=nullptr,*motion_button_=nullptr;
    std::array<lv_obj_t*,kCount> buttons_{},dots_{};
    std::array<Action,kCount> actions_{};
    std::unique_ptr<FaceView> face_;
    int selected_=0,progress_=42;
    bool round_=true,motion_=true,auto_=false;
    uint32_t last_cycle_=0;
};

std::string AllText(lv_obj_t* obj) {
    std::string result;
    if (lv_obj_check_type(obj,&lv_label_class)) result+=lv_label_get_text(obj);
    for (uint32_t i=0;i<lv_obj_get_child_count(obj);++i) result+='\n'+AllText(lv_obj_get_child(obj,static_cast<int>(i)));
    return result;
}
void Require(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}

void SelfTest(Canvas& canvas) {
    Studio studio;
    const auto nodes=lv_obj_get_child_count(studio.face().root());
    for (int pass=0;pass<3;++pass) {
        for (int i=0;i<kCount;++i) {
            lv_obj_send_event(studio.button(i),LV_EVENT_CLICKED,nullptr);
            Require(studio.selected()==i,"Emotion button failed");
            Require(studio.face().emotion()==static_cast<Emotion>(i),"Emotion state mismatch");
            for (int j=0;j<4;++j) canvas.Frame(ticks+100);
            Require(lv_obj_get_child_count(studio.face().root())==nodes,"Animation allocated unexpected objects");
        }
        studio.SetRound(!studio.round());
        canvas.Frame(ticks+100);
    }
    studio.Select(0);
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
    studio.face().SetMessage("Activation code: 123456");
    Require(AllText(studio.face().root()).find("123456")!=std::string::npos,"Message missing");
    studio.ToggleMotion();canvas.Frame(ticks+1000);studio.ToggleMotion();
    Require(bambu::ui::FromXiaozhi("neutral")==Emotion::Idle,"XiaoZhi alias mismatch");
    Require(bambu::ui::FromXiaozhi("shocked")==Emotion::Error,"XiaoZhi error alias mismatch");
    Require(bambu::ui::FromXiaozhi("unrecognized")==Emotion::Idle,"Unknown emotion fallback mismatch");
    studio.ToggleAuto();canvas.Frame(ticks+3600);studio.Tick();
    Require(studio.selected()==1,"Auto sequence failed");
    std::cout<<"PASS: 14 emotion buttons, animated transitions, display switching, bounded objects, stale/offline/unknown values, messages, reduced motion, aliases and sequence\n";
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
        studio.SetRound(true);
        for (int i=0;i<kCount;++i) {
            studio.Select(i);canvas.Frame(ticks+1000);
            canvas.Save(output/(std::string("emotion-")+Describe(static_cast<Emotion>(i)).name+".bmp"));
            canvas.SaveLcd(studio.face().root(),output/(std::string("lcd-")+Describe(static_cast<Emotion>(i)).name+".bmp"));
        }
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
    Label(sheet,36,692,"White silhouette. Forest greens. A familiar face, with room for a little personality.",&lv_font_montserrat_14,0x88A18D);
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
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type==SDL_QUIT) running=false;
                if (event.type==SDL_MOUSEMOTION) {canvas.mouse_x=event.motion.x;canvas.mouse_y=event.motion.y;}
                if (event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP) {
                    canvas.mouse_x=event.button.x;canvas.mouse_y=event.button.y;
                    canvas.pressed=event.type==SDL_MOUSEBUTTONDOWN;
                }
                if (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) canvas.pressed=false;
                if (event.type==SDL_KEYDOWN && !event.key.repeat) {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:running=false;break;
                        case SDLK_RIGHT:studio.Select(studio.selected()+1);break;
                        case SDLK_LEFT:studio.Select(studio.selected()-1);break;
                        case SDLK_SPACE:studio.ToggleAuto();break;
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
