# Client test plan (macOS)

## What changed
- **Host sync fallback:** if the DAW reports “playing” but doesn’t provide PPQ position, the plugin now derives beat phase from host time (seconds/samples) and will still run.
- **Internal Play uses project BPM:** when the DAW provides BPM while stopped, the internal preview now follows that BPM instead of only the manual BPM.

## Quick install checklist (macOS)
1. Install using the provided PKG:
   - Standalone: installs to `/Applications/VizBeatsStandalone.app`
   - VST3: installs to `/Library/Audio/Plug-Ins/VST3/VizBeats.vst3`
   - AU: installs to `/Library/Audio/Plug-Ins/Components/VizBeats.component`
2. Restart the DAW after install.

## Sync verification steps (in the DAW)
1. Insert **VizBeats** on an audio track (or instrument/effect slot depending on DAW).
2. Set the DAW project tempo to a clear value (e.g. 90 BPM), then start playback:
   - The visual should animate in time.
   - If “Sound Volume” is > 0, you should hear a click on beats.
3. Change the project tempo while playing (e.g. 90 → 140):
   - The visual/click should immediately follow the new tempo.
4. Stop playback:
   - The visual should stop (unless you enable **Internal Play**).
5. While stopped, enable **Internal Play**:
   - If the DAW provides BPM while stopped, the internal preview should match project tempo.
   - Otherwise, it should follow “Manual BPM”.

## If the client still sees “doesn’t play / doesn’t match BPM”
Ask for these details (they directly affect what the host reports to the plugin):
- DAW name + version (Logic/Ableton/FL/Studio One/Reaper/etc.)
- Plugin format used (VST3 or AU)
- macOS version + CPU (Intel/Apple Silicon)
- Does the BPM readout in the plugin show the project tempo correctly?
- Does anything move when the DAW transport is running?

