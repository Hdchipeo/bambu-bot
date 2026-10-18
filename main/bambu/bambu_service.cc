#include "bambu_service.h"
#include "configuration.h"
#include "printer_state.h"

#include "application.h"
#include "board.h"
#include "display.h"
#if CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD
#include "boards/bambu-deskbot/bambu_display.h"
#endif
#include "mcp_server.h"
#include "settings.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <atomic>

namespace bambu {
namespace {
constexpr const char* kTag = "BambuBot";
PrinterState state;
esp_mqtt_client_handle_t client = nullptr;
std::string report_topic, request_topic, incoming;
int incoming_total = 0;
std::atomic<bool> mqtt_connected{false};
int64_t Now() { return esp_timer_get_time() / 1000; }

void OnMqtt(void*, esp_event_base_t, int32_t id, void* data) {
    auto event = static_cast<esp_mqtt_event_handle_t>(data);
    switch (id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            state.SetConnection(true);
            state.SetDemo(false);
            incoming.clear();
            incoming_total = 0;
            esp_mqtt_client_subscribe(client, report_topic.c_str(), 0);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            // Only request telemetry. No pause, resume, stop, light or motion commands.
            esp_mqtt_client_enqueue(client, request_topic.c_str(),
                "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}", 0, 0, 0, true);
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            state.SetConnection(false);
            state.SetDemo(false);
            incoming.clear();
            incoming_total = 0;
            break;
        case MQTT_EVENT_DATA: {
            if (event->current_data_offset == 0) {
                incoming.clear();
                incoming_total = 0;
                if (!event->topic || event->topic_len < 0 ||
                    std::string(event->topic, event->topic_len) != report_topic ||
                    event->total_data_len <= 0 || event->total_data_len > 65536) break;
                incoming_total = event->total_data_len;
            }
            if (incoming_total == 0 || event->current_data_offset != static_cast<int>(incoming.size()) ||
                event->data_len <= 0 || !event->data ||
                event->current_data_offset + event->data_len > incoming_total) {
                incoming.clear();
                incoming_total = 0;
                break;
            }
            incoming.append(event->data, event->data_len);
            if (static_cast<int>(incoming.size()) == incoming_total) {
                state.Apply(incoming, Now());
                incoming.clear();
                incoming_total = 0;
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            // Do not log broker payloads, access codes or full MQTT configuration.
            ESP_LOGW(kTag, "Printer MQTT error; waiting for reconnect");
            break;
        default: break;
    }
}

bool HasIp() {
    auto netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip{};
    return netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0;
}

void StartMqtt() {
    if (std::strlen(BAMBU_HOST) == 0 || std::strlen(BAMBU_SERIAL) == 0 ||
        std::strlen(BAMBU_ACCESS_CODE) == 0 || std::strlen(BAMBU_TLS_CA_PEM) == 0) {
        ESP_LOGE(kTag, "Missing printer configuration or CA certificate; LAN client disabled");
        return;
    }
    report_topic = std::string("device/") + BAMBU_SERIAL + "/report";
    request_topic = std::string("device/") + BAMBU_SERIAL + "/request";
    esp_mqtt_client_config_t config{};
    config.broker.address.hostname = BAMBU_HOST;
    config.broker.address.port = 8883;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    config.broker.verification.certificate = BAMBU_TLS_CA_PEM;
    // LAN uses an IP while printer certificates use a different CN. Still verify the provided CA.
    config.broker.verification.skip_cert_common_name_check = true;
    config.credentials.username = "bblp";
    config.credentials.authentication.password = BAMBU_ACCESS_CODE;
    config.session.keepalive = 20;
    config.network.reconnect_timeout_ms = 10000;
    config.buffer.size = 4096;
    config.task.stack_size = 8192;
    client = esp_mqtt_client_init(&config);
    if (!client) { ESP_LOGE(kTag, "Cannot allocate MQTT client"); return; }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, OnMqtt, nullptr));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

void Task(void*) {
    bool attempted = false;
    unsigned step = 0;
    int64_t last_demo = -10000;
    int64_t last_request = 0;
    std::string last_summary;
    std::vector<std::string> pending;
    while (true) {
        const auto now = Now();
        if (BAMBU_DEMO && now - last_demo >= 5000) {
            state.SetConnection(true);
            state.SetDemo(true);
            // 100-second synthetic print, then a pause before the next job.
            unsigned phase = step % 24;
            std::string payload = "{\"print\":{\"subtask_id\":\"demo-" + std::to_string(step / 24) +
                "\",\"gcode_state\":\"" + (phase < 20 ? "RUNNING" : "FINISH") +
                "\",\"mc_percent\":" + std::to_string(phase < 20 ? phase * 5 : 100) +
                ",\"mc_remaining_time\":" + std::to_string(phase < 20 ? 20 - phase : 0) +
                ",\"nozzle_temper\":220,\"bed_temper\":60,\"print_error\":0}}";
            state.Apply(payload, now);
            ++step;
            last_demo = now;
        } else if (!BAMBU_DEMO && !attempted && HasIp()) {
            state.SetDemo(false);
            StartMqtt();
            attempted = true;
        }
        if (!BAMBU_DEMO && client && mqtt_connected && now - last_request >= 20000) {
            // Refresh sparse fields before their per-field freshness deadline.
            esp_mqtt_client_enqueue(client, request_topic.c_str(),
                "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}", 0, 0, 0, true);
            last_request = now;
        }
        auto events = state.TakeEvents();
        for (auto& event : events) {
            if (pending.size() < 8) pending.push_back((BAMBU_DEMO ? "DEMO: " : "") + event);
        }
        // UI work runs on the application task; no LVGL access from MQTT callbacks.
        const auto summary = state.Summary(now);
        const bool update = summary != last_summary;
        // Deliver temperature-only updates and field expiry on every service tick.
        auto batch = std::move(pending);
        pending.clear();
        Application::GetInstance().Schedule([summary, update, snapshot = state.GetFreshSnapshot(now), batch = std::move(batch)]() {
            auto& app = Application::GetInstance();
            auto display = Board::GetInstance().GetDisplay();
#if CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD
            static_cast<BambuDisplay*>(display)->UpdatePrinter(snapshot);
#else
            if (update) display->ShowNotification(summary, 1500);
#endif
            for (const auto& event : batch) {
                display->ShowNotification(event, 8000);
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.PlaySound(Lang::Sounds::OGG_POPUP);
                }
            }
        });
        last_summary = summary;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}  // namespace

void StartService() {
    state.SetDemo(BAMBU_DEMO);
    if (std::strlen(BAMBU_OTA_URL) > 0) {
        Settings settings("wifi", true);
        settings.SetString("ota_url", BAMBU_OTA_URL);
    }
    McpServer::GetInstance().AddTool("self.bambu.get_printer_status",
        "Read Bambu A1 status. Call before answering printer status questions. "
        "demo=true means simulated data, never describe it as a real printer. "
        "Check connected, stale and each field's age/stale metadata; null means unknown. "
        "This tool cannot control the printer or diagnose issues without documentation.",
        PropertyList(), [](const PropertyList&) -> ReturnValue { return state.ToJson(Now()); });
    ESP_LOGI(kTag, "Starting %s printer source; servo outputs disabled", BAMBU_DEMO ? "DEMO" : "LAN");
    if (xTaskCreate(Task, "bambu_printer", 6144, nullptr, 3, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "Cannot create printer task");
    }
}
}  // namespace bambu
