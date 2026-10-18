# Bambu Bot Emotion Studio

Ứng dụng giả lập **LVGL 9.5.0 + SDL2 trên macOS**, dùng để duyệt khuôn mặt và emotion trước khi tích hợp vào firmware. Đây là giao diện LVGL thực sự, không phải mockup web hoặc ảnh GIF.

![Emotion library](ui/emotion-sheet.png)

## Thiết kế

Lấy cảm hứng từ biểu tượng robot xanh/trắng trong ảnh `IMG_6476.PNG`: đầu chữ nhật bo góc, hai tai, anten bên trái, mắt trái hình dấu `>` và mắt phải hình viên thuốc. Nền xanh rừng giữ độ tương phản với nét trắng; màu phụ thay đổi theo cảm xúc. Chuyển động nhỏ để robot có sức sống mà không làm mất khả năng đọc thông tin máy in.

Vẽ bằng `lv_obj`, `lv_line`, `lv_arc`, `lv_bar` và `lv_label`; không cần bitmap, GIF, SVG hay ThorVG. Các mảng điểm vẽ sống cùng component để an toàn với API `lv_line_set_points`. Animation dùng một timer 33 ms và tập đối tượng cố định, không tạo/xóa đối tượng ở mỗi frame.

## Chạy trên macOS

```sh
brew install cmake ninja sdl2
./scripts/run_simulator.sh
```

Script build chế độ Release và mở ứng dụng. Nếu đã có bản đóng gói, chỉ cần mở `dist/Bambu Bot Studio.app`; SDL2 đã được kèm bên trong bản đóng gói nên không cần Homebrew để chạy bản đó.

Ứng dụng được build/kiểm tra trên Mac Apple Silicon **arm64** trong môi trường hiện tại; bản ZIP này có deployment target **macOS 26.0**. Chưa kiểm chứng Intel hoặc các phiên bản macOS khác. Bản đóng gói ký ad-hoc để chạy thử, chưa notarize.

Nếu chưa build firmware và chưa có `managed_components/lvgl__lvgl`, CMake tự tải LVGL tag `v9.5.0`. Có thể chỉ định nguồn sẵn có bằng `-DBAMBU_LVGL_DIR=/absolute/path/to/lvgl`. LVGL source phải đúng 9.5.0.

## Điều khiển

| Thao tác | Kết quả |
| --- | --- |
| Chọn một emotion bên phải | Đổi mắt, miệng, màu và chuyển động |
| Round / Desk-bot | Đổi vùng render thật 466×466 hoặc 280×240 |
| Thanh Print progress | Đổi dữ liệu giả lập từ 0–100% |
| Play sequence / Space | Chạy lần lượt các emotion, mỗi 3,5 giây |
| Motion on/off / M | Bật/tắt chuyển động khuôn mặt |
| R | Đổi loại màn hình |
| Phím trái/phải | Chuyển emotion trước/sau |
| Escape | Đóng ứng dụng |

Tất cả số liệu đều gắn nhãn **DEMO**. Simulator không kết nối A1, không thu âm và không gọi server AI. Waveform của trạng thái Speaking là animation minh họa, chưa đồng bộ biên độ TTS.

![Round preview](ui/studio-round.png)

![Desk-bot preview](ui/studio-deskbot.png)

## Bộ emotion

| Emotion | Biểu cảm/chuyển động |
| --- | --- |
| Idle | Mắt bất đối xứng của biểu tượng gốc; thở và chớp mắt |
| Listening | Mở mắt cao hơn, sắc xanh lam nhẹ |
| Thinking | Liếc nhẹ; ba chấm chuyển động trên đầu |
| Speaking | Năm thanh sóng thay cho miệng |
| Printing | Giữ khuôn mặt quen thuộc và tiến độ in |
| Happy | Mắt cười, miệng cong, nhún nhẹ |
| Completed | Mắt cười và các hạt trang trí ăn mừng; demo chuyển tiến độ lên 100% |
| Curious | Hai mắt cao thấp khác nhau, liếc nhẹ |
| Confused | Một mắt nghiêng, miệng hơi lệch |
| Sleepy | Mắt khép, nhịp thở chậm và chữ z |
| Sad | Mắt trĩu, miệng buồn, một giọt nước mắt |
| Warning | Mắt dựng, sắc hổ phách |
| Error | Mắt chữ X, màu đỏ dịu; không nhấp nháy cảnh báo |
| Offline | Mắt ngang, nét mờ và thông báo mất kết nối; ẩn số liệu sống |

