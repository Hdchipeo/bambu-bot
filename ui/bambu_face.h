#pragma once

// Portable LVGL 9 component. No SDL, ESP-IDF, networking or printer credentials.
// All methods and destruction must run under the caller's LVGL lock/thread.
#include <lvgl.h>
#include <array>
#include <cstdint>
#include <string>

namespace bambu::ui {

// Bambu Studio official splash_logo.svg fill; RGB (0, 174, 66).
inline constexpr uint32_t kBambuGreen = 0x00AE42;

enum class Emotion {
    Idle, Listening, Thinking, Speaking, Printing, Happy, Completed,
    Curious, Confused, Sleepy, Sad, Warning, Error, Offline, Count
};

struct EmotionInfo {
    const char* name;
    const char* description;
    uint32_t accent;
};
const EmotionInfo& Describe(Emotion emotion);
Emotion FromXiaozhi(const std::string& name);

struct Telemetry {
    bool demo = true;
    bool connected = true;
    bool stale = false;
    // Negative means unavailable. Never show missing data as 0.
    int progress = -1;
    int remaining_min = -1;
    int nozzle_c = -1;
    int bed_c = -1;
};

class FaceView {
public:
    FaceView(lv_obj_t* parent, int width, int height, bool round);
    ~FaceView();
    FaceView(const FaceView&) = delete;
    FaceView& operator=(const FaceView&) = delete;

    lv_obj_t* root() const { return root_; }
    void SetEmotion(Emotion emotion);
    Emotion emotion() const { return emotion_; }
    void SetTelemetry(const Telemetry& telemetry);
    void SetMessage(const std::string& message);
    void SetAnimated(bool animated);
    void SetMinimal(bool minimal);
    bool animated() const { return animated_; }
    // Deterministic drawing for previews/tests. Timer calls this on real hardware.
    void RenderAt(uint32_t tick_ms);

private:
    struct Pose {
        std::array<float, 6> left{}, right{};
        float mouth_curve = 0;
        float mouth_width = 44;
        float bob = 0;
    };
    static Pose Target(Emotion emotion);
    void Rect(lv_obj_t* obj, float x, float y, float w, float h);
    void Eye(lv_obj_t* line, std::array<lv_point_precise_t, 3>& points,
             const std::array<float, 6>& pose, float x, float y, float blink);
    void Labels();
    int S(float value) const;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* halo_ = nullptr;
    lv_obj_t* halo_inner_ = nullptr;
    lv_obj_t* head_ = nullptr;
    lv_obj_t* ear_left_ = nullptr;
    lv_obj_t* ear_right_ = nullptr;
    lv_obj_t* antenna_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* cross_left_ = nullptr;
    lv_obj_t* cross_right_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* ring_ = nullptr;
    lv_obj_t* bar_ = nullptr;
    lv_obj_t* brand_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* progress_label_ = nullptr;
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* source_label_ = nullptr;
    lv_obj_t* temperature_row_ = nullptr;
    lv_obj_t* nozzle_value_ = nullptr;
    lv_obj_t* bed_value_ = nullptr;
    // Independently drawn temperature symbols; persistent storage for LVGL lines.
    std::array<std::array<lv_point_precise_t, 10>, 8> temperature_points_{};
    std::array<lv_obj_t*, 3> dots_{};
    std::array<lv_obj_t*, 5> voice_{};
    std::array<lv_obj_t*, 6> confetti_{};
    lv_obj_t* tear_ = nullptr;
    lv_obj_t* sleep_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    std::array<lv_point_precise_t, 3> left_points_{}, right_points_{};
    std::array<lv_point_precise_t, 2> cross_left_points_{}, cross_right_points_{};
    std::array<lv_point_precise_t, 9> mouth_points_{};
    Emotion emotion_ = Emotion::Idle;
    Telemetry telemetry_;
    Pose from_, to_;
    uint32_t transition_start_ = 0;
    float scale_ = 1;
    int offset_x_ = 0, offset_y_ = 0;
    bool round_ = false, animated_ = true, minimal_ = false;
    std::string message_;
};
}  // namespace bambu::ui
