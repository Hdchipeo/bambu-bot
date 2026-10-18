# Thiết kế bản mẫu Bambu Bot

> Cập nhật 18/10/2026: phần cứng test trước mắt chuyển sang ESP32-S3 + ST7789 240×280 theo `config.h` của Desk-bot. Waveshare 1.75 và hai servo bên dưới là cấu hình sản phẩm mục tiêu. Đã có firmware demo/LAN và MCP; xem README cùng TESTING.md để biết trạng thái triển khai thực tế.

## 1. Mục tiêu và phạm vi

Bản mẫu cá nhân cho một máy Bambu A1 và board Waveshare ESP32-S3-Touch-AMOLED-1.75. Robot có khuôn mặt trên AMOLED, đàm thoại tiếng Việt và chuyển động đầu hai trục.

Ba giá trị chính:

- Biết tình trạng máy: tiến độ, thời gian còn lại, nhiệt độ, trạng thái và lỗi khi máy cung cấp dữ liệu.
- Chủ động báo sự kiện có ý nghĩa mà không lặp lại liên tục.
- Giải thích vấn đề in 3D bằng tài liệu phù hợp với A1, phân biệt thông tin máy báo và suy đoán nguyên nhân.

MVP chỉ đọc dữ liệu máy in. Điều khiển pause/resume/stop, camera, nhiều máy, pin di động và phát hiện lỗi bản in qua hình ảnh là các bước mở rộng.

## 2. Những gì đã xác minh và còn cần thử

| Hạng mục | Cơ sở hiện có | Việc cần kiểm chứng |
| --- | --- | --- |
| Board 1.75 | XiaoZhi có lựa chọn board riêng trong CMake | Build, nạp, màn hình, cảm ứng và audio trên board thực |
| Kết nối máy in | PrintSphere hỗ trợ trạng thái qua LAN/cloud | A1 với phiên bản firmware và cấu hình mạng thực tế |
| Hội thoại | Repo 78 là firmware; có server tự host riêng | Nhận dạng và tổng hợp tiếng Việt trong tiếng ồn máy in |
| Hai trục servo | Dự kiến dùng PWM và GPIO mở rộng | Loại servo, tải cơ khí, dòng kẹt trục và giới hạn hành trình |
| Bambu Wiki | Đã xác định nguồn chính thức | Truy cập, chọn bài, điều kiện sử dụng và chất lượng truy xuất |

Nguồn xác minh nằm trong [README](../README.md). Chưa đo hiệu năng hay thử phần cứng. Tất cả ngưỡng bên dưới là tiêu chí đề xuất.

## 3. Phân chia phần mềm

### Firmware: lấy XiaoZhi làm nền

Giữ phần board, thu/phát âm thanh và giao thức hội thoại. Thêm các mô-đun do Bambu Bot quản lý:

| Mô-đun đề xuất | Trách nhiệm |
| --- | --- |
| `printer_adapter` | Kết nối A1 qua LAN, xác thực, reconnect có backoff, phân tích bản tin |
| `printer_state` | Gộp cập nhật từng phần, lưu thời điểm nhận theo trường, cung cấp snapshot |
| `event_engine` | Phát hiện chuyển trạng thái, chống lặp, xếp độ ưu tiên thông báo |
| `robot_ui` | Khuôn mặt, vòng tiến độ, trang trạng thái và nguồn tài liệu |
| `motion_controller` | Điều khiển hai servo với giới hạn góc và tốc độ tại thiết bị |
| `bambu_tools` | Cung cấp công cụ đọc trạng thái qua MCP của XiaoZhi |

Các tên trên mô tả thiết kế, chưa phải thành phần đã có trong repo.

Chỉ lấy phần cần thiết từ PrintSphere sau khi đọc cấu trúc và phụ thuộc ở commit đã ghim. Không ghép nguyên hai ứng dụng: mỗi ứng dụng có vòng đời, quản lý UI, mạng và tài nguyên riêng. Nếu dùng lại mã, giữ giấy phép, ghi nguồn và mô tả thay đổi theo điều khoản upstream.

