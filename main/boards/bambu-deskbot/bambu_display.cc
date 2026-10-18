#include "bambu_display.h"
#include "scene_policy.h"
#include "application.h"
#include "settings.h"
#include "lvgl_theme.h"
#include <algorithm>
#include <cmath>
#include <cstring>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

BambuDisplay::~BambuDisplay() {
    DisplayLockGuard lock(this);
    if (refresh_timer_) lv_timer_delete(refresh_timer_);
    if (message_panel_) lv_obj_delete(message_panel_);
    scene_.reset();
}

void BambuDisplay::SetupUI() {
    DisplayLockGuard lock(this);
    if (setup_ui_called_) return;
    Display::SetupUI();
    Settings settings("display", false);
    scene_theme_ = settings.GetString("theme", "dark") == "light" ? bambu::ui::Theme::Green : bambu::ui::Theme::Black;
    current_theme_ = LvglThemeManager::GetInstance().GetTheme(scene_theme_ == bambu::ui::Theme::Green ? "light" : "dark");
    auto screen = lv_display_get_screen_active(display_);
    scene_ = std::make_unique<bambu::ui::SceneView>(screen, width_, height_, width_ == height_, 2, 100);
    ESP_LOGI("BambuDisplay", "3D head UI: %dx%d raster, %dx%d native HUD; 100 ms frame target", width_/2, height_/2, width_, height_);
    // Retain the upstream multilingual font for provisioning, activation and chat.
    message_panel_ = lv_obj_create(screen);
    lv_obj_remove_style_all(message_panel_);
    lv_obj_set_size(message_panel_, width_, 56);
    lv_obj_align(message_panel_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_remove_flag(message_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(message_panel_, LV_OPA_COVER, 0);
    message_label_ = lv_label_create(message_panel_);
    lv_obj_set_width(message_label_, width_ - 16);
    lv_label_set_long_mode(message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(message_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message_label_, LV_ALIGN_CENTER, 0, 0);
    ApplyColors();
    Refresh();
    refresh_timer_ = lv_timer_create([](lv_timer_t* timer) {
        static_cast<BambuDisplay*>(lv_timer_get_user_data(timer))->Refresh();
    }, 100, this);
}

void BambuDisplay::ApplyColors() {
    if (!scene_) return;
    scene_->SetTheme(scene_theme_);
    const auto bg = lv_color_hex(scene_theme_ == bambu::ui::Theme::Black ? 0 : bambu::ui::kBambuGreen);
    const auto ink = lv_color_hex(scene_theme_ == bambu::ui::Theme::Black ? bambu::ui::kBambuGreen : 0xFFFFFF);
    lv_obj_set_style_bg_color(message_panel_, bg, 0);
    lv_obj_set_style_text_color(message_label_, ink, 0);
}

void BambuDisplay::Refresh() {
    if (!scene_) return;
    using bambu::ui::Emotion;
    const auto device = Application::GetInstance().GetDeviceState();
    Emotion emotion = requested_;
    if (sleeping_) emotion = Emotion::Sleepy;
    else if (device == kDeviceStateFatalError || printer_.error.value.value_or(0) != 0) emotion = Emotion::Error;
    else if (device == kDeviceStateListening) emotion = Emotion::Listening;
    else if (device == kDeviceStateSpeaking) emotion = requested_ == Emotion::Idle ? Emotion::Speaking : requested_;
    else if (device == kDeviceStateConnecting) emotion = Emotion::Thinking;
    else if (emotion == Emotion::Idle) {
        if (!printer_.connected) emotion = Emotion::Offline;
        else if (!printer_.state.value) emotion = Emotion::Warning;
        else if (*printer_.state.value == "RUNNING" || *printer_.state.value == "PREPARE") emotion = Emotion::Printing;
        else if (*printer_.state.value == "FINISH") emotion = Emotion::Completed;
        else if (*printer_.state.value == "PAUSE") emotion = Emotion::Warning;
        else if (*printer_.state.value == "FAILED") emotion = Emotion::Error;
    }
    const uint32_t now = lv_tick_get();
    if(emotion==Emotion::Idle && active_emotion_!=Emotion::Idle)idle_started_=now;
    active_emotion_=emotion;
    const auto printer_state=printer_.state.value.value_or("");
    const bool printer_visual=emotion==Emotion::Error ? printer_.error.value.value_or(0)!=0||printer_state=="FAILED" :
        emotion==Emotion::Completed ? printer_state=="FINISH" :
        emotion==Emotion::Printing ? printer_state=="RUNNING"||printer_state=="PREPARE" :
        emotion==Emotion::Warning && printer_state=="PAUSE";
    auto selected=bambu::ui::AutomaticScene(emotion,(now-idle_started_)/12000,printer_visual);
    if(selected!=active_scene_){active_scene_=selected;scene_->SetScene(selected);if(sleeping_)scene_->RenderAt(2500);}
    scene_->SetEmotion(emotion);
    if (!notification_.empty() && now - notification_started_ >= notification_duration_) notification_.clear();
    if (!system_chat_ && !chat_.empty() && now - chat_started_ >= 10000) chat_.clear();
    const bool conversation = device == kDeviceStateIdle || device == kDeviceStateListening || device == kDeviceStateSpeaking;
    const std::string message = !notification_.empty() ? notification_ : !chat_.empty() ? chat_ : conversation ? "" : status_;
    if (message != shown_message_) {
        shown_message_ = message;
        lv_label_set_text(message_label_, message.c_str());
        // The multilingual panel overlays the telemetry row while a message is present.
    }
    if (message.empty()) lv_obj_add_flag(message_panel_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(message_panel_, LV_OBJ_FLAG_HIDDEN);
}

void BambuDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this); status_ = status ? status : ""; Refresh();
}
void BambuDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this); requested_ = bambu::ui::FromXiaozhi(emotion ? emotion : "neutral"); Refresh();
}
void BambuDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    chat_ = content ? content : "";
    system_chat_ = role && std::strcmp(role, "system") == 0;
    chat_started_ = lv_tick_get(); Refresh();
}
void BambuDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this); chat_.clear(); requested_ = bambu::ui::Emotion::Idle; Refresh();
}
void BambuDisplay::ShowNotification(const std::string& text, int duration_ms) { ShowNotification(text.c_str(), duration_ms); }
void BambuDisplay::ShowNotification(const char* text, int duration_ms) {
    DisplayLockGuard lock(this);
    notification_ = text ? text : ""; notification_started_ = lv_tick_get();
    notification_duration_ = std::clamp(duration_ms, 1, 600000); Refresh();
}
void BambuDisplay::SetTheme(::Theme* theme) {
    if (!theme) return;
    DisplayLockGuard lock(this);
    scene_theme_ = theme->name() == "light" ? bambu::ui::Theme::Green : bambu::ui::Theme::Black;
    Display::SetTheme(theme); ApplyColors();
}
void BambuDisplay::ToggleBambuTheme() {
    // Called on the application task, rather than the GPIO callback or LVGL timer.
    ::Theme* theme;
    {
        DisplayLockGuard lock(this);
        theme = LvglThemeManager::GetInstance().GetTheme(scene_theme_ == bambu::ui::Theme::Black ? "light" : "dark");
    }
    SetTheme(theme);
}
void BambuDisplay::SetPowerSaveMode(bool on) {
    DisplayLockGuard lock(this); sleeping_ = on; if (scene_) scene_->SetAnimated(!on); Refresh();
}
void BambuDisplay::UpdatePrinter(const bambu::Snapshot& snapshot) {
    DisplayLockGuard lock(this);
    printer_ = snapshot;
    if (!scene_) return;
    bambu::ui::Telemetry t;
    t.demo = snapshot.demo; t.connected = snapshot.connected; t.stale = !snapshot.state.value;
    auto value = [](const bambu::Field<double>& field) { return field.value ? static_cast<int>(std::lround(*field.value)) : -1; };
    t.progress = value(snapshot.progress); t.remaining_min = value(snapshot.remaining);
    t.nozzle_c = value(snapshot.nozzle); t.bed_c = value(snapshot.bed);
    scene_->SetTelemetry(t); Refresh();
}
