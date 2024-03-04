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

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL	0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE	0x0008
#define SF_TRACKTRAIN_NO_FIRE_ON_PASS	0x0010
#define SF_TRACKTRAIN_UNBLOCKABLE	0x0020
#define SF_TRACKTRAIN_SPEEDBASED_PITCH	0x0040

class CFuncTrain;
class CBaseTrainDoor;

class CFuncTrackTrain : public CBaseDelay
{
	DECLARE_CLASS( CFuncTrackTrain, CBaseDelay );
public:
	CFuncTrackTrain();

	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void Next( void );
	void Find( void );
	void NearestPath( void );
	void DeadEnd( void );

	void Stop( void );

	bool IsDirForward() { return ( m_dir == 1 ); }
	void SetDirForward( bool bForward );
	void SetSpeed( float flSpeed, float flAccel = 0.0f );
	void SetSpeedExternal( float flSpeed );

	void SetTrainDoor( CBaseTrainDoor *pDoor ) { m_pDoor = pDoor; }
	void SetTrack( CPathTrack *track ) { m_ppath = track->Nearest( GetLocalOrigin( )); }
	void SetControls( CBaseEntity *pControls );
	BOOL OnControls( CBaseEntity *pTest );

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent );
	void TeleportToPathTrack( CPathTrack *pTeleport );
	void UpdateTrainVelocity( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateTrainOrientation( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationAtPathTracks( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationBlend( int eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval );
	void DoUpdateOrientation( const Vector &curAngles, const Vector &angles, float flInterval );
	float GetSpeed( void ) { return m_flDesiredSpeed; }
	float GetMaxSpeed( void ) { return m_maxSpeed; }
	void SetMaxSpeed( float fNewSpeed ) { m_maxSpeed = fNewSpeed; }

	DECLARE_DATADESC();

	virtual STATE	GetState( void ) { return (pev->speed != 0) ? STATE_ON : STATE_OFF; }
	virtual int	ObjectCaps( void )
	{
		return (CBaseDelay :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DIRECTIONAL_USE | FCAP_HOLD_ANGLES;
	}

	void		ArriveAtNode( CPathTrack *pNode );
	virtual void	OverrideReset( void );
	void		UpdateOnRemove( void );

	CBaseTrainDoor	*m_pDoor;
	CPathTrack	*m_ppath;
	CBaseEntity	*m_pSpeedControl;
	float		m_length;
	float		m_height;
	float		m_maxSpeed;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	Vector		m_controlOrigin;
	int		m_soundPlaying;
	int		m_sounds;
	int		m_soundStart;
	int		m_soundStop;
	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;
	float		m_dir;
	int		m_eOrientationType;
	int		m_eVelocityType;

	float		m_flDesiredSpeed;
	float		m_flAccelSpeed;
	float		m_flReachedDist;
};
