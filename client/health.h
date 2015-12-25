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

#ifndef HEALTH_H
#define HEALTH_H

#define DMG_IMAGE_LIFE		2	// seconds that image is up

#define DMG_IMAGE_POISON		0
#define DMG_IMAGE_ACID		1
#define DMG_IMAGE_COLD		2
#define DMG_IMAGE_DROWN		3
#define DMG_IMAGE_BURN		4
#define DMG_IMAGE_NERVE		5
#define DMG_IMAGE_RAD		6
#define DMG_IMAGE_SHOCK		7
#define DMG_IMAGE_NUCLEAR		8
#define NUM_DMG_TYPES		9

#include <dmg_types.h>

typedef struct
{
	float	fExpire;
	float	fBaseline;
	int	x, y;
} DAMAGE_IMAGE;
	
//
//-----------------------------------------------------
//
class CHudHealth: public CHudBase
{
public:
	virtual int Init( void );
	virtual int VidInit( void );
	virtual int Draw(float fTime);
	virtual void Reset( void );
	int MsgFunc_Health( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_Damage( const char *pszName, int iSize, void *pbuf );
	float m_fAttackFront, m_fAttackRear, m_fAttackLeft, m_fAttackRight;
	void GetPainColor( int &r, int &g, int &b );

	float	m_fFade;
	int	m_iHealth;
	int	m_HUD_dmg_bio;
	int	m_HUD_cross;
private:
	HSPRITE	m_hSprite;
	HSPRITE	m_hDamage;
	int	m_bitsDamage;

	DAMAGE_IMAGE m_dmg[NUM_DMG_TYPES];

	int DrawPain( float fTime );
	int DrawDamage( float fTime );
	void CalcDamageDirection( Vector vecFrom );
	void UpdateTiles( float fTime, long bits );
};	

#endif//HEALTH_H