Khởi tạo build từ yêu cầu SDK của commit XiaoZhi đã chọn; README upstream hiện nêu tối thiểu ESP-IDF 6.0.1 và đề xuất 6.1. Phải đối chiếu phụ thuộc của mã PrintSphere trước khi chuyển sang cùng môi trường.

### Server: hội thoại và công cụ

Ứng viên đầu tiên là `xinnan-tech/xiaozhi-esp32-server`, chạy trên máy tính trong LAN. ASR chuyển tiếng nói thành chữ, LLM chọn công cụ/soạn câu trả lời, TTS tạo tiếng nói. Nhà cung cấp và mô hình sẽ chọn sau phép thử tiếng Việt thực tế.

Hai công cụ đầu tiên:

- `get_printer_status(printer_id)`: đọc snapshot có dấu thời gian từ thiết bị. Đây là nguồn cho câu hỏi về trạng thái hiện tại.
- `search_bambu_docs(query, printer_model, error_code?)`: trả đoạn tài liệu, tiêu đề, URL nguồn và thông tin cập nhật.

Không dùng kết quả tìm tài liệu để điền tiến độ hay nhiệt độ còn thiếu. Không cho LLM tự phát lệnh góc servo tùy ý: chỉ chọn cử chỉ đã định nghĩa, bộ điều khiển tại thiết bị áp dụng giới hạn.

### Quyết định chạy adapter trên ESP32

Đây là lựa chọn đầu tiên để màn hình và sự kiện máy in vẫn hoạt động khi server AI mất kết nối. Cần đo heap nội bộ, PSRAM, CPU, thời gian cập nhật UI và gián đoạn audio khi đồng thời chạy TLS, thoại và animation.

Nếu tài nguyên hoặc độ ổn định không đạt, chuyển adapter sang dịch vụ bridge trong LAN. Khi đó phải ghi rõ phụ thuộc mới: mất bridge sẽ mất dữ liệu máy in, dù robot vẫn có thể hiển thị snapshot cũ với nhãn quá hạn.

## 4. Dữ liệu máy in

Snapshot đề xuất:

```json
{
  "printer_id": "a1-desk",
  "model": "A1",
  "connection": "online",
  "state": "printing",
  "job_id": null,
  "progress_pct": 42,
  "remaining_min": 37,
  "nozzle_c": 219.8,
  "bed_c": 60.0,
  "errors": [],
  "observed_at": "2026-10-17T05:00:00Z",
  "stale": false
}
```

Ví dụ minh họa hợp đồng nội bộ, không phải bản tin A1 thực tế. Trường không có dữ liệu phải là `null` hoặc có trạng thái chưa biết; không chuyển thành 0. Kết nối và trạng thái bản in là hai khái niệm riêng.

- Kiểm chứng cách upstream nhận và gộp bản tin từng phần trước khi viết bộ chuyển đổi.
- Giữ tuổi dữ liệu theo trường; một bản tin mới không làm mọi trường cũ trở thành mới.
- Ngưỡng quá hạn đề xuất ban đầu: 30 giây, điều chỉnh sau khi đo nhịp cập nhật A1. Robot nói rõ khi đang đọc dữ liệu cũ.
- Không coi mất kết nối là bản in thất bại hoặc hoàn tất.
- Sau reconnect, lấy đủ trạng thái trước khi phát sự kiện; không phát thông báo hoàn tất chỉ vì khởi động thấy máy đang rảnh.
- Cấu hình LAN dự kiến gồm IP/hostname, serial và access code; xác minh đường kết nối đọc dữ liệu trên A1 thực tế trước khi chốt hướng dẫn cấu hình.

## 5. Thông báo chủ động

Thông báo xuất phát từ `event_engine`, không nhờ LLM liên tục nhìn trạng thái để đoán sự kiện.

