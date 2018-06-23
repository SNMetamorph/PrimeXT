/*
pm_movevars.h - shared physics variables.
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef PM_MOVEVARS_H
#define PM_MOVEVARS_H

typedef struct movevars_s
{
	float	gravity;           // Gravity for map
	float	stopspeed;         // Deceleration when not moving
	float	maxspeed;          // Max allowed speed
	float	spectatormaxspeed;
	float	accelerate;        // Acceleration factor
	float	airaccelerate;     // Same for when in open air
	float	wateraccelerate;   // Same for when in water
	float	friction;          
	float	edgefriction;	   // Extra friction near dropofs 
	float	waterfriction;     // Less in water
	float	entgravity;        // 1.0
	float	bounce;            // Wall bounce value. 1.0
	float	stepsize;          // sv_stepsize;
	float	maxvelocity;       // maximum server velocity.
	float	zmax;			   // Max z-buffer range (for GL)
	float	waveHeight;		   // Water wave height (for GL)
	qboolean	footsteps;        // Play footstep sounds
	char	skyName[32];	   // Name of the sky map
	float	rollangle;
	float	rollspeed;
	float	skycolor_r;			// Sky color
	float	skycolor_g;			// 
	float	skycolor_b;			//
	float	skyvec_x;			// Sky vector
	float	skyvec_y;			// 
	float	skyvec_z;			// 
	int	features;		// engine features that shared across network
	int	fog_settings;	// Global fog settings (packed color+density) 
	float	wateralpha;	// World water alpha 1.0 - solid 0.0 - transparent
} movevars_t;

#endif//PM_MOVEVARS_H
