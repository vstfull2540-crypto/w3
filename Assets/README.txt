VUMister NAM - PNG GUI assets
Source: user-provided 1585x992 interface image.
Files:
- vu_meter_input.png / vu_meter_output.png: cropped VU meter assemblies
- logo_brand.png: approximate dark-background removal for logo
- knob_input.png / knob_mix.png / knob_output.png: circular crops of knobs
- bypass_switch.png: switch crop
- model_selector.png: model dropdown crop
- load_nam_button.png: Load NAM button crop
- status_indicator.png: green status LED crop
- reference_full_interface.png: original reference image

Notes:
These are raster crops from the provided screenshot, not individually redrawn vector assets.
Some controls still include parts of the original dark panel due to the source image.
Use ImageCache in JUCE to load them; controls need separate hit-testing/parameter bindings in C++.
