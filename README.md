# VUMister NAM v51 — GitHub Ready

Windows x64 JUCE VST3/Standalone NAM processor with the supplied VUMister NAM vintage faceplate.

## UI fixes in this revision

- Uses the supplied `Assets/reference_full_interface.png` as the fixed visual reference.
- Enlarges the live rotary-control masks to fully cover the baked-in INPUT / MIX / OUTPUT knobs before drawing the interactive controls, reducing doubled rings and overlapping labels.
- Makes the LOAD NAM control transparent so the reference artwork's border and label are not drawn twice.
- Keeps the model selector's dark interior to mask the static sample filename while making its border transparent.
- Restricts the BYPASS click target to the switch body and synchronizes its lamp with the bypass parameter.
- Uses a conventional VU scale with the classic -20, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3 markings and a red overload region above +1. The live needle and scale share the same nonlinear mapping. A steady -18 dBFS RMS signal points at 0 VU. At silence/startup, both needles rest at the -20 mark.

## Compile fix in v38

- Explicitly captures `cropY` in the compact VU meter needle-drawing lambda. MSVC reported that this local constant was not captured, which prevented the lambda call from compiling.

## Main editor layout

- The VU ONLY / SHOW ALL toggle and compact layout have been removed. The plug-in opens directly in its single main interface at 50% reference size (761 x 416 pixels).

## Build on GitHub

1. Upload the contents of this directory to the root of a GitHub repository. `CMakeLists.txt` must be at repository root.
2. Open **Actions** and run **Build VUMister NAM (Windows x64)**, or push to `main` / `master`.
3. Download `VUMisterNAM-Windows-x64-VST3` after the build succeeds.
4. If a build fails, download `VUMisterNAM-Windows-x64-Diagnostics` and inspect `configure.log` / `build.log`.

## Notes

- The workflow builds with Visual Studio 2022 x64 on `windows-2022` and packages the generated VST3 bundle.
- The workflow is not evidence of a successful build until GitHub Actions completes successfully.
- If the selected NAM model is unavailable at runtime, use **LOAD NAM** to select a `.nam` file.
- This project uses JUCE 8.0.6 and the NeuralAudio Git repository declared in `CMakeLists.txt`. The exact NeuralAudio API/target must be confirmed by the Windows CI build.

## Latest UI and gain-control improvements

- `Assets/reference_full_interface.png` is the runtime faceplate aligned to the supplied 1522 × 833 reference. It replaces the static needles with the matching needle-free meter artwork so only the live input/output needles are drawn by the editor. The original supplied composite is preserved as `Assets/reference_full_interface_original.png`.
- First-load defaults are INPUT 0 dB, MIX 50% wet, OUTPUT 0 dB, and BYPASS off. INPUT and OUTPUT gain controls range from -24 dB to +24 dB; MIX ranges from 0% to 100%. The controls use JUCE APVTS slider attachments for host automation and state recall. Existing DAW projects/presets retain their saved settings.
- Double-click INPUT/OUTPUT to reset to 0 dB; double-click MIX to reset to 50% mix. Hold Shift while dragging for finer adjustments. Hover over a control to see its range and reset behavior.
- The rotary controls are custom-painted with a dark machined-metal face, silver bezel, amber pointer, and a 270-degree throw. The reference tick marks and labels remain visible.
- Bypass uses a transparent hit target over the illustrated switch to avoid JUCE button borders or checkbox chrome appearing in Nuendo.
- VU needles are drawn dynamically over needle-free meter artwork. Input and output meters use smoothed RMS levels with a 0 VU reference at -18 dBFS RMS.

A Windows x64 build has not been confirmed until the GitHub Actions workflow completes successfully. Always test the resulting VST3 in the target DAW.


## Numeric editing and needle update

- Shortens the live VU needle length to 180 drawing units.
- INPUT, MIX, and OUTPUT numeric overlays remain hidden during normal use; double-click the corresponding rotary to open numeric entry. Press Enter to apply the value or Escape to cancel.
- The Windows GitHub Actions workflow checks the 180-unit needle setting and the double-click-only numeric editor wiring before configuring/building.


## Vintage Copper Numbered Dials

The INPUT and OUTPUT rotary controls now draw a numbered -24 to +24 dB scale, while MIX draws 0 to 100. Each dial uses fixed tick divisions, brighter major ticks, and an orange pointer; no animated progress arc is drawn. Numeric values are fixed scale markings; live numeric readouts remain hidden unless explicitly opened by double-click.

The ZIP can be integrity-checked locally, but a successful Windows compile and DAW load test must still be confirmed by running the included GitHub Actions workflow.


## Reference UI fidelity

- The supplied `Assets/reference_full_interface.png` is the primary faceplate and is displayed without repainting its VU labels, Thai branding, legends, frame, model selector well, or LOAD NAM artwork.
- Only the live VU needles, interactive rotary controls, and bypass lamp state are drawn over the reference image. The editor keeps the artwork aspect ratio when resized.
- INPUT and OUTPUT start at 0 dB, MIX starts at 50% to match the centered MIX pointer in the reference image, and BYPASS starts off. Existing DAW project/preset values are restored normally.
- The project currently includes `UREI 1176-SF - Fast Control.nam`; it does not include a `Vintage British 50W.nam` model file, so the interface reports the actual loaded model rather than falsely claiming that a different model is loaded.


## VU +3 dB display offset (v12)

