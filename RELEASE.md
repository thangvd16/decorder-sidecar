# Release Native Sidecars

## Important distinction

Git tags do not contain build artifacts by themselves.

- `workflow_dispatch` creates **Actions artifacts** attached to that workflow run.
- `git push origin vX.Y.Z` triggers the tag workflows and creates/updates a **GitHub Release** with release assets.
- App CI should download **GitHub Release assets**, not old Actions artifacts.

If a tag was pushed before these workflows existed at repo root, that tag will not publish the expected assets. Use a new tag, for example `v0.2.1`, or delete and recreate the old tag only if nobody depends on it.

## Repository layout

Copy this template's contents to the root of the sidecar repo:

```text
.github/workflows/build-zbar-decoder-windows.yml
.github/workflows/build-zbar-decoder-macos.yml
.github/workflows/build-ffmpeg-windows.yml
.gitignore
CMakeLists.txt
README.md
RELEASE.md
fixtures/sample.pgm
src/main.cpp
triplets/x64-windows-dynamic-staticcrt.cmake
vcpkg.json
```

The workflow files must be under `.github/workflows/` at the repository root. GitHub Actions will not run workflows stored under `docs/`.

## Pre-release check

Run these workflows manually first:

1. `Build ZBar Decoder Windows`
2. `Build FFmpeg Windows Sidecar`

Download and unzip the Actions artifacts. Expected Windows files:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
zbar-0.dll
iconv-2.dll
ffmpeg-x86_64-pc-windows-msvc.exe
FFMPEG_SOURCE.txt
```

The exact ZBar DLL name may be `zbar-0.dll`; do not rename it. The decoder `.exe` depends on that DLL name.

## Publish release

After the manual workflows pass, publish a new tag from the sidecar repo:

```bash
git status --short
git tag v0.2.1
git push origin v0.2.1
```

Wait for both tag-triggered workflows to finish. The GitHub Release for that tag must contain:

```text
pack-audit-decoder-x86_64-pc-windows-msvc.exe
zbar-0.dll
iconv-2.dll
ffmpeg-x86_64-pc-windows-msvc.exe
FFMPEG_SOURCE.txt
```

If release assets are missing:

1. Open the Actions runs triggered by the tag.
2. Confirm both workflows ran from `refs/tags/v...`.
3. Confirm `permissions.contents: write` exists in the workflow.
4. Confirm the workflow files were already committed to the sidecar repo before the tag was created.
5. Use a new tag after fixing the workflow or recreate the tag only if it is safe.

## Cleanup old artifacts

You can delete old manual Actions artifacts from GitHub Actions to save storage. This does not delete GitHub Release assets.

Do not commit generated `.exe`, `.dll`, `.zip`, `build/`, or `dist/` files to the sidecar repo.
