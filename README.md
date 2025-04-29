# PrimeXT
[![Discord](https://img.shields.io/discord/824538989616824350)](https://discord.gg/BxQUMUescJ)
![GitHub Workflow Status (branch)](https://img.shields.io/github/actions/workflow/status/SNMetamorph/PrimeXT/nightly-builds.yml?branch=master)
![GitHub release (by tag)](https://img.shields.io/github/downloads/SNMetamorph/PrimeXT/total)
![GitHub repo size](https://img.shields.io/github/repo-size/SNMetamorph/PrimeXT)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/SNMetamorph/PrimeXT)

[![ModDB Rating](https://button.moddb.com/popularity/medium/mods/56077.png)](https://www.moddb.com/mods/primext)

Modernized toolkit for Xash3D FWGS engine, with extended physics, improved graphics and a lot of other new features for mod-makers. Based on XashXT and Spirit Of Half-Life and includes features and entities from it.

## Features, brief overview
- HDR rendering support
- Parallax-corrected cubemaps
- Physically based rendering support (in progress)
- Dynamic lighting with shadow mapping (omnidirectional, cascaded, etc.)
- Normal mapping, parallax mapping support
- Advanced post-processing: bloom, depth-of-field, color correction, SSAO
- PhysX engine integration
- Eliminated many of limits that were presented in GoldSrc and vanilla Xash3D

## Projects that are based on PrimeXT
- [Ionization](https://www.moddb.com/games/ionization)
- [Half-Life: History of Kumertau](https://www.moddb.com/mods/half-life-history-of-kumertau)
- ["Zemlya Rodnaya" in Novy Urengoy](https://www.moddb.com/mods/school-2-in-novy-urengoy-recreated-on-xash3d)
- [Metro 2031: Last Chance](https://www.moddb.com/mods/metro-2031-last-chance)

## Development goals
At this time, project in primal state: it somehow works, but there are a lot of things to fix or implement next. You can discuss with community members and ask questions in our [Discord](https://discord.gg/BxQUMUescJ) server.

We would be very grateful to potential contributors. Main development goals of this project is:
- Optimizing brushes rendering (clustered forward rendering, getting rid of legacy OpenGL code)
- Implementing lighting precomputation in HDR format
- Total reworking of material system
- Implementing particle engine, something like in Source Engine
- Improving physics futher: ragdolls, vehicles, fine-tuning, etc. 
- Improving cross-platform: developing Android port, supporting other architectures like ARM or RISC-V
- Writing actual documentation, translating existing pages to English
- Code refactoring (where it is really necessary, there is no goal to refactor everything)

You can see the full list of project goals and a detailed description on the [documentation site](https://snmetamorph.github.io/PrimeXT/), but it is still a work in progress. 
So feel free to make suggestions on what should be documented first.

## Installation
You can read the detailed installation guide on our documentation site: available on [English](https://snmetamorph.github.io/PrimeXT/docs/eng/installation) and [Russian](https://snmetamorph.github.io/PrimeXT/docs/rus/installation) languages.

## Building
> NOTE: Never download sources from GitHub manually, because it doesn't include external depedencies, you SHOULD use Git clone instead.
1) Install [Git](https://git-scm.com/download/win) for cloning project
2) Clone this repository, enter these commands to Git console:
```
git clone --recursive https://github.com/SNMetamorph/PrimeXT.git
cd PrimeXT
```

Next steps will be vary according to your development environment and tools.

### Windows (using Visual Studio)
3) Open cloned repository directory as CMake folder with Visual Studio (you can use VS2019 or VS2022)  
4) Select desired build preset, for example you can use `Windows / x64 / Debug`. You can see other available presets in [`CMakePresets.json`](/CMakePresets.json) file.
5) In `Build` menu select `Build solution`, or you can use hotkey `Ctrl+Shift+B` instead. Wait for completion.
6) Compiled binaries locates in `build\x\bin` and `build\x\devkit`, where `x` is your build configuration name, in this case it will be "Debug".

### Linux (using CMake)
This example shows how to build project for Linux with x64 architecture. Of course, you can set another target platform, check [`CMakePresets.json`](/CMakePresets.json) file for more available presets.

Tested on Ubuntu 18.04 and Ubuntu 22.04, but also will work on other Linux distributions which uses `apt` package manager, such as Debian.  

3) Install required packages:
```
sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib cmake ninja-build 
sudo apt-get install curl zip unzip pkgconfig
sudo apt-get install qtbase5-dev
```
4) Prepare build environment and configure project:
```
external/vcpkg/bootstrap-vcpkg.sh
cmake -E make_directory ./build
cd build
cmake .. --preset linux-x64-debug
```
5) Build project:
```
cmake --build .
```
6) Compiled binaries will be located in `build` and `build\primext\bin` directories.
