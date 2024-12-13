/*
cl_env_dynlight.h - client wrapper class for env_dynlight entity
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "hud.h"
#include "utils.h"
#include "vector.h"
#include "entity_types.h"

class CEntityEnvDynlight
{
public:
    CEntityEnvDynlight(cl_entity_t *ent);

    int GetSpotlightTextureIndex() const;
    int GetLightstyleIndex() const;
    bool Spotlight() const;
    bool DisableShadows() const;
    bool DisableBump() const;
    bool EnableLensFlare() const;
    float GetFOVAngle() const;
    float GetRadius() const;
    float GetBrightness() const;
    Vector GetColor() const;
    void GetLightVectors(Vector &origin, Vector &angles) const;

    // cinematic stuff
    int GetVideoFileIndex() const;
    bool Cinematic() const;
    float GetCinTime() const;

private:
    cl_entity_t *m_pEntity = nullptr;
};