| Sự kiện | Hành vi mong muốn |
| --- | --- |
| Bắt đầu in | Hiện vòng tiến độ, thông báo nhẹ nếu người dùng bật |
| Hoàn tất | Biểu cảm vui, gật đầu nhẹ, đọc thông báo một lần |
| Lỗi mới do máy báo | Hiện mã lỗi, thông báo và cho phép hỏi cách xử lý |
| Mất kết nối | Hiện biểu tượng mất kết nối, không báo “in hỏng” |
| Kết nối lại | Làm mới màn hình, tránh đọc lại sự kiện cũ |

Khóa chống lặp dựa trên máy, định danh tác vụ nếu có, loại sự kiện và mã lỗi. Nếu thiếu định danh tác vụ, cần cơ chế phiên in cục bộ và xử lý khởi động lại thận trọng. Lưu dấu sự kiện đã phát để reconnect hoặc reboot không gây báo lại hàng loạt.

Khi đang hội thoại, xếp hàng sự kiện thông thường; lỗi máy hiển thị ngay và chỉ phát giọng khi kênh âm thanh sẵn sàng. Có tắt tiếng và giờ yên lặng. Không đọc lại lỗi liên tục ở mỗi bản tin.

**Phép thử tích hợp bắt buộc:** server đang rảnh vẫn có thể nhận sự kiện, tạo TTS và đưa âm thanh về robot. Nếu luồng XiaoZhi đã chọn chưa hỗ trợ, cần thêm cầu nối thông báo có xác thực; âm báo tại thiết bị là phương án dự phòng, chưa được coi là đã hoàn thành yêu cầu đọc thông báo.

## 6. Kiến thức Bambu Wiki

Bắt đầu bằng tập tài liệu nhỏ về A1: mã lỗi, nạp/rút filament, vệ sinh bàn in, bám dính, nghẹt đầu phun và bảo trì. Đây là danh mục cần thu thập, chưa xác minh từng bài cụ thể.

Lưu URL gốc, tiêu đề, model áp dụng, ngôn ngữ, ngày lấy và phiên bản nội dung. Chia theo mục có ý nghĩa, tìm theo từ khóa/mã lỗi trước, bổ sung tìm ngữ nghĩa khi cần. Chỉ nhập nội dung có quyền sử dụng và cơ chế truy cập phù hợp; không mặc định toàn bộ wiki có thể sao chép.

Quy tắc trả lời:

1. Câu hỏi trạng thái phải gọi công cụ dữ liệu máy.
2. Câu hỏi xử lý lỗi ưu tiên tài liệu đúng A1 và đúng mã lỗi.
3. Nói ngắn 2–3 bước đầu; đặt phần chi tiết và URL nguồn trên giao diện khi có thể. Không đọc URL dài thành tiếng.
4. Nếu dữ liệu hoặc nguồn không đủ, nói rõ và hỏi thêm triệu chứng.
5. Phân biệt “máy báo lỗi X” với “nguyên nhân có thể là Y”. Không khẳng định robot nhìn thấy lỗi bề mặt khi chưa có camera.

Phép thử kiến thức cần gồm câu có nguồn phù hợp, câu thiếu nguồn, mã lỗi không tồn tại và tài liệu khác dòng máy.

## 7. Phần cứng và chuyển động

Board 1.75 có PSRAM 8 MB, flash 16 MB, hai microphone, đường audio codec và đầu nối loa theo tài liệu Waveshare. Kiểm tra loa thực tế đi kèm bộ board; bổ sung loa đúng thông số của hãng nếu thiếu.

Đề xuất cơ khí: một servo pan ở đế, một servo tilt ở cổ, cụm màn hình làm đầu. Đặt tâm khối lượng gần trục tilt, đi dây có độ chùng và chặn cơ khí để tránh kéo đứt dây.

