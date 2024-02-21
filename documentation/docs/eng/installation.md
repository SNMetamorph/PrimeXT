---
sidebar_position: 3
---

# Installation
This manual describes the installation of the latest build of PrimeXT, in the future, for release builds, the algorithm will be slightly different.
If you already have the engine installed, you can skip step 1.
It is recommended to periodically manually update the engine, as development does not stand still and bug fixes and new functionality periodically appear.

:::tip Tip
Keep in mind that PrimeXT only supports latest Xash3D FWGS builds, vanilla Xash3D or old FWGS builds will not work properly.
:::

## 1. Engine installation 
- Select and download [Xash3D FWGS engine build](https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous) for your 
platform (in case of Windows, use file `xash3d-fwgs-win32-i386.7z` for 32-bit, or file `xash3d-fwgs-win32-amd64.7z` for 64-bit), then unpack all the files from the archive into a some directory. 
In next steps, the "engine directory" will mean this exact directory.
- Copy folder `valve` from your bought [Half-Life 1](https://store.steampowered.com/app/70/HalfLife/) copy to the engine directory.
- Run `xash3d.exe`/`xash3d.sh`/`xash3d` depending on your platform.

## 2. PrimeXT development build installation
- Download [PrimeXT development build](https://github.com/SNMetamorph/PrimeXT/releases/tag/continious) for your 
platform, then unpack all the files from the archive into the engine directory. Please note that engine build and PrimeXT build must be for the same platform and architecture.
- Download [PrimeXT content](https://drive.google.com/file/d/1l3voCVdNi_SlFrOI31ZwABWLQXXUW-Zc/view?usp=sharing) and copy folder `primext` from archive into engine directory.
- Installation completed! You can run game using `primext.exe`/`primext_run` file.
