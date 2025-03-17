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
/*

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#include "cycler_weapon.h"
#include "server_weapon_layer_impl.h"
#include <utility>

LINK_ENTITY_TO_CLASS( cycler_weapon, CWeaponCycler );
LINK_ENTITY_TO_CLASS( weapon_question, CWeaponCycler );	// saverestore issues

#define DEFINE_CYCLERWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CCyclerWeaponContext *ctx = static_cast<CCyclerWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(pData, &ctx->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CCyclerWeaponContext *ctx = static_cast<CCyclerWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(&ctx->x, pData, dataSize); \
	})
	

BEGIN_DATADESC( CWeaponCycler )
	DEFINE_CYCLERWEAPON_FIELD( m_iPlayerModel, FIELD_MODELNAME ),
	DEFINE_CYCLERWEAPON_FIELD( m_iViewModel, FIELD_MODELNAME ),
	DEFINE_CYCLERWEAPON_FIELD( m_iWorldModel, FIELD_MODELNAME ),
END_DATADESC()


CCyclerWeaponContext::CCyclerWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer)),
	m_iPlayerModel(0),
	m_iWorldModel(0),
	m_iViewModel(0)
{
	m_iId = WEAPON_CYCLER;
}

int CCyclerWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = "weapon_question";	// need for right HUD displaying
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId;
	p->iWeight = -1;
	return 1;
}

bool CCyclerWeaponContext::Deploy()
{
	CWeaponCycler *pWeapon = static_cast<CWeaponCycler*>(m_pLayer->GetWeaponEntity());
	return DefaultDeploy( (char *)STRING( m_iViewModel ), (char *)STRING( m_iPlayerModel ), pWeapon->pev->impulse, "onehanded" );
}

void CCyclerWeaponContext::Holster()
{
	CWeaponCycler *pWeapon = static_cast<CWeaponCycler*>(m_pLayer->GetWeaponEntity());
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f);
	SendWeaponAnim( pWeapon->pev->button );
}

void CCyclerWeaponContext::PrimaryAttack()
{
	CWeaponCycler *pWeapon = static_cast<CWeaponCycler*>(m_pLayer->GetWeaponEntity());
	if (m_bActiveAnims.primary)
	{
		SendWeaponAnim(pWeapon->pev->sequence);
		m_flNextPrimaryAttack = gpGlobals->time + 3.0;
	}
}

void CCyclerWeaponContext::SecondaryAttack()
{
	CWeaponCycler *pWeapon = static_cast<CWeaponCycler*>(m_pLayer->GetWeaponEntity());
	if (m_bActiveAnims.secondary)
	{
		SendWeaponAnim(pWeapon->pev->team);
		m_flNextSecondaryAttack = gpGlobals->time + 3.0;
	}
}

CWeaponCycler::CWeaponCycler()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CCyclerWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CWeaponCycler::Spawn()
{
	// g-cont. this alias need for right slot switching because all selectable items must be preceed with "weapon_" or "item_"
	pev->classname = MAKE_STRING( "weapon_question" );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;

	char basemodel[80], v_path[80], p_path[80], w_path[80];
	CCyclerWeaponContext *pContext = static_cast<CCyclerWeaponContext*>(m_pWeaponContext.get());

	strncpy( basemodel, (char *)STRING(pev->model), sizeof( basemodel ) - 1 ); 

	for( int i = 0; i < (int)strlen( basemodel ); i++ )
	{
		int c = basemodel[i]; 

		if(( c == 'v' || c == 'p' || c == 'w' ) && basemodel[i+1] == '_' )
		{
			basemodel[i] = 'v';
			strcpy( v_path, basemodel );
			basemodel[i] = 'p';
			strcpy( p_path, basemodel );
			basemodel[i] = 'w';
			strcpy( w_path, basemodel );

			// create wepon model pathes
			pContext->m_iPlayerModel = ALLOC_STRING( p_path );
			pContext->m_iWorldModel = ALLOC_STRING( w_path );
			pContext->m_iViewModel = ALLOC_STRING( v_path );
			break;
		}
	}

	if(pContext-> m_iPlayerModel && pContext->m_iWorldModel && pContext->m_iViewModel )
	{
		PRECACHE_MODEL( (char *)STRING(pContext->m_iPlayerModel) );
		PRECACHE_MODEL( (char *)STRING(pContext->m_iWorldModel) );
		PRECACHE_MODEL( (char *)STRING(pContext->m_iViewModel) );

		// set right world model
		pev->model = pContext->m_iWorldModel;

		SET_MODEL( ENT(pev), STRING(pev->model) );
	}
	else
	{
		// fallback to default relationship
		PRECACHE_MODEL( (char *)STRING(pev->model) );
		SET_MODEL( ENT(pev), STRING(pev->model) );

		// setup viewmodel
		pContext->m_iViewModel = pev->model;
    }

	pContext->m_iClip = -1;
	FallInit();
}

void CWeaponCycler::KeyValue( KeyValueData *pkvd )
{
	CCyclerWeaponContext *pContext = static_cast<CCyclerWeaponContext*>(m_pWeaponContext.get());
	if (FStrEq(pkvd->szKeyName, "deploy"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holster"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "primary"))
	{
		pev->sequence = atoi(pkvd->szValue);
		pContext->m_bActiveAnims.primary = strlen(pkvd->szKeyName) > 0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "secondary"))
	{
		pev->team = atoi(pkvd->szValue);
		pContext->m_bActiveAnims.secondary = strlen(pkvd->szKeyName) > 0;
		pkvd->fHandled = TRUE;
	}
	else CBasePlayerWeapon::KeyValue( pkvd );
}
