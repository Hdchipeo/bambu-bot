# Thử nghiệm Bambu Bot

## Kiểm tra tự động

Chạy `python3 scripts/test_host.py` để kiểm tra parser C++ đang dùng trong firmware bằng ASan/UBSan và kiểm tra cấu hình Python. Bao gồm:

- Payload lỗi, quá dài, số ngoài miền và dữ liệu thiếu.
- Gộp bản tin từng phần, giữ giá trị chưa được cập nhật; tuổi riêng theo trường.
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
3. Banner phía dưới màn hình phải có `DEMO A1`, tiến độ tăng mỗi 5 giây. Banner vẫn hiển thị khi chưa kích hoạt server AI.
4. Sau khoảng 100 giây, phải xuất hiện popup hoàn tất một lần. Chu kỳ kế tiếp bắt đầu ở khoảng giây 120. Âm popup chỉ phát khi robot ở trạng thái idle để tránh chen vào hội thoại.
5. Thiết lập Wi-Fi/kích hoạt server, nhấn BOOT và thử 10 lượt hội thoại tiếng Việt. Ghi nhận lỗi nhận dạng và âm thanh khi máy in đang chạy.
6. Hỏi trạng thái máy in; trong log phía server xác nhận model gọi `self.bambu.get_printer_status`. Câu trả lời phải nêu rõ DEMO.

Không kỳ vọng cảm ứng, camera hay servo hoạt động trong profile này. Khuôn mặt/emoji và hội thoại là phần XiaoZhi, banner máy in là phần Bambu Bot.

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

## Trạng thái kiểm chứng

- Ngày 18/10/2026: host parser test với ASan/UBSan đạt; 6 bài kiểm tra cấu hình Python đạt.
- Build ESP-IDF 5.5.4 đạt: app `0x2ad220` byte, còn `0x142de0` byte (32%) trong phân vùng ứng dụng.
- Định nghĩa phần cứng khớp từng byte với `config.h` nguồn; SHA-256 ghi trong UPSTREAM.md.
- SDK cài trên máy báo `v5.5.4-dirty`; đây là kết quả build trên môi trường hiện có, chưa phải xác nhận build trên SDK sạch hoặc kết quả CI GitHub.
- Chưa xác nhận nạp/chạy trên Desk-bot, chưa thử MQTT với A1 thật.
- Chưa xác nhận ASR/TTS tiếng Việt, chất lượng microphone/loa, độ ổn định chạy dài hoặc mức tiêu thụ bộ nhớ khi có thoại và MQTT đồng thời.
