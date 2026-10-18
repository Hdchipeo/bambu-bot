#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "hms.h"

namespace bambu {
template <typename T> struct Field {
    std::optional<T> value;
    int64_t updated_ms = -1;
};

struct Snapshot {
    bool connected = false;
    bool demo = true;
    Field<std::string> state;
    Field<std::string> job;
    Field<double> progress;
    Field<double> remaining;
    Field<double> nozzle;
    Field<double> bed;
    Field<uint32_t> error;
    Field<std::vector<HmsFault>> hms;
};

// ESP-independent: host tests exercise the same parser/event logic as firmware.
class PrinterState {
public:
    explicit PrinterState(bool bmcu_mode=false):bmcu_mode_(bmcu_mode) {}
    void SetConnection(bool connected);
    void SetDemo(bool demo);
    bool Apply(const std::string& payload, int64_t now_ms);
    std::string ToJson(int64_t now_ms) const;
    // Presentation/alerts: expired fields and explicitly ignored BMCU HMS are absent.
    Snapshot GetFreshSnapshot(int64_t now_ms) const;
    std::string Summary(int64_t now_ms) const;
    std::vector<std::string> TakeEvents();

private:
    const bool bmcu_mode_;
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    bool session_running_ = false;
    std::vector<std::string> events_;
};
}  // namespace bambu
