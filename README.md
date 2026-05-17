# Pack Audit ZBar Decoder Sidecar Template

Copy toàn bộ nội dung folder này sang repo build decoder riêng.

Repo đích sau khi copy nên có cấu trúc:

```text
.github/workflows/build-zbar-decoder-macos.yml
.github/workflows/build-zbar-decoder-windows.yml
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

Code C++ dùng `zbar_processor_t` + `zbar_process_image`, tương thích với `mchehab-zbar` trên vcpkg Windows. Không dùng API cũ `zbar_image_scanner_t`.

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
2. Chọn `Build ZBar Decoder macOS` hoặc `Build ZBar Decoder Windows`.
3. Run workflow.
4. Nếu `CMakeLists.txt` ở root repo, giữ `cmake_source_dir` là `.`.
5. Nếu copy template vào subfolder, nhập subfolder đó, ví dụ `native/decoder`.

Hai workflow tách riêng để macOS và Windows không chặn nhau:

- macOS dùng Homebrew `zbar` + `ninja`, không chạy vcpkg.
- Windows dùng MSVC + vcpkg manifest với triplet `x64-windows-static`.
- Windows workflow kiểm tra `dumpbin /dependents` để tránh ship binary còn phụ thuộc `zbar`/`iconv`/`gettext` DLL ngoài bundle.

Artifact đầu ra:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
pack-audit-decoder-aarch64-apple-darwin
pack-audit-decoder-x86_64-apple-darwin
```

macOS artifact phụ thuộc host runner thực tế nên chỉ có một trong hai dòng macOS ở mỗi lần chạy. Nếu cần build đủ cả Apple Silicon và Intel, chạy workflow trên runner tương ứng hoặc thêm matrix runner riêng.

Khi copy sang project khác, đổi `BINARY_BASENAME`, `CMAKE_TARGET`, tên artifact và tên executable trong `CMakeLists.txt` nếu không muốn dùng prefix `pack-audit-decoder`.

Nếu push tag `v*`, workflow publish binary vào GitHub Release.

## Windows-first app build

macOS Homebrew chỉ phục vụ dev/test sidecar trên máy macOS. Windows app build không dùng Homebrew.

Luồng Windows-first nên là:

1. Chạy `Build ZBar Decoder Windows`.
2. Lấy artifact hoặc release asset:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
```

3. Trước khi chạy `pnpm tauri build` trong app repo, copy file vào:

```text
src-tauri/binaries/pack-audit-decoder-x86_64-pc-windows-msvc.exe
```

4. Copy FFmpeg sidecar Windows vào cùng folder:

```text
src-tauri/binaries/ffmpeg-x86_64-pc-windows-msvc.exe
```

5. Tauri config trỏ sidecar theo base name:

```json
{
  "bundle": {
    "externalBin": ["binaries/pack-audit-decoder", "binaries/ffmpeg"]
  }
}
```

Binary macOS không cần cho Windows release. Chỉ cần macOS binary khi dev trực tiếp trên macOS và muốn Rust spawn sidecar tại local.

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
