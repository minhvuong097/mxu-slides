# MxU Slides for Windows

A native Windows companion to the macOS app, written in C++20 with Qt 6.12 Widgets. It uses the same `.mxuslides` document schema and library layout, so a bundle can move between the Mac and Windows apps.

> **Status: experimental.** The editor, presenter view, program output, stage display, timer, and library are implemented. Capture, video/audio playback, NDI, DeckLink, streaming, and the Local API are not. See [Scope](#scope) before relying on it for a service.

## Requirements

- Windows 10 or 11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24 or newer
- Qt 6.12 for MSVC 2022 64-bit, including the **Qt Widgets** and **Qt Test** modules
- PowerShell 7 (or Windows PowerShell 5.1) for packaging

A typical Qt installation is `C:\Qt\6.12.0\msvc2022_64`. The build has no other third-party runtime dependencies: the ZIP reader/writer (miniz) is vendored.

## Build and test

From `apps/windows`:

```powershell
$env:QT_ROOT = "C:\Qt\6.12.0\msvc2022_64"
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Or configure manually:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="$env:QT_ROOT" -DMXU_CORE_ONLY=OFF -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The executable is `build\windows-release\Release\MxU Slides.exe`. To run it without deploying Qt, the Qt `bin` directory must be on `PATH`.

`MXU_CORE_ONLY=ON` builds only the portable document/library/renderer core and its tests; it does not require Qt Widgets.

## Package

```powershell
.\scripts\package-windows.ps1 -QtRoot $env:QT_ROOT
```

The script configures, builds, tests, runs `windeployqt` next to the executable, and writes:

- `dist\MxU-Slides-Windows-x64\` — self-contained application folder
- `dist\MxU-Slides-Windows-x64.zip` — distributable archive

Use `-Configuration Debug` for a debug package, or `-SkipTests` when the test suite already ran. The package is not code-signed; Windows SmartScreen may warn on first launch until the executable is signed with an Authenticode certificate.

## Run

Double-click `MxU Slides.exe`, or pass a bundle path:

```powershell
& ".\MxU Slides.exe" "C:\Users\me\Documents\MxU Slides\presentations\deck.mxuslides"
```

The local library defaults to `%USERPROFILE%\Documents\MxU Slides`, with `presentations`, `themes`, `media`, and `blobs` subfolders. Change it under **File › Settings**. Settings and the last-opened deck are stored by Qt under the current user's application settings (`MxU`, `MxU Slides`).

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| Save | `Ctrl+S` |
| New presentation | `Ctrl+N` |
| New slide | `Ctrl+Shift+N` |
| Import / export presentation | `Ctrl+O` / `Ctrl+Shift+E` |
| Undo / redo | `Ctrl+Z` / `Ctrl+Y` |
| Duplicate / delete object | `Ctrl+D` / `Delete` |
| Edit selected text | `F2` or double-click |
| Fit slide | `Ctrl+0` |
| Start / stop presenting | `F5` / `Esc` |
| Next / previous slide while presenting | `→`, `Page Down`, `Space` / `←`, `Page Up` |
| Blackout | `B` |
| Logo screen | `L` |
| Nudge selected object | Arrow keys (`Shift` for a larger step) |
| Context menu | Right-click the canvas |

The program output and stage display are separate windows. Choose the output display under **Present › Program output**; `Windowed preview` is the safe default for a single-monitor machine. Stage display is toggled under **Present › Stage display** or from the presenter page.

## Scope

Implemented:

- `.mxuslides` ZIP bundles: `presentation.json`, themes, and media, using the shared JSON schema
- Local library browsing, create/duplicate/delete presentation, recent decks, image import
- Slide and object editing: text, rectangles/rounded rectangles/ellipses, images, fills, gradients, fonts, alignment, opacity, z-order, undo/redo, autosave
- Rendering with text wrapping and auto-shrink, image cropping, and theme fonts
- Presenter page with current/next preview, notes, clock, and elapsed/remaining timer
- Program output on a selected screen, with fade and blackout/logo states
- Stage display with current/next slide and speaker notes
- Headless test suites for documents, bundles, library, and renderer

Not implemented yet:

- Video and audio objects, media playback, and capture devices
- NDI, DeckLink, and streaming outputs
- ProPresenter or other presentation imports
- The macOS Local API, MIDI, timecode, and multitrack audio
- Mac-specific document features that do not map to this renderer are preserved in each object's `extra` data when the file is saved, not displayed

Shape support is intentionally limited to rectangle, rounded rectangle, and ellipse; other shape kinds render as a rectangle. Media placeholders are visible for unsupported media objects. Fonts are resolved from the Windows system; if a deck references a font that is not installed, Qt falls back to a default font.

## CI

`.github/workflows/windows.yml` installs Qt 6.12 on `windows-2022`, builds with Visual Studio 2022, runs the tests, packages with `windeployqt`, and uploads the ZIP. The workflow is the authoritative Windows build recipe.
