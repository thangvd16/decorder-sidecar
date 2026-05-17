# Pack Audit ZBar Decoder Sidecar Template

Copy toàn bộ nội dung folder này sang repo build decoder riêng.

Repo đích sau khi copy nên có cấu trúc:

```text
.github/workflows/build-zbar-decoder-sidecar.yml
CMakeLists.txt
vcpkg.json
src/main.cpp
fixtures/sample.pgm
```

Template này build **custom decoder sidecar dùng ZBar core**, không build full ZBar CLI. Runtime chính của Pack Audit vẫn là:

```text
Rust camera engine
  -> grayscale/raw frame
  -> pack-audit-decoder sidecar
  -> newline-delimited JSON result
```

Không dùng `zbarcam` để mở camera và không dùng `zbarimg` để decode từng frame.

## Supported formats

ZBar phù hợp với scope đóng hàng phổ biến:

- `Code128` / `GS1-128`
- `QR`
- `EAN-13` / `EAN-8` / `UPC-A` / `UPC-E`
- `Code39` / `Code93`
- `Codabar`
- `ITF`

Các format ngoài scope ZBar core: `DataMatrix`, `PDF417`, `Aztec`, `Micro QR`, `rMQR`, `GS1 DataBar`.

## Run workflow

Vào GitHub repo decoder:

1. Actions.
2. Build ZBar Decoder Sidecar.
3. Run workflow.
4. Nếu `CMakeLists.txt` ở root repo, giữ `cmake_source_dir` là `.`.
5. Nếu copy template vào subfolder, nhập subfolder đó, ví dụ `native/decoder`.

Workflow mặc định tạo Windows artifact và một macOS artifact theo runner đang chạy:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
pack-audit-decoder-aarch64-apple-darwin
pack-audit-decoder-x86_64-apple-darwin
```

macOS artifact phụ thuộc host runner thực tế nên chỉ có một trong hai dòng macOS ở mỗi lần chạy. Nếu cần build đủ cả Apple Silicon và Intel, chạy workflow trên runner tương ứng hoặc thêm matrix runner riêng.

Khi copy sang project khác, đổi `BINARY_BASENAME`, `CMAKE_TARGET`, tên artifact và tên executable trong `CMakeLists.txt` nếu không muốn dùng prefix `pack-audit-decoder`.

Nếu push tag `v*`, workflow publish binary vào GitHub Release.

## Smoke test

Workflow chạy:

```text
pack-audit-decoder-* --decode-image fixtures/sample.pgm
```

Fixture hiện tại là ảnh PGM nhỏ không có barcode, nên output hợp lệ sẽ có `results: []`. Thay fixture bằng QR/barcode thật khi cần verify decode thực tế.

## Runtime stdin protocol

Sidecar có chế độ chạy dài hạn:

```text
pack-audit-decoder --decode-stdin
```

Input mỗi frame:

```text
FRAME <width> <height> <byte_length>\n
<grayscale bytes>
```

Output mỗi frame là một JSON line:

```json
{"results":[{"text":"...","format":"CODE-128","timestamp":1778990000000}],"timestamp":1778990000000}
```

`byte_length` phải bằng `width * height` với grayscale/luma 8-bit.
