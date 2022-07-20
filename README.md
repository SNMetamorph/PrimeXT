# PrimeXT
[![Discord](https://img.shields.io/discord/824538989616824350)](https://discord.gg/BxQUMUescJ)
![GitHub Workflow Status (branch)](https://img.shields.io/github/workflow/status/SNMetamorph/PrimeXT/nightly-builds/master)
![GitHub release (by tag)](https://img.shields.io/github/downloads/SNMetamorph/PrimeXT/total)
![GitHub top language](https://img.shields.io/github/languages/top/SNMetamorph/PrimeXT)
![GitHub repo size](https://img.shields.io/github/repo-size/SNMetamorph/PrimeXT)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/SNMetamorph/PrimeXT)

Modern SDK for Xash3D engine, with extended physics (using PhysX), improved graphics (dynamic lighting with shadows, HDR, cubemap/screen-space reflections, PBR support, parallax-mapping, bloom, color correction, SSAO, etc). 
Based on XashXT and Spirit Of Half-Life and includes all features and entities from it.  
At this time, project in primal state: it somehow works, but there is a lot of things to fix/implement next.  
We need interested people to work on this SDK with us! Main goals of this project is:
- Optimizing world rendering as much as possible
- Implementing HDR rendering pipeline
- Updating PhysX headers to modern SDK version
- Implementing particle engine, something like in Source Engine
- Cross-platform (now Windows and Linux supported, Android in plans)
- Writing actual documentation, translating it to English (in process)
- Code refactoring

Full list of project goals you can see on documetation site, it's available [here](https://snmetamorph.github.io/PrimeXT/), but now it's still in progress. 
Therefore, you can tell suggestion about what should be documented at first.  
You can discuss with community members and ask questions in our [Discord](https://discord.gg/BxQUMUescJ) server.

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
### Windows (using Visual Studio)
3) Open cloned repository directory as CMake folder with Visual Studio (project tested with VS2019, but more later version will works also)  
4) Select desired build configuration, highly recommended to use `x86-Debug`
5) In `Build` menu select `Build solution`, or you can use `Ctrl+Shift+B` hotkey instead. Wait for completion.
6) Compiled binaries locates in `build\x\bin` and `build\x\devkit`, where `x` is your build configuration name
### Linux (using CMake)
Tested on Ubuntu 18.04 and Ubuntu 22.04. Probably it'll work on Debian too.  
3) Install build depedencies
```
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib cmake
sudo apt-get install qtbase5-dev:i386
```
4) Prepare build environment and configure project
```
cmake -E make_directory ./build
cd build
cmake .. -DCMAKE_C_FLAGS="-m32" -DCMAKE_CXX_FLAGS="-m32" -DENABLE_PHYSX=OFF
```
5) Build project: `cmake --build . --config Debug`
6) Compiled binaries will be located in `build` and `build\primext\bin` directories
