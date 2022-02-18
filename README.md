# PrimeXT
Modern SDK for Xash3D engine, with extended physics (PhysX used), improved graphics (dynamic lighting with shadows, HDR, cubemap/screen-space reflections, PBR support, parallax-mapping, bloom, color correction, SSAO, etc.)<br>
At this time, project in primal state: it somehow works, but there is a lot of thing to fix/implement next.<br>
We need interested people to work on this SDK with us! Main goals of this project is:
- Optimizing world rendering as much as possible<br>
- Implementing HDR rendering pipeline<br>
- Updating PhysX headers to modern SDK version<br>
- Implementing particle engine, something like in Source Engine<br>
- Cross-platform (Android version in experimental stage)<br>
- Writing actual documentation<br>
- Code refactoring<br>

You can discuss with community members and ask questions in our [Discord](https://discord.gg/BxQUMUescJ) server.
## Building
1) Install [Git](https://git-scm.com/download/win) for cloning project
2) Clone this repository: enter these commands to Git console
```
git clone --recursive https://github.com/SNMetamorph/PrimeXT.git
cd PrimeXT
git submodule update --init --recursive
```
3) Open cloned repository directory as CMake folder with Visual Studio (project tested with VS2019, but more later version will works also)<br>
4) Select desired build configuration, usually it's `x86-Debug`
4) In `Build` menu select `Build solution`, or you can use `Ctrl+Shift+B` hotkey instead
5) Wait until everything will built
6) Compiled binaries locates in `build\x\bin` and `build\x\devkit`, where `x` is your build configuration name
