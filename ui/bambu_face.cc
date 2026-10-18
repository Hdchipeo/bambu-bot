#include "bambu_face.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bambu::ui {
namespace {
constexpr std::array<EmotionInfo, static_cast<size_t>(Emotion::Count)> kInfo{{
    {"Idle", "A quiet companion. Gentle breathing and a curious little blink.", 0x86E6A8},
    {"Listening", "Eyes open, attention on you. A soft blue listening pulse.", 0x73DBF5},
    {"Thinking", "A sideways glance and three little moving thoughts.", 0xBAA0F8},
    {"Speaking", "A small animated voice, with a face that stays attentive.", 0x8DE8CF},
    {"Printing", "Keeping an eye on the job. Progress stays visible below.", 0x71E4A0},
    {"Happy", "Smiling eyes and a tiny bounce. A little maker joy.", 0xC6EE8F},
    {"Completed", "The print is ready. Happy eyes, a smile and a celebration.", 0xC6F48D},
    {"Curious", "One eye raised. Ready to look a little closer.", 0x9BEAC9},
    {"Confused", "An uneven glance. Time to ask a useful follow-up.", 0xE5D5A1},
    {"Sleepy", "Slow breathing, resting eyes. Quiet company after hours.", 0xA5B7DF},
    {"Sad", "Soft eyes, a little tear. Something did not go as planned.", 0x8EBFE8},
    {"Warning", "Something needs attention. Amber, calm and clearly visible.", 0xF3CE80},
    {"Error", "The printer reported an error. Distinct eyes, no flashing.", 0xF08E89},
    {"Offline", "Connection is missing. No live values are implied.", 0x8C9F9B},
}};

lv_obj_t* Box(lv_obj_t* parent, uint32_t color, int radius = 0) {
    auto obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    return obj;
}
lv_obj_t* Label(lv_obj_t* parent, uint32_t color) {
    auto label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}
lv_obj_t* Line(lv_obj_t* parent, int width) {
    auto line = lv_line_create(parent);
    lv_obj_set_style_line_color(line, lv_color_hex(0xEDFFF5), 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}
void Visible(lv_obj_t* obj, bool visible) {
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
float Mix(float a, float b, float fraction) { return a + (b - a) * fraction; }
uint32_t Tint(uint32_t color, float strength) {
    auto channel = [strength](uint32_t c) { return static_cast<uint32_t>(c * strength); };
    return (channel((color >> 16) & 255) << 16) | (channel((color >> 8) & 255) << 8) | channel(color & 255);
}
}  // namespace

const EmotionInfo& Describe(Emotion e) {
    const auto i = static_cast<size_t>(e);
    return kInfo[i < kInfo.size() ? i : 0];
}
Emotion FromXiaozhi(const std::string& name) {
    for (size_t i = 0; i < kInfo.size(); ++i) {
        std::string lower = kInfo[i].name;
        for (auto& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (lower == name) return static_cast<Emotion>(i);
    }
    if (name == "neutral" || name == "microchip_ai") return Emotion::Idle;
    if (name == "laughing" || name == "loving" || name == "winking" || name == "cool") return Emotion::Happy;
    if (name == "crying" || name == "depressed") return Emotion::Sad;
    if (name == "angry") return Emotion::Warning;
    if (name == "surprised") return Emotion::Curious;
    if (name == "shocked") return Emotion::Error;
    if (name == "embarrassed") return Emotion::Confused;
    if (name == "sleeping") return Emotion::Sleepy;
    return Emotion::Idle;
}

int FaceView::S(float v) const { return static_cast<int>(std::lround(v * scale_)); }
void FaceView::Rect(lv_obj_t* obj, float x, float y, float w, float h) {
    lv_obj_set_pos(obj, offset_x_ + S(x), offset_y_ + S(y));
    lv_obj_set_size(obj, std::max(1, S(w)), std::max(1, S(h)));
}

FaceView::FaceView(lv_obj_t* parent, int w, int h, bool round) : round_(round) {
    scale_ = std::min(w / 320.0f, h / 320.0f);
    offset_x_ = (w - S(320)) / 2;
    offset_y_ = (h - S(320)) / 2;
    root_ = Box(parent, 0x071A14, round ? LV_RADIUS_CIRCLE : 0);
    lv_obj_set_size(root_, w, h);
#if LV_FONT_MONTSERRAT_12 && LV_FONT_MONTSERRAT_14
    lv_obj_set_style_text_font(root_, w<320 ? &lv_font_montserrat_12 : &lv_font_montserrat_14,0);
#endif
    lv_obj_set_style_clip_corner(root_, true, 0);
    lv_obj_set_style_bg_grad_color(root_, lv_color_hex(0x0C241B), 0);
    lv_obj_set_style_bg_grad_dir(root_, LV_GRAD_DIR_VER, 0);

    halo_ = Box(root_, 0x12382A, LV_RADIUS_CIRCLE);
    Rect(halo_, 40, 61, 240, 240);
    halo_inner_ = Box(root_, 0x17442F, LV_RADIUS_CIRCLE);
    Rect(halo_inner_, 58, 78, 204, 204);
    lv_obj_set_style_bg_grad_color(halo_inner_, lv_color_hex(0x0A281C), 0);
    lv_obj_set_style_bg_grad_dir(halo_inner_, LV_GRAD_DIR_VER, 0);

    ring_ = lv_arc_create(root_);
    lv_obj_set_pos(ring_, offset_x_ + S(7), offset_y_ + S(7));
    lv_obj_set_size(ring_, S(306), S(306));
    lv_arc_set_bg_angles(ring_, 135, 45);
    lv_arc_set_range(ring_, 0, 100);
    lv_obj_set_style_arc_width(ring_, std::max(2, S(2.5f)), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_, lv_color_hex(0x264B39), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring_, std::max(2, S(3)), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring_, true, LV_PART_INDICATOR);
    lv_obj_remove_style(ring_, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(ring_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    Visible(ring_, round);

    bar_ = lv_bar_create(root_);
    Rect(bar_, 88, 264, 144, 3);
    lv_bar_set_range(bar_, 0, 100);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0x274839), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, LV_PART_MAIN);
    Visible(bar_, !round);

    for (auto& piece : confetti_) piece = Box(root_, 0xB9EF8A, LV_RADIUS_CIRCLE);
    ear_left_ = Box(root_, 0xEDFFF5, S(4));
    ear_right_ = Box(root_, 0xEDFFF5, S(4));
    antenna_ = Box(root_, 0xEDFFF5, S(5));
    head_ = Box(root_, 0x103E29, S(20));
    lv_obj_set_style_bg_opa(head_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(head_, lv_color_hex(0xEDFFF5), 0);
    lv_obj_set_style_border_width(head_, std::max(3, S(7)), 0);
    eye_left_ = Line(root_, std::max(3, S(8)));
    eye_right_ = Line(root_, std::max(3, S(8)));
    cross_left_ = Line(root_, std::max(3, S(6)));
    cross_right_ = Line(root_, std::max(3, S(6)));
    mouth_ = Line(root_, std::max(3, S(6)));
    for (auto& dot : dots_) dot = Box(root_, 0xBAA0F8, LV_RADIUS_CIRCLE);
    for (auto& voice : voice_) voice = Box(root_, 0x8DE8CF, LV_RADIUS_CIRCLE);
    tear_ = Box(root_, 0x8EBFE8, LV_RADIUS_CIRCLE);
    sleep_label_ = Label(root_, 0xA5B7DF);
    lv_label_set_text(sleep_label_, "z");

    brand_ = Label(root_, 0xB5D1BF);
    lv_label_set_text(brand_, "B A M B U   B O T");
    Rect(brand_, 46, round ? 34 : 14, 228, 18);
    state_label_ = Label(root_, 0x86E6A8);
    Rect(state_label_, 42, round ? 57 : 39, 236, 20);
    progress_label_ = Label(root_, 0xE5F4E8);
    Rect(progress_label_, 38, 275, 244, 20);
    detail_label_ = Label(root_, 0x90AE9D);
    Rect(detail_label_, 35, 295, 250, 20);
    lv_label_set_long_mode(detail_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    source_label_ = Label(root_, 0x779E88);
    Rect(source_label_, 118, round ? 253 : 242, 84, 18);

    from_ = to_ = Target(emotion_);
    transition_start_ = lv_tick_get();
    Labels();
    RenderAt(transition_start_);
    timer_ = lv_timer_create([](lv_timer_t* timer) {
        auto self = static_cast<FaceView*>(lv_timer_get_user_data(timer));
        self->RenderAt(lv_tick_get());
    }, 33, this);
}

FaceView::~FaceView() {
    if (timer_) lv_timer_delete(timer_);
    if (root_) lv_obj_delete(root_);
}

FaceView::Pose FaceView::Target(Emotion e) {
    Pose p{{-6, -9, 3, 0, -6, 9}, {0, -11, 0, 0, 0, 11}, 0, 44, 0};
    switch (e) {
        case Emotion::Listening:
            p.left = {0,-14,0,0,0,14}; p.right = p.left; p.mouth_width = 24; break;
        case Emotion::Thinking:
            p.left = {-8,-5,0,-8,8,-5}; p.right = {-8,1,0,1,8,1}; p.mouth_width = 24; break;
        case Emotion::Speaking: p.mouth_width = 46; break;
        case Emotion::Printing: p.bob = -1; break;
        case Emotion::Happy:
        case Emotion::Completed:
            p.left = {-11,5,0,-6,11,5}; p.right = p.left; p.mouth_curve = 10; p.mouth_width = 46; break;
        case Emotion::Curious:
            p.left = {0,-8,0,0,0,8}; p.right = {1,-16,1,0,1,12}; p.mouth_width = 14; p.bob=-3; break;
        case Emotion::Confused:
            p.left = {-9,-2,0,2,9,6}; p.right = {0,-11,0,-2,0,7}; p.mouth_width = 26; p.mouth_curve=-3; break;
        case Emotion::Sleepy:
            p.left = {-11,0,0,4,11,0}; p.right = p.left; p.mouth_width = 24; p.bob=4; break;
        case Emotion::Sad:
            p.left = {-10,-5,0,0,10,2}; p.right = {-10,2,0,0,10,-5}; p.mouth_curve=-7; p.mouth_width=32; p.bob=3; break;
        case Emotion::Warning:
            p.left = {0,-12,0,0,0,10}; p.right = p.left; p.mouth_width=30; break;
        case Emotion::Error:
            p.left = {-9,-9,0,0,9,9}; p.right = p.left; p.mouth_width=32; break;
        case Emotion::Offline:
            p.left = {-9,0,0,0,9,0}; p.right=p.left; p.mouth_width=26; break;
        default: break;
    }
    return p;
}

void FaceView::SetEmotion(Emotion e) {
    if (e < Emotion::Idle || e >= Emotion::Count) e = Emotion::Idle;
    if (emotion_ == e) return;
    // Start a new transition from the current interpolated shape, even mid-transition.
    float t = std::min(1.0f, (lv_tick_get() - transition_start_) / 280.0f);
    t = t*t*(3-2*t);
    for (size_t i=0; i<6; ++i) { from_.left[i]=Mix(from_.left[i],to_.left[i],t); from_.right[i]=Mix(from_.right[i],to_.right[i],t); }
    from_.mouth_curve=Mix(from_.mouth_curve,to_.mouth_curve,t);
    from_.mouth_width=Mix(from_.mouth_width,to_.mouth_width,t);
    from_.bob=Mix(from_.bob,to_.bob,t);
    emotion_=e;
    to_=Target(e);
    transition_start_=lv_tick_get();
    Labels();
    RenderAt(transition_start_);
}
void FaceView::SetTelemetry(const Telemetry& t) {
    telemetry_=t;
    telemetry_.progress=std::clamp(t.progress,-1,100);
    Labels();
}
void FaceView::SetMessage(const std::string& message) { message_=message; Labels(); }
void FaceView::SetAnimated(bool animated) {
    animated_=animated;
    if (animated_) lv_timer_resume(timer_); else lv_timer_pause(timer_);
    RenderAt(lv_tick_get());
}
void FaceView::SetMinimal(bool minimal) {
    for (auto obj : {brand_,state_label_,progress_label_,detail_label_,source_label_}) Visible(obj,!minimal);
    Visible(bar_,!minimal && !round_);
    Visible(ring_,!minimal && round_);
}

void FaceView::Labels() {
    const auto& info=Describe(emotion_);
    lv_label_set_text(state_label_,info.name);
    lv_obj_set_style_text_color(state_label_,lv_color_hex(info.accent),0);
    lv_label_set_text(source_label_, telemetry_.demo ? "DEMO" : "A1 / LAN");
    const bool live=telemetry_.connected && !telemetry_.stale && emotion_!=Emotion::Offline;
    char first[96],second[96];
    if (!live) {
        snprintf(first,sizeof(first),"%s",telemetry_.connected && emotion_!=Emotion::Offline ? "Waiting for fresh data" : "Printer disconnected");
        snprintf(second,sizeof(second),"Live status unavailable");
    } else {
        std::string progress=telemetry_.progress<0 ? "--" : std::to_string(telemetry_.progress);
        std::string remaining=telemetry_.remaining_min<0 ? "--" : std::to_string(telemetry_.remaining_min);
        snprintf(first,sizeof(first),"%s%%   /   %s min left",progress.c_str(),remaining.c_str());
        std::string nozzle=telemetry_.nozzle_c<0 ? "--" : std::to_string(telemetry_.nozzle_c);
        std::string bed=telemetry_.bed_c<0 ? "--" : std::to_string(telemetry_.bed_c);
        snprintf(second,sizeof(second),"Nozzle %s C   /   Bed %s C",nozzle.c_str(),bed.c_str());
    }
    lv_label_set_text(progress_label_,first);
    lv_label_set_text(detail_label_,message_.empty() ? second : message_.c_str());
    int progress=live && telemetry_.progress>=0 ? telemetry_.progress : 0;
    lv_arc_set_value(ring_,progress);
    lv_bar_set_value(bar_,progress,LV_ANIM_OFF);
    lv_obj_set_style_arc_color(ring_,lv_color_hex(live?info.accent:0x526B60),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_,lv_color_hex(live?info.accent:0x526B60),LV_PART_INDICATOR);
}

void FaceView::Eye(lv_obj_t* line, std::array<lv_point_precise_t,3>& points,
                   const std::array<float,6>& pose,float x,float y,float blink) {
    for (int i=0;i<3;++i) {
        points[i].x=offset_x_+S(x+pose[i*2]);
        points[i].y=offset_y_+S(y+pose[i*2+1]*blink);
    }
    lv_line_set_points(line,points.data(),points.size());
}

void FaceView::RenderAt(uint32_t tick) {
    const float time=animated_ ? tick/1000.0f : 0;
    float t=animated_ ? std::min(1.0f,(tick-transition_start_)/280.0f) : 1;
    t=t*t*(3-2*t);
    Pose p;
    for (int i=0;i<6;++i) { p.left[i]=Mix(from_.left[i],to_.left[i],t); p.right[i]=Mix(from_.right[i],to_.right[i],t); }
    p.mouth_curve=Mix(from_.mouth_curve,to_.mouth_curve,t);
    p.mouth_width=Mix(from_.mouth_width,to_.mouth_width,t);
    p.bob=Mix(from_.bob,to_.bob,t);
    const bool offline=emotion_==Emotion::Offline;
    const float bob=offline ? 0 : std::sin(time*(emotion_==Emotion::Sleepy?1.3f:2.0f))*1.6f+p.bob;
    const float breath=offline?0:(std::sin(time*1.6f)+1)*1.8f;
    const auto accent=Describe(emotion_).accent;
    lv_obj_set_style_bg_color(halo_,lv_color_hex(Tint(accent,offline?0.10f:0.16f)),0);
    Rect(halo_,40-breath,61-breath,240+breath*2,240+breath*2);
    lv_obj_set_style_bg_opa(halo_,offline?LV_OPA_40:LV_OPA_70,0);
    lv_obj_set_style_bg_color(halo_inner_,lv_color_hex(Tint(accent,offline?0.10f:0.23f)),0);
    const uint32_t ink=offline?0x77958B:0xEDFFF5;
    lv_obj_set_style_border_color(head_,lv_color_hex(ink),0);
    for (auto obj : {ear_left_,ear_right_,antenna_}) lv_obj_set_style_bg_color(obj,lv_color_hex(ink),0);
    for (auto obj : {eye_left_,eye_right_,mouth_}) lv_obj_set_style_line_color(obj,lv_color_hex(ink),0);
    Rect(head_,65,101+bob,190,116);
    Rect(ear_left_,54,135+bob,13,42);
    Rect(ear_right_,253,135+bob,13,42);
    Rect(antenna_,88,80+bob,13,28);

    float blink=1;
    const auto phase=tick%4700;
    if (animated_ && phase>4350 && phase<4530 && !offline && emotion_!=Emotion::Error && emotion_!=Emotion::Sleepy) {
        blink=std::max(0.08f,std::abs(static_cast<float>(phase)-4440)/90.0f);
    }
    float glance=(emotion_==Emotion::Thinking || emotion_==Emotion::Curious) ? std::sin(time*1.5f)*3 : 0;
    Eye(eye_left_,left_points_,p.left,126+glance,158+bob,blink);
    Eye(eye_right_,right_points_,p.right,194+glance,158+bob,blink);
    const bool error=emotion_==Emotion::Error;
    Visible(cross_left_,error); Visible(cross_right_,error);
    for (int side=0;side<2;++side) {
        auto& points=side?cross_right_points_:cross_left_points_;
        const float x=side?194:126;
        points[0]={static_cast<lv_value_precise_t>(offset_x_+S(x-9)),static_cast<lv_value_precise_t>(offset_y_+S(167+bob))};
        points[1]={static_cast<lv_value_precise_t>(offset_x_+S(x+9)),static_cast<lv_value_precise_t>(offset_y_+S(149+bob))};
        lv_line_set_points(side?cross_right_:cross_left_,points.data(),2);
    }
    for (int i=0;i<9;++i) {
        float u=i/8.0f;
        mouth_points_[i].x=offset_x_+S(160-p.mouth_width/2+p.mouth_width*u);
        mouth_points_[i].y=offset_y_+S(233+bob+p.mouth_curve*std::sin(u*3.14159265f));
    }
    lv_line_set_points(mouth_,mouth_points_.data(),mouth_points_.size());
    const bool speaking=emotion_==Emotion::Speaking;
    Visible(mouth_,!speaking);
    for (int i=0;i<5;++i) {
        Visible(voice_[i],speaking);
        float height=8+(std::sin(time*8+i*1.2f)+1)*7;
        Rect(voice_[i],135+i*11,229+bob-height/2,6,height);
    }
    for (int i=0;i<3;++i) {
        const bool thinking=emotion_==Emotion::Thinking;
        Visible(dots_[i],thinking);
        Rect(dots_[i],145+i*12,90+std::sin(time*4+i)*2,5,5);
        lv_obj_set_style_opa(dots_[i],static_cast<lv_opa_t>(100+(std::sin(time*4+i)+1)*75),0);
    }
    for (int i=0;i<6;++i) {
        Visible(confetti_[i],emotion_==Emotion::Completed);
        float x=i<3 ? 42+i*8 : 262+(i-3)*8;
        Rect(confetti_[i],x,89+(i%3)*22+std::sin(time*2+i)*4,4,i%2?8:4);
    }
    Visible(tear_,emotion_==Emotion::Sad);
    Rect(tear_,206,178+bob+(std::sin(time*1.3f)+1)*5,5,10);
    Visible(sleep_label_,emotion_==Emotion::Sleepy);
    Rect(sleep_label_,235,72+std::sin(time)*3,20,20);
}
}  // namespace bambu::ui
