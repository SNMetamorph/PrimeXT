# PrimeXT
This is modern SDK for Xash3D engine, with extended physics (PhysX used), improved graphics (renderer from P2:Savior with dynamic lighting/shadowing, PBR support, parallax-mapping, etc.)<br>
At this time, project in primal state: it somehow works, but there is a lot of thing to fix/implement next.<br>
We need interested people to work on this SDK with us! Main goals of this project is:
- Optimizing world rendering as much as possible<br>
- Implementing HDR rendering pipeline<br>
- Updating PhysX headers to modern SDK version<br>
- Implementing particle engine, something like in Source Engine<br>
- Cross-platform<br>
- Writing actual documentation<br>
- Code refactoring<br>

You can discuss with community members in our [Discord](discord.gg/BxQUMUescJ) server.
## Building
1) Install [Python](https://python.org), it will be used next for waf build system<br>
Install [Git](https://git-scm.com/download/win) for cloning project
2) Clone this repository, following the instruction.
```
git clone --recursive https://github.com/SNMetamorph/xashxt-fwgs.git
cd xashxt-fwgs
git submodule update --init --recursive
```

3) Configure project using `waf configure` command<br>
For example, you can use this configuration.<br>
Here `..\build-x86` is output directory<br>
```
waf configure -T debug —prefix=..\build-x86 —enable-physx
```

4) Build project using `waf build` command
5) Copy compiled binaries to output directory using `waf install` command
