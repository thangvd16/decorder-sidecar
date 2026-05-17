# Pack Audit Decoder Sidecar Template

Copy toàn bộ nội dung folder này sang repo build decoder riêng.

Repo đích sau khi copy nên có cấu trúc:

```text
.github/workflows/build-decoder-sidecar.yml
CMakeLists.txt
vcpkg.json
src/main.cpp
fixtures/sample.pgm
```

## Run workflow

Vào GitHub repo decoder:

1. Actions.
2. Build Decoder Sidecar.
3. Run workflow.
4. Nếu `CMakeLists.txt` ở root repo, giữ `cmake_source_dir` là `.`.
5. Nếu copy template vào subfolder, nhập subfolder đó, ví dụ `native/decoder`.

Workflow sẽ tạo artifact:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
```

Nếu push tag `v*`, workflow cũng publish binary vào GitHub Release.

## Smoke test

Workflow chạy:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe --decode-image fixtures/sample.pgm
```

Fixture hiện tại là ảnh PGM nhỏ không có barcode, nên output hợp lệ sẽ có `results: []`. Thay fixture bằng QR/barcode thật khi bước Phase 3 cần verify decode thực tế.
