# VizBeats (JUCE)

Visual-only metronome plugin UI that syncs its animation to the DAW's tempo (BPM) and transport.

## Features
- VST3 (Windows/macOS) and AU (macOS) builds via CMake + JUCE
- No audio processing (passes audio through unchanged)
- Visual pulse animation synced to host BPM + playhead
- Internal preview mode when host transport is stopped

## Notes
- Pro Tools does not load VST3/AU directly (it requires AAX). You can still use the plugin in Pro Tools via a VST3 wrapper host if needed.

## Build

### Prerequisites
- CMake 3.22+
- A C++17 toolchain
  - Windows: Visual Studio 2022
  - macOS: Xcode + command line tools

### Configure
From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

If you already have JUCE locally:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/path/to/JUCE
```

### Build
```bash
cmake --build build --config Release
```

### Output locations
JUCE places binaries under `build/VizBeats_artefacts/Release/` (subfolders per format).

### Standalone preview app
This repo also builds a lightweight preview app (not the JUCE plugin “Standalone” wrapper) that just shows the UI:

- Linux: `build/VizBeatsStandalone_artefacts/Release/VizBeatsStandalone`
- Windows: `build\\VizBeatsStandalone_artefacts\\Release\\VizBeatsStandalone.exe`
- macOS: `build/VizBeatsStandalone_artefacts/Release/VizBeatsStandalone.app`
