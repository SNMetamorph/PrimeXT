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

#pragma once
#include "plats.h"
#include "func_platrot.h"

// ----------------------------------------------------------------------------
//
// Track changer / Train elevator
//
// ----------------------------------------------------------------------------

#define SF_TRACK_ACTIVATETRAIN	0x00000001
#define SF_TRACK_RELINK		0x00000002
#define SF_TRACK_ROTMOVE		0x00000004
#define SF_TRACK_STARTBOTTOM		0x00000008
#define SF_TRACK_DONT_MOVE		0x00000010
#define SF_TRACK_ONOFF_MODE		0x00000020

//
// This entity is a rotating/moving platform that will carry a train to a new track.
// It must be larger in X-Y planar area than the train, since it must contain the
// train within these dimensions in order to operate when the train is near it.
//

typedef enum { TRAIN_SAFE, TRAIN_BLOCKING, TRAIN_FOLLOWING } TRAIN_CODE;

class CFuncTrackChange : public CFuncPlatRot
{
	DECLARE_CLASS( CFuncTrackChange, CFuncPlatRot );
public:
	void Spawn( void );
	void Precache( void );

	virtual void	GoUp( void );
	virtual void	GoDown( void );

	void		KeyValue( KeyValueData* pkvd );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		Find( void );
	TRAIN_CODE	EvaluateTrain( CPathTrack *pcurrent );
	void		UpdateTrain( Vector &dest );
	virtual void	HitBottom( void );
	virtual void	HitTop( void );
	void		Touch( CBaseEntity *pOther ) {};
	virtual void	UpdateAutoTargets( int toggleState );
	virtual BOOL	IsTogglePlat( void ) { return TRUE; }

	void		DisableUse( void ) { m_use = 0; }
	void		EnableUse( void ) { m_use = 1; }
	int		UseEnabled( void ) { return m_use; }

	DECLARE_DATADESC();

	virtual void	OverrideReset( void );

	CPathTrack	*m_trackTop;
	CPathTrack	*m_trackBottom;

	CFuncTrackTrain	*m_train;

	string_t		m_trackTopName;
	string_t		m_trackBottomName;
	string_t		m_trainName;
	TRAIN_CODE	m_code;
	float		m_flRadius;	// custom radius to search train
	int		m_targetState;
	int		m_use;
};
