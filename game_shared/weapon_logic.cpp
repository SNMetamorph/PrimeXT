#include "weapon_logic.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
	return ( attack_time <= curtime ) ? TRUE : FALSE;
}

CBaseWeaponLogic::CBaseWeaponLogic(IWeaponLogicFuncs *funcs) :
	m_pFuncs(funcs),
	m_pPlayer(nullptr)
{
}

CBaseWeaponLogic::~CBaseWeaponLogic()
{
	if (m_pFuncs) {
		delete m_pFuncs;
	}
}

void CBaseWeaponLogic::ItemPostFrame()
{
	if ((m_fInReload) && ( m_pPlayer->m_flNextAttack <= gpGlobals->time ))
	{
		// complete the reload. 
		int j = Q_min( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

		m_fInReload = FALSE;
	}

	if ((m_pPlayer->pev->button & IN_ATTACK2) && CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( pszAmmo2() && !m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
		{
			m_fFireOnEmpty = TRUE;
		}

		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) && CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
		{
			m_fFireOnEmpty = TRUE;
		}

		PrimaryAttack();
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_pPlayer->pev->button & (IN_ATTACK|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

		if ( !IsUseable() && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) ) 
		{
			// weapon isn't useable, switch.
			if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && m_pFuncs->GetNextBestWeapon() )
			{
				m_flNextPrimaryAttack = ( UseDecrement() ? 0.0 : gpGlobals->time ) + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}
	
	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

void CBaseWeaponLogic::Holster( void )
{ 
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
BOOL CBaseWeaponLogic :: IsUseable( void )
{
	if ( m_iClip <= 0 )
	{
		if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 && iMaxAmmo1() != -1 )			
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type. 
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CBaseWeaponLogic :: CanDeploy( void )
{
	BOOL bHasAmmo = 0;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CBaseWeaponLogic :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal /* = 0 */, int body )
{
	if (!CanDeploy( ))
		return FALSE;

	m_pPlayer->pev->viewmodel = MAKE_STRING(szViewModel);
	m_pPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	SendWeaponAnim( iAnim, skiplocal, body );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0;

	return TRUE;
}

BOOL CBaseWeaponLogic :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return FALSE;

	int j = Q_min(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

	if (j == 0)
		return FALSE;

	m_pPlayer->m_flNextAttack = gpGlobals->time + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = gpGlobals->time + 3;
	return TRUE;
}

void CBaseWeaponLogic::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	if ( UseDecrement() )
		skiplocal = 1;
	else
		skiplocal = 0;

	m_pPlayer->pev->weaponanim = iAnim;

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev );
		WRITE_BYTE( iAnim );						// sequence number
		WRITE_BYTE( m_pFuncs->GetWeaponBodygroup() );					// weaponmodel bodygroup.
	MESSAGE_END();
}

BOOL CBaseWeaponLogic :: PlayEmptySound( void )
{
	if (m_iPlayEmptySound)
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
		m_iPlayEmptySound = 0;
		return 0;
	}
	return 0;
}

void CBaseWeaponLogic :: ResetEmptySound( void )
{
	m_iPlayEmptySound = 1;
}

int CBaseWeaponLogic::PrimaryAmmoIndex( void )
{
	return m_iPrimaryAmmoType;
}

int CBaseWeaponLogic::SecondaryAmmoIndex( void )
{
	return -1;
}

int CBaseWeaponLogic::iItemSlot(void)				{ return 0; }	// return 0 to MAX_ITEMS_SLOTS, used in hud
int	CBaseWeaponLogic::iItemPosition( void ) 		{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iPosition; }
const char *CBaseWeaponLogic::pszAmmo1( void ) 		{ return CBasePlayerItem::ItemInfoArray[ m_iId ].pszAmmo1; }
int CBaseWeaponLogic::iMaxAmmo1( void ) 			{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iMaxAmmo1; }
const char *CBaseWeaponLogic::pszAmmo2( void ) 		{ return CBasePlayerItem::ItemInfoArray[ m_iId ].pszAmmo2; }
int	CBaseWeaponLogic::iMaxAmmo2( void ) 			{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iMaxAmmo2; }
const char *CBaseWeaponLogic::pszName( void ) 		{ return CBasePlayerItem::ItemInfoArray[ m_iId ].pszName; }
int	CBaseWeaponLogic::iMaxClip( void ) 				{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iMaxClip; }
int	CBaseWeaponLogic::iWeight( void ) 				{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iWeight; }
int CBaseWeaponLogic::iFlags( void ) 				{ return CBasePlayerItem::ItemInfoArray[ m_iId ].iFlags; }