- Dự kiến tín hiệu servo qua GPIO16 và GPIO17 trên header của bản 1.75. Đối chiếu schematic đúng revision và cấu hình firmware trước khi nối. Không áp dụng sơ đồ này cho bản 1.75C.
- Servo dùng nguồn riêng đúng điện áp của mẫu servo, nối chung GND với ESP32. Không cấp servo từ chân 3V3 của board.
- Chọn nguồn theo tổng dòng đỉnh/kẹt trục của hai servo và tải board; chưa chốt công suất khi chưa biết mẫu servo. Bố trí tụ gần nhánh servo và tránh cấp ngược giữa các đường nguồn.
- Hiệu chỉnh trung tâm, xung PWM và giới hạn riêng cho từng trục. Khởi động ở tư thế đã hiệu chỉnh với tốc độ thấp.
- Hạn chế servo chạy lúc thu tiếng vì tiếng động cơ có thể lọt vào microphone. Kiểm chứng audio với cả máy in và servo đang hoạt động.
- Chưa có cảm biến định hướng âm thanh trong thiết kế; hai microphone không tự bảo đảm robot quay đúng hướng người nói.

Biểu cảm đề xuất: chờ, lắng nghe, suy nghĩ, đang nói, đang in, hoàn tất, lỗi và mất kết nối. Trạng thái hội thoại điều khiển khuôn mặt; trạng thái máy in điều khiển vòng tiến độ/biểu tượng. Nhờ vậy người dùng vẫn thấy máy đang in khi robot đang trả lời.

## 8. Lộ trình và tiêu chí hoàn thành

| Mốc | Công việc | Tiêu chí nghiệm thu |
| --- | --- | --- |
| M0: nền phần cứng | Ghim upstream, build XiaoZhi đúng board, kiểm tra loa/mic/touch | Khởi động lặp lại ổn định, ghi lại commit và cấu hình build |
| M1: tiếng Việt | Kết nối server, chọn ASR/TTS, thử khi A1 hoạt động | Hoàn thành 10 lượt hỏi đáp; ghi lỗi nhận dạng, độ trễ và gián đoạn |
| M2: A1 LAN | Tích hợp adapter chỉ đọc, snapshot và UI | Đối chiếu cùng lúc với máy; thử mất Wi-Fi, thiếu trường và reconnect |
| M3: thông báo | Phát hiện sự kiện, chống lặp, TTS chủ động | Một lần cho mỗi sự kiện; không báo hoàn tất giả khi reboot/reconnect |
| M4: kiến thức | Nhập bộ tài liệu A1 có nguồn, kết nối công cụ tìm kiếm | Bộ 20 câu thử có URL kiểm chứng, xử lý được câu thiếu nguồn |
| M5: robot hai trục | Lắp cơ khí, cấp nguồn, hiệu chỉnh cử chỉ | Không reset hoặc kẹt cơ khí khi nói và cử động đồng thời |
| M6: chạy dài | Một tác vụ in thực và 8 giờ hoạt động | Ghi heap, reconnect, sự kiện; không crash hoặc thông báo lặp |

Chưa ước lượng lịch cố định trước khi hoàn thành M0–M2; đây là các mốc xác định tính tương thích và giới hạn tài nguyên.

## 9. Cấu hình và dữ liệu riêng tư

Không đưa Wi-Fi password, access code máy in hay khóa dịch vụ AI vào Git hoặc log. Chỉ cung cấp trạng thái đã chuẩn hóa cho mô hình; không đưa thông tin xác thực vào prompt. Phân biệt mất Internet/server AI với mất kết nối LAN tới máy in trên UI.

Bản mẫu ưu tiên mạng LAN. Nếu dùng dịch vụ ASR/LLM/TTS bên ngoài, âm thanh hoặc nội dung hội thoại sẽ đi tới dịch vụ theo cấu hình; cần thể hiện trạng thái microphone và có nút tắt thu âm.

## 10. Quyết định còn mở

- Loại hai servo, điện áp, dòng và kích thước cụm đầu.
- Máy chạy server và dịch vụ ASR/LLM/TTS tiếng Việt.
- Phiên bản firmware A1, khả năng truy cập LAN và nhịp cập nhật thực tế.
- Bộ tài liệu Wiki ban đầu và cách nhập nội dung được phép.

Bước triển khai kế tiếp là M0: kiểm chứng XiaoZhi nguyên bản trên đúng board trước khi sửa phần kết nối máy in.
