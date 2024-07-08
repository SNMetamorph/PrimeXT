---
sidebar_position: 1
description: Modern Half-Life 1 SDK for the Xash3D FWGS engine, supports cross-platform and have improved graphics & physics, and a lot of new features, while retaining all the features and approaches to work inherent in GoldSrc and Xash3D.
---

# Introduction
PrimeXT - modern Half-Life 1 SDK for the Xash3D FWGS engine, supports cross-platform and have improved graphics & physics, and a lot of new features, while retaining all the features and approaches to work inherent in GoldSrc and Xash3D.
Based on XashXT, and therefore inherits all the functionality from XashXT and Spirit Of Half-Life. Suitable for creating both singleplayer and multiplayer mods. 

### Current functionality
- Compatibility with most of GoldSrc mods
- Absence of many limits inherent in GoldSrc and vanilla Xash3D
- Studiomodel rendering optimizations
- Inverse kinematics and jiggle bones for models
- Studiomodels weighting support
- Extended maximum map size limit (65535x65535x65535 units)
- Automatic exposure correction (eye adaptation effect)
- Support for decals on studiomodels
- Extended model limits (no more need to split into a bunch of smd-files)
- Dynamic lighting with shadows
- Rigid body physics (using PhysX engine)
- Normal mapping support
- Parallax mapping support
- Cubemap reflections
- HDR-rendering
- 3D skybox
- Bloom
- Sun beams shader (sunshafts/godrays)
- Feature to make the map as background in the main menu
- Mirrors
- Monitors
- Portals, with the ability to move entities through them
- Bunch of utilities for mod development (asset compilers, model/sprite viewers) 

### Future plans
- Support for physically based rendering (PBR), but preserving possibility to switch back to classic lighting model
- Implement GPU-based light baking utility
- Real-time screen space reflections
- Forward+ rendering implementation
- Major rendering optimizations (depth pre-pass, flexible culling system, etc.)
- Total rework of material system
- Implement in-game material editor
- Implement particle engine
- Implement ragdoll physics
- Implement vehicles
- Adding support for OpenAL Soft / Steam Audio
- Bringing the Android port to a working state

### Developers and contributors
- **SNMetamorph** - Lead developer
- **Velaron** - Help with porting to Linux
- **СASPERX69X** - Testing, documentation, logo
- **ncuxonaT** - Help with renderer development
- **Lev** - Help with renderer development
- **g-cont** - Help with common development
- **Next Day** - Testing
- **Aynekko** - Testing
- **ThomasvonWinkler** - Testing
- **KorteZZ** - Testing

If you wish, you can also take part in the development of PrimeXT - any contribution will be important for us. More details
you can find out about contributing in development on the project [Discord server](https://discord.gg/BxQUMUescJ).
