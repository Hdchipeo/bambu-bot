#pragma once
#include "display/lcd_display.h"
#include "bambu/printer_state.h"
#include "scene3d.h"

// All public methods lock LVGL; the refresh timer already runs on the LVGL task.
class BambuDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    ~BambuDisplay() override;
    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void ShowNotification(const char* text, int duration_ms = 3000) override;
    void ShowNotification(const std::string& text, int duration_ms = 3000) override;
    void SetTheme(::Theme* theme) override;
    void SetPowerSaveMode(bool on) override;
    void UpdatePrinter(const bambu::Snapshot& snapshot);
    void ToggleBambuTheme();
private:
    void Refresh();
    void ApplyColors();
    std::unique_ptr<bambu::ui::SceneView> scene_;
    bambu::ui::Scene active_scene_ = bambu::ui::Scene::Count;
    bambu::ui::Emotion active_emotion_ = bambu::ui::Emotion::Offline;
    uint32_t idle_started_ = 0;
    lv_timer_t* refresh_timer_ = nullptr;
    lv_obj_t* message_panel_ = nullptr;
    lv_obj_t* message_label_ = nullptr;
    bambu::ui::Theme scene_theme_ = bambu::ui::Theme::Black;
    bambu::ui::Emotion requested_ = bambu::ui::Emotion::Idle;
    bambu::Snapshot printer_;
    std::string status_, chat_, notification_, shown_message_;
    uint32_t chat_started_ = 0, notification_started_ = 0, notification_duration_ = 0;
    bool system_chat_ = false, sleeping_ = false;
};
