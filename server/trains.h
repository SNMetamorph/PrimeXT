/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef TRAINS_H
#define TRAINS_H

#include "path_track.h"
#include "func_tracktrain.h"

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN		0x00000008
#define SF_PATH_TELEPORT		0x00000010
#define SF_PATH_ALTERNATE		0x00008000

// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG		0x001
#define SF_CORNER_TELEPORT		0x002
#define SF_CORNER_FIREONCE		0x004

enum TrainVelocityType_t
{
        TrainVelocity_Instantaneous = 0,
        TrainVelocity_LinearBlend,
        TrainVelocity_EaseInEaseOut,
};

enum TrainOrientationType_t
{
        TrainOrientation_Fixed = 0,
        TrainOrientation_AtPathTracks,
        TrainOrientation_LinearBlend,
        TrainOrientation_EaseInEaseOut,
};

enum TrackOrientationType_t
{
	TrackOrientation_Fixed = 0,
	TrackOrientation_FacePath,
	TrackOrientation_FacePathAngles,
};

#endif//TRAINS_H
