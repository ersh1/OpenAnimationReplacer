# OpenAnimationReplacer

A SKSE framework plugin that replaces animations depending on configurable conditions. In-game editor. Backwards compatible with more features. Extensible by other SKSE plugins. Supports SE/AE/VR. Open source.

[Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/92109)

## Requirements

- [CMake](https://cmake.org/)
  - Add this to your `PATH`
- [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
- [Vcpkg](https://github.com/microsoft/vcpkg)
  - Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
- [Visual Studio Community 2019](https://visualstudio.microsoft.com/)
  - Desktop development with C++
- [CommonLibSSENG](https://github.com/alandtse/CommonLibVR/tree/ng)
  - Add this as as an environment variable `CommonLibSSEPath`

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Register Visual Studio as a Generator

- Open `x64 Native Tools Command Prompt`
- Run `cmake`
- Close the cmd window

## Building

```
git clone https://github.com/ersh1/OpenAnimationReplacer.git
cd OpenAnimationReplacer
# pull commonlib /extern to override the path settings
git submodule init
# to update submodules to checked in build
git submodule update
#configure cmake
cmake --preset vs2022-windows
#build dll
cmake --build build --config Release
```

## License

[GPL-3.0-or-later](COPYING) WITH [Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)](EXCEPTIONS). Specifically, the Modded Code is Skyrim (and its variants) and Modding Libraries include [SKSE](https://skse.silverlock.org/) and Commonlib (and variants).
