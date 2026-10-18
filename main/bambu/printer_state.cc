#include "printer_state.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <type_traits>
#include "cJSON.h"

namespace bambu {
namespace {
constexpr int64_t kStaleMs = 30000;
bool Fresh(int64_t updated, int64_t now) {
    return updated >= 0 && now >= updated && now - updated <= kStaleMs;
}
const cJSON* Get(const cJSON* obj, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}
bool Number(const cJSON* obj, const char* key, double low, double high,
            Field<double>& out, int64_t now) {
    auto n = Get(obj, key);
    if (!cJSON_IsNumber(n) || !std::isfinite(n->valuedouble) ||
        n->valuedouble < low || n->valuedouble > high) return false;
    out = {n->valuedouble, now};
    return true;
}
bool Text(const cJSON* obj, const char* key, Field<std::string>& out, int64_t now) {
    auto n = Get(obj, key);
    if (!cJSON_IsString(n) || !n->valuestring || std::char_traits<char>::length(n->valuestring) > 256) return false;
    out = {std::string(n->valuestring), now};
    return true;
}
bool Unsigned(const cJSON* value,uint32_t& result) {
    if(!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) || value->valuedouble<0 ||
       value->valuedouble>std::numeric_limits<uint32_t>::max() || std::floor(value->valuedouble)!=value->valuedouble)return false;
    result=static_cast<uint32_t>(value->valuedouble);return true;
}
template<typename T> void Add(cJSON* root, const char* name, const Field<T>& f, int64_t now) {
    if (f.value) {
        if constexpr (std::is_same_v<T, std::string>) {
            cJSON_AddStringToObject(root, name, f.value->c_str());
        } else {
            cJSON_AddNumberToObject(root, name, *f.value);
        }
    } else {
        cJSON_AddNullToObject(root, name);
    }
    auto meta = cJSON_AddObjectToObject(root, (std::string(name) + "_meta").c_str());
    cJSON_AddBoolToObject(meta, "stale", !Fresh(f.updated_ms, now));
    if (f.updated_ms >= 0) cJSON_AddNumberToObject(meta, "age_ms", now - f.updated_ms);
    else cJSON_AddNullToObject(meta, "age_ms");
}
}  // namespace

void PrinterState::SetConnection(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.connected != connected) {
        // Do not infer job transitions across a network gap.
        const bool demo = snapshot_.demo;
        snapshot_ = Snapshot{};
        snapshot_.demo = demo;
        session_running_ = false;
        events_.clear();
    }
    snapshot_.connected = connected;
}
void PrinterState::SetDemo(bool demo) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.demo = demo;
}

bool PrinterState::Apply(const std::string& payload, int64_t now) {
    if (payload.empty() || payload.size() > 65536 || now < 0) return false;
    cJSON* root = cJSON_ParseWithLengthOpts(payload.c_str(), payload.size() + 1, nullptr, true);
    if (!root) return false;
    auto p = Get(root, "print");
    if (!cJSON_IsObject(p)) { cJSON_Delete(root); return false; }
    std::lock_guard<std::mutex> lock(mutex_);
    const auto previous = snapshot_.state;
    const auto previous_job = snapshot_.job.value;
    bool changed = Text(p, "subtask_id", snapshot_.job, now);
    // Changing job identity must not inherit fields/events from the previous print.
    if (snapshot_.job.value != previous_job) {
        const auto job = snapshot_.job;
        const bool connected = snapshot_.connected, demo = snapshot_.demo;
        snapshot_ = Snapshot{};
        snapshot_.job = job;
        snapshot_.connected = connected;
        snapshot_.demo = demo;
        session_running_ = false;
        events_.clear();
    }
    bool state_changed = Text(p, "gcode_state", snapshot_.state, now);
    changed |= state_changed;
    changed |= Number(p, "mc_percent", 0, 100, snapshot_.progress, now);
    changed |= Number(p, "mc_remaining_time", 0, 1000000, snapshot_.remaining, now);
    changed |= Number(p, "nozzle_temper", 0, 500, snapshot_.nozzle, now);
    changed |= Number(p, "bed_temper", 0, 200, snapshot_.bed, now);
    const auto error = Get(p, "print_error");
    if (cJSON_IsNumber(error) && std::isfinite(error->valuedouble) &&
        error->valuedouble >= 0 && error->valuedouble <= std::numeric_limits<uint32_t>::max() &&
        std::floor(error->valuedouble) == error->valuedouble) {
        uint32_t code = static_cast<uint32_t>(error->valuedouble);
        if (code != 0 && (!snapshot_.error.value || *snapshot_.error.value != code)) {
            char msg[80];
            snprintf(msg, sizeof(msg), "A1 error: 0x%08lX", static_cast<unsigned long>(code));
            if (events_.size() < 8) events_.emplace_back(msg);
        }
        snapshot_.error = {code, now};
        changed = true;
    }
    const auto hms=Get(p,"hms");
    if(cJSON_IsArray(hms) && cJSON_GetArraySize(hms)<=32){
        std::vector<HmsFault> faults;bool valid=true;
        for(const cJSON* item=hms->child;item;item=item->next){
            HmsFault fault;
            if(!cJSON_IsObject(item) || !Unsigned(Get(item,"attr"),fault.attr) || !Unsigned(Get(item,"code"),fault.code)){
                valid=false;break;
            }
            faults.push_back(fault);
        }
        if(valid){
            std::sort(faults.begin(),faults.end());faults.erase(std::unique(faults.begin(),faults.end()),faults.end());
            snapshot_.hms={std::move(faults),now};changed=true;
        }
    }
    if (state_changed) {
        const auto& state = *snapshot_.state.value;
        // A fresh observed RUNNING is required; a FINISH snapshot alone is insufficient.
        if (state == "FINISH" && session_running_ && previous.value != snapshot_.state.value &&
            Fresh(previous.updated_ms, now) && events_.size() < 8) {
            events_.emplace_back("A1: Print finished");
        }
        if (state == "RUNNING") session_running_ = true;
        else if (state != "PAUSE") session_running_ = false;
    }
    cJSON_Delete(root);
    return changed;
}

