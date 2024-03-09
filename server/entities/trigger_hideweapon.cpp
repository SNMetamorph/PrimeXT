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
***/

#include "trigger_hideweapon.h"
#include "player.h"

LINK_ENTITY_TO_CLASS( trigger_hideweapon, CTriggerHideWeapon );

void CTriggerHideWeapon::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance(INDEXENT( 1 ));

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( !pActivator || !ShouldToggle( useType, FBitSet( pPlayer->m_iHideHUD, HIDEHUD_WEAPONS )))
		return;

	if (pPlayer->m_iHideHUD & HIDEHUD_WEAPONS)
		pPlayer->HideWeapons(FALSE);
	else pPlayer->HideWeapons(TRUE);

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}
