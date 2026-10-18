# Nguồn mã và phần cứng

## XiaoZhi

- Repository: https://github.com/78/xiaozhi-esp32
- Commit tải trực tiếp từ GitHub: `49ac8a6da399f27a9546d4f73640b7f86c24bac6`.
- Phiên bản upstream: 2.2.6. Phiên bản Bambu Bot: 0.1.0.
- Giữ nguyên giấy phép MIT của upstream trong `LICENSE`.
- Giữ các board, tài liệu và công cụ upstream để tiện đối chiếu/nâng cấp; chỉ board `bambu-deskbot` được chọn cho bản test này.
- Môi trường build thử: ESP-IDF 5.5.4. Không áp dụng yêu cầu IDF 6 của nhánh XiaoZhi mới nhất cho snapshot này.

Các thay đổi Bambu Bot: tên project; profile build mặc định; board test riêng; mô-đun `main/bambu`; gọi service sau khi khởi tạo ứng dụng; bỏ tự động thay firmware theo discovery; lọc metadata `._*` và bổ sung dependency sinh header ngôn ngữ; script cấu hình, host test và CI.

## Desk-bot: chỉ định nghĩa phần cứng

- Project nguồn tại máy: `/Volumes/BOX_120GB/project/DeskBot`.
- Repo nguồn: https://github.com/Hdchipeo/DeskBot-Esp32
- File duy nhất nhập: `main/boards/bread-compact-wifi-lcd/config.h` của working tree.
- File đích: `main/boards/bambu-deskbot/config.h`, giữ nguyên nội dung.
- SHA-256: `25d95e67f092e25ca069fca2a9e665e904188059e97fb0716cff29be6ac9c35d`.
- Chỉ đọc `sdkconfig` để xác định lựa chọn đang dùng: `BREAD_COMPACT_WIFI_LCD`, `ST7789_240X280`, ESP32-S3, flash 16 MB và PSRAM octal. Không sao chép `sdkconfig`, `.env`, NVS hoặc thông tin dịch vụ.
- Board implementation được dựng từ board bread-compact của **XiaoZhi gốc**, bỏ khởi tạo đèn mẫu GPIO18. Không lấy board implementation từ Desk-bot.

| Thiết bị | Định nghĩa trong config.h |
| --- | --- |
| Microphone I2S simplex | WS 47, SCK 45, DIN 38; 16 kHz |
| Loa I2S simplex | DOUT 21, BCLK 20, LRCK 19; 24 kHz |
| LCD SPI | MOSI 40, CLK 39, DC 42, RST 41, CS 2 |
| Đèn nền | GPIO1 |
| LED | GPIO48, hành vi trạng thái theo XiaoZhi |
| Nút BOOT | GPIO0 |
| ST7789 240×280 | Kích thước logic 280×240, swap XY, mirror X, offset X=20 theo file gốc |
| Cảm ứng và nút âm lượng | GPIO_NUM_NC, không có trong cấu hình test |
| Servo | Không có định nghĩa; không khởi tạo |

GPIO19/20 cũng là chân USB native của ESP32-S3. Cần giữ phương thức nạp/đấu dây phù hợp với Desk-bot thực tế. Bản này không dùng sơ đồ chân Waveshare AMOLED.

## PrintSphere

- Repository tham khảo: https://github.com/cptkirki/PrintSphere
- Commit đọc để đối chiếu: `ed071d11dd09c2c93abdba62d8458e4b592f79d0`.
- Đối chiếu file `main/src/printer_client.cpp`: MQTT TLS 8883, user `bblp`, topic report/request, yêu cầu `pushall`, các trường telemetry.
- Không nhập mã triển khai, asset hoặc chứng chỉ từ PrintSphere vào bản này. Bộ chuyển đổi được viết riêng, chỉ hỗ trợ nhóm trường cơ bản của A1, chưa tương đương mức hỗ trợ của PrintSphere.
- Nếu nhập mã PrintSphere về sau, phải kèm giấy phép và mô tả sửa đổi theo điều khoản của repo. Giấy phép đang được repo công bố: FNCL v1.1, giới hạn phi thương mại.

Ngày triển khai bản test: 18/10/2026. Thông tin phần cứng và kết nối phải được đối chiếu thêm trên thiết bị thực tế.
