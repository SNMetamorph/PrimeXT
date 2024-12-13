#pragma once
#include "weapon_logic_funcs.h"
#include "vector.h"
#include "item_info.h"

class CBasePlayer;

class CBaseWeaponLogic
{
public:
	CBaseWeaponLogic(IWeaponLogicFuncs *funcs);
	virtual ~CBaseWeaponLogic();
	void ItemPostFrame();

	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }				// do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void Reload( void ) { return; }						// do "+RELOAD"
	virtual void WeaponIdle( void ) { return; }					// called when no buttons pressed

	virtual BOOL ShouldWeaponIdle( void ) {return FALSE; };
	virtual BOOL CanDeploy( void );
	virtual BOOL Deploy( ) { return TRUE; };		// returns is deploy was successful	 
	virtual BOOL CanHolster( void ) { return TRUE; };		// can this weapon be put away right nxow?
	virtual void Holster(void);
	virtual BOOL IsUseable( void );
	virtual BOOL UseDecrement( void ) { return FALSE; };
	
	virtual int GetItemInfo(ItemInfo *p) { return 0; };	// returns 0 if struct not filled out
	virtual int	PrimaryAmmoIndex(); 
	virtual int	SecondaryAmmoIndex(); 

	virtual int iItemSlot(void);
	virtual int	iItemPosition(void);
	virtual const char *pszAmmo1(void);
	virtual int iMaxAmmo1(void);
	virtual const char *pszAmmo2(void);
	virtual int	iMaxAmmo2(void);
	virtual const char *pszName(void);
	virtual int	iMaxClip(void);
	virtual int	iWeight(void);
	virtual	int iFlags(void);

	BOOL DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal = 0, int body = 0 );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );
	void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations
	BOOL PlayEmptySound( void );
	void ResetEmptySound( void );

	int	m_iId;				// WEAPON_???
	int m_iPlayEmptySound;
	int m_fFireOnEmpty;		// True when the gun is empty and the player is still holding down the attack key(s)
	float m_flPumpTime;
	int	m_fInSpecialReload;			// Are we in the middle of a reload for the shotguns
	float m_flNextPrimaryAttack;		// soonest time ItemPostFrame will call PrimaryAttack
	float m_flNextSecondaryAttack;		// soonest time ItemPostFrame will call SecondaryAttack
	float m_flTimeWeaponIdle;			// soonest time ItemPostFrame will call WeaponIdle
	int	m_iPrimaryAmmoType;			// "primary" ammo index into players m_rgAmmo[]
	int	m_iSecondaryAmmoType;		// "secondary" ammo index into players m_rgAmmo[]
	int	m_iClip;				// number of shots left in the primary weapon clip, -1 it not used
	int	m_iClientClip;			// the last version of m_iClip sent to hud dll
	int	m_iClientWeaponState;		// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int	m_fInReload;			// Are we in the middle of a reload;
	int	m_iDefaultAmmo;// how much ammo you get when you pick up this weapon as placed by a level designer.
	CBasePlayer	*m_pPlayer;
	IWeaponLogicFuncs *m_pFuncs;
};