- Both INPUT and OUTPUT VU needles receive a display-only +3 dB offset.
- This moves the indicated meter position up by 3 dB; it does not change the audio signal, INPUT/OUTPUT gain parameters, or NAM processing.
- The underlying meter reference remains -18 dBFS RMS = 0 VU before the requested display offset.


## VU CURRENT / MAX readouts

Both INPUT and OUTPUT meters show a live `CURRENT` value and a `MAX` peak-hold value. The displayed values use the same -18 dBFS RMS reference and display-only +3 dB offset as the needles, are clamped to the printed -20 to +3 scale, and do not change audio gain. MAX holds the highest displayed value for the current editor session and resets when the editor is recreated.


## CURRENT / MAX range (v14)

- CURRENT and MAX readouts are limited to -24 dB through +24 dB.
- Both readouts initialize to 0 dB when the editor opens.
- CURRENT follows the live signal. MAX starts at 0 dB and begins peak-hold tracking when signal activity is detected; subsequent negative peaks are handled correctly.
- The existing VU faceplate and +3 dB display calibration are retained; this readout range change does not alter audio gain.


## CURRENT / MAX behavior (v15)

- CURRENT and MAX display range: -20 dB through +24 dB.
- CURRENT and MAX initialize to 0 dB when the editor opens.
- Click the `MAX ↻` readout under either meter to reset that channel's peak hold to 0 dB at any time.
- The reset zones scale with the reference interface; the audio signal and gain are not changed by meter reset.


## CURRENT / MAX display update (v16)

- Removed the reset-arrow symbol from the visible MAX label; click the MAX readout to reset its peak hold to 0 dB.
- Increased the CURRENT/MAX readout font size from 11 to 14 scaled units for better legibility.
- Retains the CURRENT -20 dB to +24 dB range and 0 dB startup defaults.

### UI adjustment (v17)
- Lowered the INPUT / MIX / OUTPUT legends beneath the three rotary knobs by 8 artwork pixels. The upper VU legends and control positions are unchanged.


UI adjustment v20: lowered the MIX 50% scale label and DRY/WET endpoint labels slightly for improved spacing.


## File support and Windows installer (v1.1.0)

- **NAM models:** load `.nam` model files through the LOAD NAM control. Model compatibility depends on the NeuralAudio/NAM architecture supported by the bundled NeuralAudio version; arbitrary model formats are not interchangeable.
- **Audio files:** the same file picker can load audio files readable by JUCE's registered basic formats (typically WAV, AIFF, FLAC and Ogg Vorbis, depending on the JUCE build). The loaded audio is decoded to memory (512 MiB decoded PCM limit), resampled linearly to the host rate and looped as the plug-in's input source. To stop the WAV loop and return to normal DAW input, load a valid `.nam` or `.gen` model; successful model loading clears the WAV source automatically. The plug-in uses the first two channels of a multichannel file.
- **Limitations:** this is not a claim of support for every file extension or every NAM architecture. The reader/model loader reports unsupported files rather than treating them as valid. Audio file playback loops independently of the DAW transport; it is intended for audition/testing, not tempo-synchronised playback.


## Supported files (v33)

- `.nam`: NAM model, loaded through NeuralAudio.
- `.gen`: NAM A2 model candidate, loaded through the same NeuralAudio model loader with NAM Core enabled. Actual compatibility depends on the file containing a valid model format supported by the bundled NeuralAudio version; the extension alone cannot convert a model.
- `.wav`: audio-file loop source only. The audio decoder is explicitly limited to WAV; AIFF, FLAC, OGG, MP3 and other audio extensions are rejected.
- The LOAD NAM button opens the file picker directly and is restricted to `.nam`, `.gen`, and `.wav`. There is no secondary “Use DAW input” menu option. Loading a valid `.nam` or `.gen` model automatically clears any active WAV loop and returns input to the DAW. Unsupported extensions are rejected in the processor as well, even if selected programmatically.
- No sample `.gen` file is bundled because the project does not include a verified NAM A2 sample. Use a valid NAM A2 model file supplied by the user.
- Windows build/install success must be confirmed by the GitHub Actions run; source-level checks and ZIP integrity are not proof of a successful native build.


## UI change v34

- Removed the secondary “Use DAW input (stop audio-file loop)” menu item. The LOAD NAM button now opens the `.nam` / `.gen` / `.wav` file chooser directly. Loading a valid `.nam` or `.gen` model clears any loaded WAV loop automatically and restores DAW input.


## v46 changes
- MAX readouts turn red at +1.0 dB and above, in the main layout.
- The main interface keeps the active filename and the studio artwork visible.


## v51 main-window cleanup

- Removed all obsolete VU ONLY / SHOW ALL drawing, resizing, and hit-testing branches.
- The plug-in now opens only in the fixed 50% main interface (761 x 416 pixels).
- MAX reset hit areas remain aligned to the main-interface readouts and reset each channel independently to 0.0 dB.
- Updated CI validation to reject obsolete compact-mode references instead of incorrectly requiring them.


## v51 main-window cleanup

- Removed all obsolete compact-mode drawing, resizing, and hit-testing branches.
- The plug-in now opens only in the fixed 50% main interface (761 x 416 pixels).
- MAX reset hit areas remain aligned to the main-interface readouts and reset each channel independently to 0.0 dB.
- Updated CI validation to reject obsolete compact-mode references instead of incorrectly requiring them.
