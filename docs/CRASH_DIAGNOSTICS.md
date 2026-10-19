# Chẩn đoán crash custom wake word / MN7 — 0.5.4

## Log hiện có nói gì?

Bản ghi chạy **0.5.2**, ELF trên thiết bị bắt đầu bằng `023027288`, nhưng monitor
cảnh báo ELF dùng giải mã bắt đầu bằng `f9c6a82d…`. Do đó các dòng hàm/source đã
symbolicate có thể sai. Đặc biệt, không kết luận lỗi nằm trong `std::vector` chỉ
dựa vào dòng inline của ELF không khớp.

Phần raw panic xác nhận **Core 0 LoadProhibited**, `EXCVADDR=0x00000008`, thường
phù hợp truy cập thành viên qua con trỏ null/gần null. Backtrace được hiển thị
có `rnnt_path_append → model_reset/model_detect → CustomWakeWord::Feed`, gợi ý
MultiNet nhưng cần ELF đúng để xác nhận hàm và dòng. SRAM trước crash khoảng
63 KB, minimum khoảng 50 KB, chưa chứng minh cạn tổng SRAM; vẫn thiếu số liệu
PSRAM, khối liên tục lớn nhất và stack của task nhận mic.

Các báo cáo [ESP-SR #225](https://github.com/espressif/esp-sr/issues/225) và
[#226](https://github.com/espressif/esp-sr/issues/226) mô tả vấn đề beam/memory/crash
trong các cấu hình MultiNet khác. Đây là báo cáo tham khảo, **không chứng minh**
cùng nguyên nhân trên Desk-bot. Changelog 2.1.4 từng ghi sửa một số lỗi leak,
nhưng không đủ căn cứ coi nâng ESP-SR là đã sửa crash này. Bản chẩn đoán giữ
ESP-SR **2.3.1**, MN7 English và custom wake word hiện tại.

## Bản chẩn đoán

```sh
. "$HOME/esp/esp-idf/export.sh"
idf.py -D BAMBU_CRASH_DIAGNOSTICS=ON menuconfig
```

Trong **Component config → Heap memory debugging**, chọn:

- Heap corruption detection: **Light impact** (`CONFIG_HEAP_POISONING_LIGHT=y`).
- Abort when allocation fails: **Enabled** (`CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS=y`).

Hai lựa chọn này đã bật trong `sdkconfig` cục bộ của bản thử trên Mac. Không thay
model MN7, câu “bam bu”, ngưỡng nhận dạng, BMCU hay cấu hình LAN/RAG.
`BAMBU_CRASH_DIAGNOSTICS` mặc định OFF trong source; build thử hiện tại đặt ON.

```sh
idf.py -D BAMBU_CRASH_DIAGNOSTICS=ON build
idf.py flash monitor
```

Dùng **flash monitor trong cùng checkout/build**, không chỉ mở monitor với một
ELF khác firmware. Không sửa/build lại project giữa lúc nạp và lúc thu panic nếu
muốn monitor giải mã tự động bằng ELF hiện tại.

## Thông tin thêm được ghi

- Mỗi 10 giây: free/minimum SRAM, **largest internal 8-bit block**, free/minimum
  và largest PSRAM. Tổng free đủ không đồng nghĩa một yêu cầu cấp phát liên tục
  cụ thể sẽ thành công.
- Kiểm tra integrity heap ở nhịp này, cùng canary light-impact khi heap allocate/free.
  Nếu phát hiện corruption, abort tại thời điểm phát hiện thay vì tiếp tục chạy.
- Hook cấp phát thất bại: kích thước yêu cầu, capability mask và tên hàm; không
  cấp phát thêm trong hook. Bật abort giúp backtrace gần chỗ cấp phát lỗi.
- Mỗi phút đang feed custom wake word: tên model, số chunk, số timeout,
  kích thước chunk và stack high-water của **task đang gọi Feed**.
- Guard interface/model/chunk size và kết quả MultiNet (pointer, số kết quả,
  command ID) trước khi caller sử dụng. Không thay thuật toán detect/clean của
  thư viện và không tự khởi tạo lại model theo chu kỳ.

Ví dụ nhãn:

```text
SystemInfo: Crash diagnostics ON: ...
SystemInfo: heap diag: largest_internal_8bit=... free_psram=... min_psram=... largest_psram=...
CustomWakeWord: MN diagnostics: model=mn7_en chunks=... timeouts=... frame_samples=... feed_stack_hwm=...
HeapDiag: allocation failed: bytes=... caps=... caller=...
```

Heap poisoning/kiểm tra integrity có chi phí RAM/CPU và có thể thay đổi thời điểm
crash; đây là build chẩn đoán, không phải cam kết ổn định chạy dài. Không có cách
bắt `LoadProhibited` bên trong thư viện native bằng một `try/catch` C++ thông thường.

## Lưu ELF khớp SHA tự động

Từ 0.5.4, build/flash phụ thuộc target `bambu_debug_symbols`, chạy
`scripts/archive_debug_symbols.py`. ELF được lưu vào:

```text
build/diagnostics/<elf-sha256>.elf
build/diagnostics/<elf-sha256>.json
```

Giữ bản hiện tại và hai bản trước. Metadata chứa version và SHA để đối chiếu
`ELF file SHA256` trong log; rebuild mới không ghi đè tên của ELF cũ. ELF của build
LAN có thông tin cấu hình riêng, giữ cục bộ trong `build/` được Git bỏ qua, không
đưa lên release công khai. Cơ chế này không thể khôi phục ELF 0.5.2 đã mất trước
khi bật lưu trữ.

Nếu monitor báo checksum mismatch, dùng ELF lưu trữ có SHA bắt đầu đúng với panic:

```sh
# Đổi MATCHING_SHA và PORT thành giá trị thực tế.
python "$IDF_PATH/tools/idf_monitor.py" -p PORT -b 115200 \
  --target esp32s3 --toolchain-prefix xtensa-esp32s3-elf- \
  build/diagnostics/MATCHING_SHA.elf
```

Hoặc giải mã các PC raw trong backtrace sau khi export ESP-IDF:

```sh
xtensa-esp32s3-elf-addr2line -pfiaC -e build/diagnostics/MATCHING_SHA.elf 0x420624e5
```

Địa chỉ ví dụ thuộc log cũ, chỉ có ý nghĩa nếu có đúng ELF cũ; không đưa vào ELF
0.5.4 để suy luận lỗi 0.5.2.

## Thử nghiệm tiếp theo

Giữ MN7 theo lựa chọn của người dùng, nạp cả app/assets cùng bản và chạy lâu hơn
mốc crash cũ (~21 phút), gồm idle liên tục và vài lượt hội thoại. Giữ log từ khởi
động có version/SHA, các dòng heap/MN gần crash và toàn bộ panic/reboot. Không
cần cố gây lỗi máy in; MQTT A1 và RAG vẫn hoạt động theo cấu hình hiện tại.

Build và host tests chỉ xác nhận mã tích hợp/archiving/guard biên dịch được; không
chạy được thư viện MN7 ESP32 trên host. Chưa có kiểm chứng chạy dài hoặc xác nhận
nguyên nhân gốc cho crash này. Muốn quay về build thường, tắt option diagnostics
và đặt lại hai lựa chọn heap trong menuconfig; không đổi model trừ khi chủ động chọn.
