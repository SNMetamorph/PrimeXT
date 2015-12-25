/*
r_weather.cpp - rain and snow code based on original code by BUzer
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

#ifndef R_WEATHER_H
#define R_WEATHER_H

#define DRIPSPEED			900	// speed of raindrips (pixel per secs)
#define SNOWSPEED			200	// speed of snowflakes
#define SNOWFADEDIST		80

#define MAX_RAIN_VERTICES		65536	// snowflakes and waterrings draw as quads
#define MAX_RAIN_INDICES		MAX_RAIN_VERTICES * 4

// this limits can be blindly increased
#define MAXDRIPS			40000	// max raindrops
#define MAXFX			20000	// max effects

#define MODE_RAIN			0
#define MODE_SNOW			1

#define DRIP_SPRITE_HALFHEIGHT	46
#define DRIP_SPRITE_HALFWIDTH		8
#define SNOW_SPRITE_HALFSIZE		3
#define MAX_RING_HALFSIZE		25	

typedef struct cl_drip
{
	float		birthTime;
	float		minHeight;	// minimal height to kill raindrop
	Vector		origin;
	float		alpha;

	float		xDelta;		// side speed
	float		yDelta;
	int		landInWater;
} cl_drip_t;

typedef struct cl_rainfx
{
	float		birthTime;
	float		life;
	Vector		origin;
	float		alpha;
} cl_rainfx_t;

void R_InitWeather( void );
void R_ResetWeather( void );
void R_DrawWeather( void );
void R_ParseRainMessage( void );

#endif//R_WEATHER_H
