# PrimeXT
[![Discord](https://img.shields.io/discord/824538989616824350)](https://discord.gg/BxQUMUescJ)
![GitHub Workflow Status (branch)](https://img.shields.io/github/actions/workflow/status/SNMetamorph/PrimeXT/nightly-builds.yml?branch=master)
![GitHub release (by tag)](https://img.shields.io/github/downloads/SNMetamorph/PrimeXT/total)
![GitHub repo size](https://img.shields.io/github/repo-size/SNMetamorph/PrimeXT)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/SNMetamorph/PrimeXT)

[![ModDB Rating](https://button.moddb.com/popularity/medium/mods/56077.png)](https://www.moddb.com/mods/primext)

Modern SDK for Xash3D engine, with extended physics (using PhysX), improved graphics (dynamic lighting with shadows, HDR, cubemap/screen-space reflections, PBR support, parallax-mapping, bloom, color correction, SSAO, etc). Based on XashXT and Spirit Of Half-Life and includes all features and entities from it.

At this time, project in primal state: it somehow works, but there is a lot of things to fix/implement next. You can discuss with community members and ask questions in our [Discord](https://discord.gg/BxQUMUescJ) server.

We need interested people to work on this SDK with us! Main goals of this project is:
- Optimizing world rendering as much as possible
- Implementing HDR rendering pipeline
- Total rework of material system
- Improving physics futher: ragdolls, vehicles, fine-tuning, etc. 
- Implementing particle engine, something like in Source Engine
- Cross-platform (Windows and Linux supported, Android port in plans)
- Writing actual documentation, translating existing pages to English
- Code refactoring (where it really needed)

Full list of project goals you can see on documetation site, it's available [here](https://snmetamorph.github.io/PrimeXT/), but now it's still in progress. 
Therefore, you can tell suggestion about what should be documented at first.  

## Projects that are based on PrimeXT
- [Ionization](https://www.moddb.com/games/ionization)
- [Half-Life: History of Kumertau](https://www.moddb.com/mods/half-life-history-of-kumertau)
- ["Zemlya Rodnaya" in Novy Urengoy](https://www.moddb.com/mods/school-2-in-novy-urengoy-recreated-on-xash3d)
- [Metro 2031: Last Chance](https://www.moddb.com/mods/metro-2031-last-chance)

## Installation
Detailed installation guide you can read on our documentation site: available on [english](https://snmetamorph.github.io/PrimeXT/docs/eng/installation) and [russian](https://snmetamorph.github.io/PrimeXT/docs/rus/installation).

## Building
> NOTE: Never download sources from GitHub manually, because it doesn't include external depedencies, you SHOULD use Git clone instead.
1) Install [Git](https://git-scm.com/download/win) for cloning project
2) Clone this repository: enter these commands to Git console
```
git clone --recursive https://github.com/SNMetamorph/PrimeXT.git
cd PrimeXT
```
Next steps will be vary according to your development environment and tools.

#### Windows (using Visual Studio)
3) Open cloned repository directory as CMake folder with Visual Studio (you can use VS2019 or VS2022)  
4) Select desired build preset, for example you can use `Windows / x64 / Debug`. You can see other available presets in [`CMakePresets.json`](/CMakePresets.json) file.
5) In `Build` menu select `Build solution`, or you can use hotkey `Ctrl+Shift+B` instead. Wait for completion.
6) Compiled binaries locates in `build\x\bin` and `build\x\devkit`, where `x` is your build configuration name, in this case it will be "Debug".

#### Linux (using CMake)
This example shows how to build project for Linux with x64 architecture. Of course, you can set another target platform, see [`CMakePresets.json`](/CMakePresets.json) file for more available presets.
Tested on Ubuntu 18.04 and Ubuntu 22.04, but also will work on other Linux distributions which uses `apt` package manager.  

3) Install build depedencies
```
sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib cmake ninja-build 
sudo apt-get install curl zip unzip pkgconfig
sudo apt-get install qtbase5-dev
```
4) Prepare build environment and configure project
```
external/vcpkg/bootstrap-vcpkg.sh
cmake -E make_directory ./build
cd build
cmake .. --preset linux-x64-debug
```
5) Build project:
```
cmake --build . --config Debug
```
6) Compiled binaries will be located in `build` and `build\primext\bin` directories
