---
slug: alpha-oct-2021
title: Alpha release 25.10.2021
authors: snmetamorph
tags: [primext, alpha, release]
---

This is first ever release of PrimeXT, so it somehow works, but it a lot of things to fix and implement, therefore feel free to report about bugs and glitches to GitHub issues.

# Known issues
- `r_sun_allowed` should be 0
- `r_occlusion_culling` should be 0
- Invalid game directory name in `_start_primext.cmd` file (should be `primext` instead `xash`)

# Installation
1. Download and install [Xash3D FWGS engine build](https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous) (select `win32-i386` package)  
Keep in mind that Xash3D FWGS continious builds only supported, vanilla Xash3D or old FWGS builds will not work properly.
2. Download PrimeXT build [.zip file](https://github.com/SNMetamorph/PrimeXT/releases/download/alpha/primext_build_25102021.zip)
3. Copy PrimeXT files to same folder where engine binaries located.
4. Start game using `_start_primext.cmd` file
