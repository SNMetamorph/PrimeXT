---
sidebar_position: 1
description: Modernized toolkit for the Xash3D FWGS engine, supports cross-platform and have improved graphics & physics, and a lot of new features, while retaining all the features and approaches to work inherent in GoldSrc and Xash3D.
---

# Introduction
PrimeXT is the modernized toolkit for the Xash3D FWGS engine, supports cross-platform and have improved graphics & physics, and a lot of new features, while retaining all the features and approaches to work inherent in GoldSrc and Xash3D.
Based on XashXT, and therefore inherits all the functionality from XashXT and Spirit Of Half-Life. Suitable for creating both singleplayer and multiplayer mods. 

## Features
- Compatibility with most of content originally made for GoldSrc
- Eliminated many of limits that were presented in GoldSrc and vanilla Xash3D
- Significant studiomodel rendering optimizations
- Inverse kinematics and jiggle bones for studiomodels
- Studiomodels vertex weighting support (up to 4 bones per vertex)
- Extended maximum map size limit (65535x65535x65535 units)
- Support for decals on studiomodels
- Extended studiomodel limits (no more need to split into a bunch of smd-files)
- Dynamic lighting with shadows
- Rigid body physics (using PhysX engine)
- ImGui integration, could be used for in-game or development UI
- Normal mapping support
- Parallax mapping support
- Cubemap reflections
- HDR rendering, automatic exposure correction (eye adaptation effect)
- 3D skybox
- Bloom, color correction post-processing effects
- Depth-of-field post-processing effect
- Sun beams shader (sunshafts/godrays) post-processing effect
- Feature to make the map as background in the main menu
- Mirrors, monitors
- Portals, with the ability to move entities through them
- Set of utilities for mod development (asset compilers, model/sprite viewers)

## Development goals
- Support for physically based rendering (PBR), but preserving possibility to switch back to classic lighting model
- Implementing GPU-based lighting precomputing utility
- Real-time screen space reflections
- Forward+ rendering implementation
- Major rendering optimizations (depth pre-pass, flexible culling system, getting rid of legacy OpenGL code, etc.)
- Total rework of material system
- Implementing in-game material editor
- Implementing particle engine
- Implementing ragdoll physics
- Implementing vehicles
- Adding support for OpenAL Soft / Steam Audio
- Bringing the Android port to a working state

## Developers and contributors
- **SNMetamorph** - Lead developer
- **Velaron** - Help with porting to Linux
- **СASPERX69X** - Testing, documentation, logo
- **ncuxonaT** - Help with renderer development
- **Lev** - Help with renderer development
- **g-cont** - Help with common development
- **Next Day** - Testing
- **NightFox** - Testing
- **Aynekko** - Testing
- **ThomasvonWinkler** - Testing
- **KorteZZ** - Testing

If you wish, you can also take part in the development of PrimeXT - any contribution will be important for us.

More details you can find out about contributing in development on the project [Discord server](https://discord.gg/BxQUMUescJ).
