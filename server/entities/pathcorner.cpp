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

#include "pathcorner.h"

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

BEGIN_DATADESC( CPathTrack )
	DEFINE_FIELD( m_length, FIELD_FLOAT ),
	DEFINE_FIELD( m_pnext, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_paltpath, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pprevious, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_altName, FIELD_STRING, "altpath" ),
	DEFINE_KEYFIELD( m_eOrientationType, FIELD_INTEGER, "type" ),
	DEFINE_KEYFIELD( m_iszFireFow, FIELD_STRING, "m_iszFireFow" ),
	DEFINE_KEYFIELD( m_iszFireRev, FIELD_STRING, "m_iszFireRev" ),
#ifdef PATH_SPARKLE_DEBUG
	DEFINE_FUNCTION( Sparkle ),
#endif
END_DATADESC()

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
