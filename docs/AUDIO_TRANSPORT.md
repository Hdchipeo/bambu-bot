# Chẩn đoán âm thanh MQTT/UDP — 0.5.3

## Ý nghĩa cảnh báo sequence

`MQTT: Received audio packet with wrong sequence: 1277, expected: 1274` đến từ
luồng **UDP nhận Opus của server hội thoại**, không phải MQTT telemetry A1.
Receiver chưa thấy 1274–1276 khi gói 1277 tới. Có thể mất gói hoặc đến muộn/đảo
thứ tự; chỉ từ dòng này chưa xác định được lỗi nằm ở server, mạng hay ESP32.
Âm thanh có thể bị hụt/giật. Việc gọi `search_wiki` HTTP thành công là một luồng
riêng; log chưa chứng minh RAG gây mất gói. Đây cũng chưa phải bằng chứng thiếu
SRAM: log mẫu còn khoảng 58 KB, tối thiểu khoảng 50 KB, không có lỗi cấp phát.

## Thay đổi 0.5.3

- Sửa kiểm tra header: header giao thức dài **16 byte**, không phải
  `sizeof(std::string)` (**24 byte** với compiler Xtensa hiện tại).
  Opus ngắn 3 byte tạo datagram 19 byte hợp lệ; mã cũ loại gói này trước khi xử lý
  sequence, có thể tạo gap ở gói tiếp theo. Đây là lỗi đã xác nhận trong mã;
  chưa có capture chứng minh mọi gap trong log người dùng đều do lỗi đó.
- Kiểm tra payload length đúng kích thước datagram, đọc số nguyên không cần căn
  hàng và chép nonce trước khi AES-CTR cập nhật counter.
- Bỏ gói trùng/đến muộn; so sánh sequence hỗ trợ vòng uint32. Gói đầu phiên được
  dùng làm mốc, không suy đoán số gói đã mất trước đó.
- Gộp thống kê thay vì ghi warning từng gap, tối đa mỗi 5 giây có bất thường và
  khi đóng kênh. Không chỉ tắt log để coi như đã hết lỗi.
- Đếm riêng gói đã nhận nhưng bị hàng đợi giải mã từ chối.
- Tăng vừa phải các bộ đệm RX cho burst. Giữ nguyên codec, 60 ms/frame, bộ đệm
  giải mã, nhịp animation và cơ chế TTS/ASR.

| Cấu hình | Trước | 0.5.3 |
| --- | --- | --- |
| `CONFIG_LWIP_UDP_RECVMBOX_SIZE` | 6 | 16 |
| `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM` | 3 | 4 |
| `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` | 6 | 12 |
| `CONFIG_ESP_WIFI_RX_BA_WIN` | 3 | 4 |

Các giá trị mới trong `sdkconfig.defaults.esp32s3`; checkout đã có `sdkconfig`
cũ cần chỉnh trong menuconfig nếu chưa áp dụng. Bản build riêng trên Mac đã áp
dụng các giá trị này, giữ cấu hình MN7, BMCU và LAN/RAG. Bộ đệm mới tốn thêm RAM,
nên phải theo dõi free/minimum SRAM trên thiết bị sau khi nạp.

XiaoZhi đã gọi `PowerSaveLevel::PERFORMANCE` khi mở audio channel, tương ứng
Wi-Fi `WIFI_PS_NONE`. Không quy lỗi này cho power save chỉ từ log khởi động
LOW_POWER. Tham khảo [tài liệu bộ đệm nhận Wi-Fi/LwIP của Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi-driver/wifi-performance-and-power-save.html).

## Đọc thống kê mới

```text
MQTT: Audio RX totals: received=... gap_events=... gap_packets=... late_or_duplicate=... invalid=...
Application: Audio RX decode_queue_drops=... in last 10 seconds
```

- `received`: datagram hợp lệ qua kiểm tra sequence, tính từ mở kênh.
- `gap_events`: số lần sequence tiến lên vượt quá một gói.
- `gap_packets`: tổng số vị trí sequence chưa thấy tại những lần nhảy đó,
  **không phải số gói mất trên mạng đã được xác nhận**. Gói tới muộn vẫn có thể
  nằm trong con số này.
- `late_or_duplicate`: gói tới sau mốc đã phát hoặc gói trùng bị loại.
- `invalid`: header/type/length không hợp lệ, hoặc nonce phiên không đúng 16 byte.
- `decode_queue_drops`: đã tới ứng dụng nhưng hàng đợi Opus đầy; đếm theo cửa sổ
  10 giây, tách khỏi thống kê sequence. Giá trị 0 không in warning.

Nếu gap tăng nhưng decode_queue_drops bằng 0, tiếp tục kiểm tra đường UDP,
buffer Wi-Fi/LwIP và lịch chạy task nhận; không kết luận decoder quá tải.
Nếu decode_queue_drops tăng, cần đo tốc độ giải mã/phát trước khi tăng queue.

## Kiểm chứng

Host ASan/UBSan kiểm tra datagram Opus 19 byte, dữ liệu không căn hàng, gói thiếu,
length sai, sequence liên tục/nhảy/lặp/muộn và rollover uint32. Firmware build và
kiểm tra assets trên ESP32-S3 + MN7. Chưa nạp/đo packet loss thực tế trong lượt
sửa này. Không có jitter buffer, truyền lại, FEC hoặc khôi phục gói bị mất trong
bản sửa; lỗi mạng thực sự vẫn có thể gây hụt tiếng và cần đo sau khi nạp.

```sh
idf.py build
idf.py flash monitor
```

Thử hội thoại có câu trả lời dài, cả lúc gọi Wiki và lúc không gọi, theo dõi
thống kê mới cùng free/minimum SRAM. Đổi sang WebSocket chỉ có thể thực hiện nếu
server/discovery cung cấp endpoint tương ứng; không tự gán URL để đổi giao thức.
