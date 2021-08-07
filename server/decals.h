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
#ifndef DECALS_H
#define DECALS_H

//
// Dynamic Decals
//
enum decal_e
{
	DECAL_GUNSHOT1 = 0,   // "{shot1",	0 },
	DECAL_GUNSHOT2, // "{shot2",	0 },		
	DECAL_GUNSHOT3, // "{shot3",0 },			
	DECAL_GUNSHOT4, // "{shot4",	0 },		
	DECAL_GUNSHOT5, // "{shot5",	0 },		
	DECAL_LAMBDA1, // "{lambda01", 0 },	
	DECAL_LAMBDA2, // "{lambda02", 0 },	
	DECAL_LAMBDA3, // "{lambda03", 0 },	
	DECAL_LAMBDA4, // "{lambda04", 0 },	
	DECAL_LAMBDA5, // "{lambda05", 0 },	
	DECAL_LAMBDA6, // "{lambda06", 0 },	
	DECAL_SCORCH1, // "{scorch1", 0 },	
	DECAL_SCORCH2, // "{scorch2", 0 },	
	DECAL_BLOOD1,  // "{blood1", 0 },
	DECAL_BLOOD2,  // "{blood2", 0 },
	DECAL_BLOOD3,  // "{blood3", 0 },
	DECAL_BLOOD4,  // "{blood4", 0 },
	DECAL_BLOOD5,  // "{blood5", 0 },
	DECAL_BLOOD6,  // "{blood6", 0 },
	DECAL_YBLOOD1,  // "{yblood1", 0 },	
	DECAL_YBLOOD2,  // "{yblood2", 0 },	
	DECAL_YBLOOD3,  // "{yblood3", 0 },	
	DECAL_YBLOOD4,  // "{yblood4", 0 },	
	DECAL_YBLOOD5,  // "{yblood5", 0 },	
	DECAL_YBLOOD6,  // "{yblood6", 0 },	
	DECAL_GLASSBREAK1, // "{break1", 0 },	
	DECAL_GLASSBREAK2, // "{break2", 0 },	
	DECAL_GLASSBREAK3, // "{break3", 0 },
	DECAL_BIGSHOT1, // "{bigshot1", 0 },		
	DECAL_BIGSHOT2, // "{bigshot2", 0 },		
	DECAL_BIGSHOT3, // "{bigshot3", 0 },		
	DECAL_BIGSHOT4, // "{bigshot4", 0 },		
	DECAL_BIGSHOT5, // "{bigshot5", 0 },		
	DECAL_SPIT1, // "{spit1", 0 }
	DECAL_SPIT2, // "{spit2", 0 }
	DECAL_BPROOF1, // "{bproof1", 0 },
	DECAL_GARGSTOMP1,		 // "{gargstomp", 0 }
	DECAL_SMALLSCORCH1,	 // "{smscorch1", 0 },
	DECAL_SMALLSCORCH2,	 // "{smscorch2", 0 },
	DECAL_SMALLSCORCH3, // "{smscorch3", 0 },
	DECAL_MOMMABIRTH,		 // "{mommablob", 0 },
	DECAL_MOMMASPLAT, // "{mommablob", 0 },	
};

typedef struct 
{
	char	*name;
	int	index;
} DLL_DECALLIST;

//extern DLL_DECALLIST	gDecals[];

#endif// DECALS_H
