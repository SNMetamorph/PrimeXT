---
sidebar_position: 4
description: Guide to the PrimeXT material system for level-designers and 3D-modellers — physical materials, visual properties, texture binding, and texture naming conventions.
---

# Working with Materials

PrimeXT uses a simple file-based material system. All settings live in text files under the `scripts/` folder — no coding required.

There are two kinds of materials:

- **Physical materials** — define how a surface behaves on interaction: footstep sounds, impact sounds, decals, particles. Declared in `scripts/materials.def`.
- **Visual materials** — define how a surface looks: smoothness, reflections, normal maps, parallax, etc. Declared in `scripts/*.mat` files. These also link to a physical material.

---

## Physical Materials (`scripts/materials.def`)

This file lists all physical materials. Each entry connects a surface type to sounds and effects.

```
"default"
{
    "impact_decal"    "shot"
    "impact_parts"    "test_impact" "test_smoke"
    "impact_sound"    "debris/concrete1.wav" "debris/concrete2.wav" "debris/concrete3.wav" "debris/concrete4.wav"
    "step_sound"      "materials/pstep_1.wav" "materials/pstep_2.wav" "materials/pstep_3.wav"
}

"metal"
{
    "impact_decal"    "shot_metal"
    "impact_parts"    "metal_flash" "metal_spark"
    "impact_sound"    "debris/metal1.wav" "debris/metal2.wav" "debris/metal3.wav"
    "step_sound"      "player/pl_metal1.wav" "player/pl_metal2.wav" "player/pl_metal3.wav" "player/pl_metal4.wav"
}
```

### Properties

| Property | What it does | Max count |
|----------|-------------|-----------|
| `impact_decal` | Decal group name (defined in `gfx/decals/decalinfo.txt`) | 1 |
| `impact_parts` | Particle effects spawned on impact | 8 |
| `impact_sound` | Sound files played when something hits this surface (path relative to `sound/`) | 8 |
| `step_sound` | Sound files played when walking on this surface | 8 |

You can add any number of custom materials — just add a new block with a unique name, then bind it to textures via `.mat` files.

If no `"default"` block exists in the file, the engine creates a built-in fallback with concrete sounds.

---

## Visual Materials (`scripts/*.mat`)

`.mat` files in the `scripts/` directory are loaded automatically. You can have one file or many — they all get merged. Each block in a `.mat` file describes the visual properties of a specific texture, and can also link it to a physical material.

### Full Property Reference

```
"texture_name"
{
    "material"       "metal"       // links to a physical material from materials.def
    "smoothness"     "0.8"         // how smooth the surface is: 0.0 (rough) to 1.0 (mirror-like)
    "reflectScale"   "0.3"         // reflection brightness: 0.0 to 1.0
    "refractScale"   "0.5"         // refraction strength (only for transparent surfaces): 0.0 to 1.0
    "aberrationScale" "0.01"       // chromatic aberration effect (transparent surfaces): 0.0 to 0.1
    "reliefScale"    "0.02"        // parallax mapping height: 0.0 to 1.0 (requires a heightmap texture)
    "swayHeight"     "10"          // height from model origin where vegetation swaying starts; 0 = disabled
    "detailScale"    "4 4"         // tiling scale of the detail texture (X Y)
    "detailmap"      "textures/grunge_detail"  // custom detail texture path
    "diffuseMap"     "textures/my_diffuse"     // custom diffuse (albedo) texture path
    "normalMap"      "textures/my_normal"      // custom normal map path
    "glossMap"       "textures/my_gloss"       // custom gloss / ORM texture path

    // old format — automatically converted to smoothness
    "glossExp"       "32"          // gloss exponent (0-256)
}
```

### Default Values

| Property | Default | Notes |
|----------|---------|-------|
| `smoothness` | 0.0 | |
| `detailScale` | 10.0 10.0 | |
| `reflectScale` | 0.0 | |
| `refractScale` | 0.0 | |
| `aberrationScale` | 0.0 | |
| `reliefScale` | 0.0 | |
| `swayHeight` | 0 | disabled |
| `material` | built-in default | concrete-like material |

### `glossExp` (old) → `smoothness` (new)

If you have old content using `glossExp` (0–256 range), it is converted automatically:  
`smoothness = sqrt(glossExp / 256.0)`

---

## Binding Materials to Textures

### Brush (world) textures

Use the exact texture name as the block name:

```
"WALL_CONCRETE01"
{
    "material"    "concrete"
    "smoothness"  "0.3"
    "reliefScale" "0.01"
}
```

### Studiomodel textures

Use the format `"modelname/texturename"`:

```
"box/body"
{
    "material"    "wood"
}

"v_9mmAR/body"
{
    "material"    "metal"
    "reflectScale" "0.3"
}
```

The engine first tries the model-specific key (`box/body`), then falls back to the bare texture name (`body`).

---

## Texture Map Naming Conventions

For each texture, PrimeXT automatically looks for additional map files using suffixes. If you specify `diffuseMap`, `normalMap`, `glossMap`, or `detailmap` in a `.mat` file, those paths are used directly. Otherwise, the engine looks up these suffixes:

| Suffix | Map type | Example |
|--------|----------|---------|
| `_norm` | Normal map | `wall1_norm.dds` |
| `_gloss` or `_pbr` | Gloss / ORM texture | `wall1_gloss.dds` |
| `_luma` | Self-illumination (emissive) | `wall1_luma.dds` |
| `_hmap` | Heightmap (parallax) | `wall1_hmap.dds` |
| `_detail` | Detail texture | `wall1_detail.dds` |

For studiomodels, place external textures at: `textures/modelname/texturename[_suffix].[tga|dds]`

---

## Terrain / Landscape Layers

PrimeXT supports multi-layer terrain blending. A single brush face can have up to **16 texture layers** controlled by a heightmap.

### Setup

1. **Heightmap image** — a grayscale `.tga` named `maps/mapname_heightmap.tga`
2. **Layer definitions** — a text file `maps/mapname_land.txt` that assigns textures and physical materials to height values
3. Each layer can have its own physical material (footstep sounds, impact effects, etc.)

The engine samples the heightmap at the contact point to determine which layer is active, then uses that layer's physical material for sound and decal effects.

---

## PBR Gloss Map Channel Layout

When using the physically-based rendering mode (`r_lighting_brdf 1`), the `_gloss` or `_pbr` texture packs four parameters into its RGBA channels:

| Channel | Parameter | Range |
|---------|-----------|-------|
| R | Smoothness | 0.0 (rough) – 1.0 (smooth) |
| G | Metalness | 0.0 (dielectric) – 1.0 (pure metal) |
| B | Ambient occlusion | 0.0 (fully occluded) – 1.0 (no occlusion) |
| A | Specular intensity | 0.0 (no specular) – 1.0 (full) |

Smoothness is the inverse of roughness: `smoothness = 1.0 - roughness`.

---

## GoldSrc Texture Type Compatibility

Old GoldSrc maps and mods use a single-character system to identify surface types. These characters are still recognised by PrimeXT:

| Character | Surface type |
|-----------|--------------|
| `'C'` | Concrete |
| `'M'` | Metal |
| `'D'` | Dirt |
| `'V'` | Vent |
| `'G'` | Grate |
| `'T'` | Tile |
| `'S'` | Slosh (shallow water) |
| `'W'` | Wood |
| `'P'` | Computer / electronic |
| `'Y'` | Glass |
| `'F'` | Flesh |
