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
//
// ========================== PATH_CORNER ===========================
//

#include "path_corner.h"

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

void CPathCorner::Spawn(void) 
{
	ASSERTSZ(!FStringNull(pev->targetname), "path_corner without a targetname"); 
}

int CPathCorner::ObjectCaps(void) 
{
	return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; 
}

float CPathCorner::GetDelay(void) 
{
	return m_flWait; 
}
