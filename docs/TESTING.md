# Thử nghiệm Bambu Bot

## Kiểm tra tự động

Chạy `python3 scripts/test_host.py` để kiểm tra parser C++ đang dùng trong firmware bằng ASan/UBSan và kiểm tra cấu hình Python. Bao gồm:

- Payload lỗi, quá dài, số ngoài miền và dữ liệu thiếu.
- Gộp bản tin từng phần, giữ giá trị chưa được cập nhật; tuổi riêng theo trường.
- Snapshot dùng cho LCD loại bỏ từng trường cũ và mọi trường khi disconnected; nhiệt độ mới không phục hồi tiến độ cũ.
- Kết nối mới không mặc định trạng thái in.
- `FINISH` lúc khởi động không tạo thông báo hoàn tất.
- `RUNNING → PAUSE → FINISH` tạo một sự kiện khi dữ liệu còn mới.
- Mất kết nối, đổi tác vụ hoặc trạng thái cũ không sinh hoàn tất giả.
- Mã lỗi 32-bit lớn được giữ đúng, không lặp ở bản tin tiếp theo; lỗi đã hết rồi xuất hiện lại tạo sự kiện mới.
- Config demo không cần credential; từ chối kiểu dữ liệu sai và serial chứa ký tự topic MQTT.

Kiểm tra firmware: `idf.py build`. Việc build thành công chưa chứng minh LCD, audio hoặc kết nối A1 hoạt động trên thiết bị thực tế.

## Thử trên Desk-bot

1. Build và nạp profile mặc định theo README, mở serial monitor.
2. Kiểm tra log có `BambuBot: Starting DEMO printer source; servo outputs disabled`.
3. Đầu robot 3D và ba icon tiến độ/đầu phun/bàn nhiệt xuất hiện; tiến độ demo tăng mỗi 5 giây. Màn 280×240 bỏ nhãn DEMO nhưng đây vẫn là dữ liệu giả lập. Kiểm tra số đo trở lại sau thông báo kích hoạt/Wi-Fi.
4. Giữ BOOT để đổi theme, reboot để kiểm tra NVS; thử MCP `self.screen.set_theme` với `light` / `dark`.
5. Sau khoảng 100 giây, phải xuất hiện popup hoàn tất một lần. Chu kỳ kế tiếp bắt đầu ở khoảng giây 120. Âm popup chỉ phát khi robot ở trạng thái idle để tránh chen vào hội thoại.
6. Thiết lập Wi-Fi/kích hoạt server, nhấn BOOT và thử 10 lượt hội thoại tiếng Việt. Ghi nhận lỗi nhận dạng và âm thanh khi máy in đang chạy.
7. Hỏi trạng thái máy in; trong log phía server xác nhận model gọi `self.bambu.get_printer_status`. Câu trả lời phải nêu rõ DEMO.

Không kỳ vọng cảm ứng, camera hay servo hoạt động trong profile này. Đầu 3D là component dùng chung với Studio; hội thoại/audio vẫn từ XiaoZhi. Kiểm tra Listening khi nghe, Speaking khi nói và Printing/Completed khi idle; so sánh nhiệt độ, mất mạng và trường stale với MCP.

## Prompt trợ lý đề xuất

```text
Bạn là Bambu Bot, trợ lý in 3D nói tiếng Việt ngắn gọn.
Trước khi trả lời trạng thái máy in, gọi self.bambu.get_printer_status.
Nếu demo=true, luôn nói rõ đây là dữ liệu giả lập.
Nếu connected=false hoặc stale=true, không mô tả dữ liệu là trạng thái hiện tại.
Kiểm tra metadata stale/age_ms của từng trường. null có nghĩa chưa biết, không phải 0.
Không tự suy ra bản in thành công từ phần trăm hay trạng thái mất mạng.
Chỉ giải thích lỗi với nguồn tài liệu được cung cấp; nếu chưa có nguồn, hỏi thêm và nói rõ giới hạn.
Không khẳng định đã tra Bambu Wiki vì bản firmware thử này chưa có công cụ tìm Wiki.
```

Prompt này là hướng dẫn cấu hình, không tự thay đổi server hoặc bảo đảm model luôn tuân thủ.

## Thử với A1

1. Bật đường truy cập LAN được firmware A1 hỗ trợ, đặt IP cố định hoặc DHCP reservation và lấy serial/access code.
2. Chuẩn bị CA PEM phù hợp từ nguồn tin cậy, tạo cấu hình cục bộ theo README, tắt demo, build và nạp lại.
3. Kiểm tra công cụ MCP trả `demo=false`, `connected=true`, trạng thái máy và metadata thời gian. Đối chiếu với màn hình A1.
4. Rút kết nối mạng A1: robot phải hiện Disconnected, không báo in hoàn tất. Nối lại: không phát hoàn tất chỉ vì snapshot đầu tiên đã là FINISH.
5. Thử một tác vụ thực, đối chiếu tiến độ/nhiệt độ. Bộ đọc không gửi pause/resume/stop.
6. Dữ liệu quá 30 giây phải được đánh dấu stale; việc nhận nhiệt độ mới không làm tiến độ cũ thành mới.

Chỉ xử lý `print_error` và nhóm trường cơ bản; chưa giải mã đầy đủ HMS, AMS, phase/substage, metadata vật liệu hoặc camera. Chưa có hàng đợi TTS chủ động, chế độ giờ yên lặng hay lưu sự kiện bền vững qua reboot.

## Kiểm tra profile 3D

- Log khởi tạo có `BambuDisplay: 3D head UI: 140x120 raster, 280x240 native HUD; 100 ms frame target`.
- Khi printer IDLE và robot idle, cảnh vui đổi mỗi 12 giây. Khi RUNNING/PREPARE, ưu tiên cảnh máy in; FINISH → Completed; mã lỗi mới/FAILED → Error. PAUSE giữ máy in đứng yên.
- Biểu cảm AI `error`/`completed` khi không có trạng thái printer tương ứng chỉ đổi đầu, không tự phát cảnh lỗi/hoàn tất máy in.
- Kiểm tra head-only, icon máy in theo ảnh mẫu, hai theme và lớp chữ Wi-Fi/kích hoạt/chat.
- Đo FPS, heap nội/PSRAM, độ trễ thoại và MQTT trên thiết bị trước khi nâng độ phân giải hoặc tốc độ animation.

## Trạng thái kiểm chứng

- Ngày 18/10/2026: host parser test với ASan/UBSan đạt; 6 bài kiểm tra cấu hình Python đạt.
- Build ESP-IDF 5.5.4 đạt: firmware 3D v0.3.0, app `0x2b9990` byte, còn `0x136670` byte (31%) trong phân vùng ứng dụng.
- Định nghĩa phần cứng khớp từng byte với `config.h` nguồn; SHA-256 ghi trong UPSTREAM.md.
- SDK cài trên máy báo `v5.5.4-dirty`; đây là kết quả build trên môi trường hiện có, chưa phải xác nhận build trên SDK sạch hoặc kết quả CI GitHub.
- Chưa xác nhận nạp/chạy trên Desk-bot, chưa thử MQTT với A1 thật.
- Chưa xác nhận ASR/TTS tiếng Việt, chất lượng microphone/loa, độ ổn định chạy dài hoặc mức tiêu thụ bộ nhớ khi có thoại và MQTT đồng thời.
