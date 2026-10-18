#include "printer_state.h"
#include "cJSON.h"
#include <cassert>
#include <iostream>

struct Json {
    cJSON* value;
    explicit Json(const std::string& s) : value(cJSON_Parse(s.c_str())) { assert(value); }
    ~Json() { cJSON_Delete(value); }
    const cJSON* get(const char* key) { return cJSON_GetObjectItemCaseSensitive(value, key); }
};

int main() {
    bambu::PrinterState s;
    s.SetDemo(false);
    s.SetConnection(true);
    assert(!s.Apply("not json", 0));
    assert(!s.Apply("{\"print\":{\"mc_percent\":100}} trailing", 0));
    assert(!s.Apply("{\"print\":{\"mc_percent\":101}}", 0));
    assert(!s.Apply("{\"print\":{\"print_error\":-1}}", 0));
    assert(!s.Apply("{\"print\":{\"print_error\":0.5}}", 0));
    assert(!s.Apply("{\"print\":{\"print_error\":4294967296}}", 0));
    assert(!s.Apply(std::string(65537, 'x'), 0));
    Json empty(s.ToJson(0));
    assert(cJSON_IsNull(empty.get("progress_pct")));
    assert(cJSON_IsFalse(empty.get("demo")));
    assert(cJSON_IsTrue(empty.get("stale")));

    assert(s.Apply(R"({"print":{"gcode_state":"FINISH"}})", 0));
    assert(s.TakeEvents().empty());  // Boot while finished is not a finish event.
    assert(s.Apply(R"({"print":{"subtask_id":"one","gcode_state":"RUNNING","mc_percent":42,"bed_temper":60}})", 100));
    assert(s.Apply(R"({"print":{"nozzle_temper":220}})", 200));
    auto fresh = s.GetFreshSnapshot(200);
    assert(fresh.progress.value == 42 && fresh.nozzle.value == 220);
    assert(!fresh.remaining.value);
    Json partial(s.ToJson(200));
    assert(partial.get("progress_pct")->valuedouble == 42);
    assert(cJSON_IsNull(partial.get("remaining_min")));
    assert(s.Apply(R"({"print":{"nozzle_temper":221}})", 30101));
    auto expired = s.GetFreshSnapshot(30101);
    assert(!expired.state.value && !expired.progress.value && !expired.bed.value);
    assert(expired.nozzle.value == 221); // Fresh temperatures cannot revive expired job fields.
    assert(!s.GetFreshSnapshot(99).nozzle.value);
    Json stale(s.ToJson(30101));
    assert(cJSON_IsTrue(stale.get("stale")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(stale.get("progress_pct_meta"), "stale")));
    assert(cJSON_IsFalse(cJSON_GetObjectItem(stale.get("nozzle_c_meta"), "stale")));
    assert(s.Apply(R"({"print":{"gcode_state":"FINISH"}})", 30102));
    assert(s.TakeEvents().empty());  // Old running state cannot generate a finish.

    assert(s.Apply(R"({"print":{"gcode_state":"RUNNING"}})", 31000));
    assert(s.Apply(R"({"print":{"gcode_state":"PAUSE"}})", 32000));
    assert(s.Apply(R"({"print":{"gcode_state":"FINISH"}})", 33000));
    assert(s.TakeEvents().size() == 1);
    assert(s.Apply(R"({"print":{"gcode_state":"FINISH"}})", 34000));
    assert(s.TakeEvents().empty());

    assert(s.Apply(R"({"print":{"gcode_state":"RUNNING"}})", 35000));
    s.SetConnection(false);
    assert(!s.GetFreshSnapshot(35000).nozzle.value);
    s.SetConnection(true);
    assert(s.Apply(R"({"print":{"gcode_state":"FINISH"}})", 36000));
    assert(s.TakeEvents().empty());  // No transitions inferred over disconnect.
    Json reconnected(s.ToJson(36000));
    assert(cJSON_IsNull(reconnected.get("progress_pct")));
    assert(cJSON_IsFalse(reconnected.get("demo")));

    assert(s.Apply(R"({"print":{"print_error":4294967295}})", 37000));
    assert(s.TakeEvents().size() == 1);
    assert(s.Apply(R"({"print":{"print_error":4294967295}})", 38000));
    assert(s.TakeEvents().empty());
    assert(s.Apply(R"({"print":{"print_error":0}})", 39000));
    assert(s.Apply(R"({"print":{"print_error":4294967295}})", 40000));
    assert(s.TakeEvents().size() == 1);  // A cleared then recurring error is new.

    assert(s.Apply(R"({"print":{"subtask_id":"two","gcode_state":"RUNNING","mc_percent":90}})", 41000));
    assert(s.Apply(R"({"print":{"subtask_id":"three","gcode_state":"FINISH"}})", 42000));
    assert(s.TakeEvents().empty());
    Json next_job(s.ToJson(42000));
    assert(cJSON_IsNull(next_job.get("progress_pct")));
    assert(s.Apply(R"({"print":{"hms":[{"attr":50331904,"code":65543}]}})",43000));
    auto h=s.GetFreshSnapshot(43000);assert(h.hms.value && h.hms.value->size()==1);
    assert((*h.hms.value)[0].Key()=="0300010000010007");
    assert(!s.Apply(R"({"print":{"hms":[{"attr":-1,"code":65543}]}})",43001));
    assert(s.GetFreshSnapshot(43001).hms.value->size()==1);
    assert(s.Apply(R"({"print":{"nozzle_temper":220}})",43002));
    assert(s.GetFreshSnapshot(43002).hms.value->size()==1); // Partial telemetry preserves faults.
    assert(!s.GetFreshSnapshot(73001).hms.value);
    assert(s.Apply(R"({"print":{"hms":[]}})",73002));assert(s.GetFreshSnapshot(73002).hms.value->empty());
    Json known_empty(s.ToJson(73002));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(known_empty.get("hms_meta"),"known")));
    assert(s.Apply(R"({"print":{"hms":[{"attr":1,"code":65537},{"attr":1,"code":65537}]}})",73003));
    assert(s.GetFreshSnapshot(73003).hms.value->size()==1);
    s.SetConnection(false);assert(!s.GetFreshSnapshot(73004).hms.value);
    bambu::PrinterState bmcu(true),ams;
    bmcu.SetConnection(true);ams.SetConnection(true);
    const std::string mismatch=R"({"print":{"gcode_state":"RUNNING","print_error":0,"hms":[{"attr":83887104,"code":65604}]}})";
    assert(bmcu.Apply(mismatch,0) && ams.Apply(mismatch,0));
    assert(bmcu.GetFreshSnapshot(0).hms.value->empty());
    assert(ams.GetFreshSnapshot(0).hms.value->size()==1); // Normal AMS retains the warning.
    Json raw_bmcu(bmcu.ToJson(0));
    assert(cJSON_IsTrue(raw_bmcu.get("bmcu_mode")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(raw_bmcu.get("hms"),0),"ignored")));
    assert(bmcu.Apply(R"({"print":{"hms":[{"attr":83887104,"code":65604},{"attr":50331904,"code":65543}],"print_error":42}})",1));
    const auto active=bmcu.GetFreshSnapshot(1);
    assert(active.hms.value->size()==1 && (*active.hms.value)[0].Key()=="0300010000010007");
    assert(active.error.value==42); // Other HMS and print_error are not suppressed.
    assert((!bambu::HmsFault{0x05000400,0x00010045}.IgnoredForBmcu(true)));
    std::cout << "PASS: parser, partial updates, field freshness, reconnect, completion, error dedup and job isolation\n";
}