Emotion có chuyển hình dạng mắt/miệng trong 280 ms. Khi chuyển liên tiếp, animation bắt đầu từ hình dạng hiện tại, tránh nhảy về tư thế ban đầu. Màu trạng thái đổi ngay để dễ nhận biết. Trường thiếu dùng `--`; kết nối thiếu hoặc dữ liệu stale không hiển thị các số đo như dữ liệu hiện tại.

## Component dùng lại cho LCD

`ui/bambu_face.h` và `ui/bambu_face.cc` chỉ phụ thuộc LVGL. Constructor nhận parent, kích thước và kiểu màn hình:

```cpp
// Chỉ gọi trong LVGL thread hoặc khi đã giữ khóa LVGL.
bambu::ui::FaceView face(parent, 280, 240, false);
face.SetEmotion(bambu::ui::Emotion::Listening);

bambu::ui::Telemetry status;
status.demo = true;
status.progress = 42;
status.remaining_min = 43;
status.nozzle_c = 220;
status.bed_c = 60;
face.SetTelemetry(status);
```

Component sở hữu root và timer; **hủy FaceView trước khi hủy parent/LVGL display**. Không copy/move component và không giữ nó trên stack của hàm đã trả về. `SetMessage` dành cho thông điệp ngắn, ví dụ mã kích hoạt; `SetMinimal` cho thumbnail biểu cảm; `FromXiaozhi` ánh xạ các tên emotion phổ biến của XiaoZhi và trả Idle khi tên chưa biết.

Chưa gắn renderer mới vào `BambuDeskBot`/`Application`; firmware v0.1.0 vẫn là bản demo trước. Bước tích hợp tiếp theo cần nối trạng thái hội thoại và dữ liệu printer vào component, giữ đường hiển thị thông báo Wi-Fi/kích hoạt/lỗi, rồi đo RAM/FPS trên ESP32. Những số liệu hiệu năng ở Mac không thay thế kiểm chứng trên LCD thật.

## Kiểm tra và xuất ảnh

```sh
cmake -S simulator -B simulator/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBAMBU_SANITIZERS=ON
cmake --build simulator/build --parallel 8
ctest --test-dir simulator/build --output-on-failure

# Xuất ảnh render LVGL thực; không cần mở cửa sổ.
simulator/build/bambu_studio.app/Contents/MacOS/bambu_studio \
  --headless --output dist/ui-preview
```

Headless export tạo ảnh toàn studio, bảng 14 emotion và ảnh LCD đúng kích thước. File BMP có thể đổi sang PNG bằng `sips` trên macOS. Các ảnh trong tài liệu được xuất từ chính renderer này.

Đã kiểm tra ngày 18/10/2026: build AppleClang trên nguồn LVGL có sẵn và bản tải sạch từ GitHub; bài smoke test ASan/UBSan cho toàn bộ nút emotion, chuyển màn hình, kích thước tập đối tượng ổn định, offline/stale/unknown, message, motion và alias; thử tương tác chuột/phím trực tiếp trên cửa sổ SDL2. Chưa đo FPS, RAM hay khả năng chạy trên ESP32.

## Đóng gói ứng dụng

```sh
./scripts/run_simulator.sh --build-only
python3 scripts/package_studio.py
```

Script kèm SDL2 và thông báo giấy phép, đổi đường dẫn thư viện sang bundle, ký ad-hoc, kiểm tra chữ ký và chạy smoke test từ bản bundle. Nó từ chối đóng gói bản chứa sanitizer. Kết quả: `.app` và ZIP theo kiến trúc máy trong `dist/`. Việc ký được thực hiện ở thư mục tạm để tránh metadata AppleDouble của ổ ngoài đi vào bundle.
