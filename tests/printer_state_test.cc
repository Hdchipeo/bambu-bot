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
    Json partial(s.ToJson(200));
    assert(partial.get("progress_pct")->valuedouble == 42);
    assert(cJSON_IsNull(partial.get("remaining_min")));
    assert(s.Apply(R"({"print":{"nozzle_temper":221}})", 30101));
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
    std::cout << "PASS: parser, partial updates, field freshness, reconnect, completion, error dedup and job isolation\n";
}