std::string PrinterState::ToJson(int64_t now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "A1");
    cJSON_AddBoolToObject(root,"bmcu_mode",bmcu_mode_);
    cJSON_AddBoolToObject(root, "demo", snapshot_.demo);
    cJSON_AddBoolToObject(root, "connected", snapshot_.connected);
    cJSON_AddBoolToObject(root, "stale", !snapshot_.connected || !Fresh(snapshot_.state.updated_ms, now));
    Add(root, "state", snapshot_.state, now);
    Add(root, "job_id", snapshot_.job, now);
    Add(root, "progress_pct", snapshot_.progress, now);
    Add(root, "remaining_min", snapshot_.remaining, now);
    Add(root, "nozzle_c", snapshot_.nozzle, now);
    Add(root, "bed_c", snapshot_.bed, now);
    Add(root, "print_error", snapshot_.error, now);
    auto faults=cJSON_AddArrayToObject(root,"hms");
    if(snapshot_.hms.value)for(const auto& fault:*snapshot_.hms.value){
        auto item=cJSON_CreateObject();cJSON_AddItemToArray(faults,item);
        cJSON_AddStringToObject(item,"code",fault.DisplayCode().c_str());
        cJSON_AddStringToObject(item,"key",fault.Key().c_str());
        cJSON_AddStringToObject(item,"severity",fault.Severity());
        cJSON_AddBoolToObject(item,"ignored",fault.IgnoredForBmcu(bmcu_mode_));
        if(fault.IgnoredForBmcu(bmcu_mode_))cJSON_AddStringToObject(item,"ignore_reason","Expected AMS A firmware mismatch with BMCU; intentionally ignored by user");
        cJSON_AddNumberToObject(item,"attr",fault.attr);cJSON_AddNumberToObject(item,"raw_code",fault.code);
        cJSON_AddNumberToObject(item,"module_id",fault.attr>>24);
        cJSON_AddNumberToObject(item,"module_number",(fault.attr>>16)&255);
        cJSON_AddNumberToObject(item,"part_id",(fault.attr>>8)&255);
        cJSON_AddNumberToObject(item,"message_id",fault.code&65535);
    }
    auto hms_meta=cJSON_AddObjectToObject(root,"hms_meta");
    cJSON_AddBoolToObject(hms_meta,"known",snapshot_.hms.value.has_value());
    cJSON_AddBoolToObject(hms_meta,"stale",!snapshot_.connected || !Fresh(snapshot_.hms.updated_ms,now));
    if(snapshot_.hms.updated_ms>=0)cJSON_AddNumberToObject(hms_meta,"age_ms",now-snapshot_.hms.updated_ms);
    else cJSON_AddNullToObject(hms_meta,"age_ms");
    char* json = cJSON_PrintUnformatted(root);
    std::string result = json ? json : "{}";
    cJSON_free(json);
    cJSON_Delete(root);
    return result;
}

Snapshot PrinterState::GetFreshSnapshot(int64_t now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto copy = snapshot_;
    auto expire = [&](auto& field) {
        if (!copy.connected || !Fresh(field.updated_ms, now)) field.value.reset();
    };
    expire(copy.state); expire(copy.job); expire(copy.progress); expire(copy.remaining);
    expire(copy.nozzle); expire(copy.bed); expire(copy.error); expire(copy.hms);
    if(copy.hms.value){
        auto& faults=*copy.hms.value;
        faults.erase(std::remove_if(faults.begin(),faults.end(),[this](const HmsFault& f){return f.IgnoredForBmcu(bmcu_mode_);}),faults.end());
    }
    return copy;
}

std::string PrinterState::Summary(int64_t now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string prefix = snapshot_.demo ? "DEMO A1: " : "A1: ";
    if (!snapshot_.connected) return prefix + "Disconnected";
    if (!Fresh(snapshot_.state.updated_ms, now)) return prefix + "Waiting / stale";
    std::string result = prefix + snapshot_.state.value.value_or("Unknown");
    if (snapshot_.progress.value && Fresh(snapshot_.progress.updated_ms, now)) {
        result += " " + std::to_string(static_cast<int>(*snapshot_.progress.value)) + "%";
    }
    if (snapshot_.remaining.value && Fresh(snapshot_.remaining.updated_ms, now)) {
        result += " / " + std::to_string(static_cast<int>(*snapshot_.remaining.value)) + " min";
    }
    return result;
}

std::vector<std::string> PrinterState::TakeEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto events = std::move(events_);
    events_.clear();
    return events;
}
}  // namespace bambu
