# XashXT fork (will be transformed to PrimeXT)
## Building
1) Install [Python](https://python.org), it will be used for waf build system<br>
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
