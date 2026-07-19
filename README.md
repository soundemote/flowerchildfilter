# Flower Child Filter

VCV Rack 2 plugin workspace for a Soundemote analog filter module.

Current state: repository skeleton only. Rack SDK setup is the next step.

## Development Setup

```sh
# Windows requirements
# 1. Install VCV Rack Free 2.6.6.
# 2. Install MSYS2.
# 3. Open "MSYS2 MinGW 64-bit", not the plain MSYS shell.
# 4. Install build tools:
pacman -Syu
pacman -Syu git wget make tar unzip zip mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb mingw-w64-x86_64-cmake autoconf automake libtool mingw-w64-x86_64-jq python zstd mingw-w64-x86_64-pkgconf

# 5. Download and extract Rack-SDK-2.6.6-win-x64.zip.
# 6. Point RACK_DIR at the extracted SDK folder:
export RACK_DIR=/c/path/to/Rack-SDK

# 7. From the plugin repo root, build once the Rack template exists:
make
make install
```

## First Target

Create one Rack module:

- analog filter DSP core
- audio input
- audio output
- frequency control
- resonance control
- mode control if the DSP exposes multiple modes
- custom panel/UI assets
- phosphor-style display widget

## Notes

- Rack DSP runs in `process(const ProcessArgs& args)`, once per engine sample.
- UI belongs in `ModuleWidget` and custom child widgets.
- Dynamic display drawing can use NanoVG through Rack's widget draw path.
- Do not allocate, block, or perform filesystem work in the audio process path.
- Selling a non-GPL Rack plugin requires checking VCV's commercial plugin licensing requirements early.
