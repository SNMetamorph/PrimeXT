# env_dynlight
Dynamic light source with adjustable light beam angle. It can work both as a spot light, and as an omnidirectional light. It also allows you to project textures/videos onto the level geometry, which can be used, for example, to implement a projector at a location with a cinema.

## Properties
- **`Name`** - Light source targetname
- **`Parent`** - Targetname of object which light source will be attached to
- **`Light Color`** - Light color (in format R G B)
- **`Light Distance`** - Light attenuation radius (in units)
- **`Brightness`** - Light brightness
- **`Cutoff Angle`** - Light beam angle (in degrees)
- **`Texture`**  - Path to texture (for projecting)
- **`Media file`** - Path to video file (for projecting)

## Spawnflags
- **`Start Off`** - Light source appears initially turned off
- **`Disable Shadows`** - Disables shadow mapping for light source
- **`Disable Bump`** - Disables normal-mapping for light source

:::tip Tip
To make the light source omnidirectional, you need to set the value to 0 for the `Cutoff Angle` parameter
:::

:::danger Warning
To avoid shadow artifacts, it is advisable not to set a value higher than 180 degrees for the `Cutoff Angle` parameter
:::
