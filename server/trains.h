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
#ifndef TRAINS_H
#define TRAINS_H

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL	0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE	0x0008
#define SF_TRACKTRAIN_NO_FIRE_ON_PASS	0x0010

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN		0x00000008
#define SF_PATH_TELEPORT		0x00000010
#define SF_PATH_ALTERNATE		0x00008000

// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG	0x001
#define SF_CORNER_TELEPORT		0x002
#define SF_CORNER_FIREONCE		0x004

//#define PATH_SPARKLE_DEBUG		// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
	DECLARE_CLASS( CPathTrack, CPointEntity );
public:
	void		Spawn( void );
	void		Activate( void );
	void		KeyValue( KeyValueData* pkvd);
	
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CPathTrack	*ValidPath( CPathTrack *ppath, int testFlag );		// Returns ppath if enabled, NULL otherwise
	void		Project( CPathTrack *pstart, CPathTrack *pend, Vector *origin, float dist );
	Vector		GetOrientation( bool bForwardDir );
	CPathTrack	*GetNextInDir( bool bForward );

	static CPathTrack *Instance( edict_t *pent );

	CPathTrack	*LookAhead( Vector *origin, float dist, int move );
	CPathTrack	*Nearest( Vector origin );

	CPathTrack	*GetNext( void );
	CPathTrack	*GetPrevious( void );

	DECLARE_DATADESC();

#ifdef PATH_SPARKLE_DEBUG
	void 		Sparkle(void);
#endif
	float		m_length;
	string_t		m_altName;
	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;

	string_t		m_iszFireFow;
	string_t		m_iszFireRev;
};

class CFuncTrain;
class CBaseTrainDoor;

class CFuncTrackTrain : public CBaseDelay
{
	DECLARE_CLASS( CFuncTrackTrain, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void Next( void );
	void Find( void );
	void NearestPath( void );
	void DeadEnd( void );

	void SetTrainDoor( CBaseTrainDoor *pDoor ) { m_pDoor = pDoor; }
	void SetTrack( CPathTrack *track ) { m_ppath = track->Nearest( GetLocalOrigin( )); }
	void SetControls( CBaseEntity *pControls );
	BOOL OnControls( CBaseEntity *pTest );

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent );
	void TeleportToPathTrack( CPathTrack *pTeleport );

	DECLARE_DATADESC();

	virtual STATE	GetState( void ) { return (pev->speed != 0) ? STATE_ON : STATE_OFF; }
	virtual int	ObjectCaps( void )
	{
		return (CBaseDelay :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DIRECTIONAL_USE | FCAP_HOLD_ANGLES;
	}
	virtual void	OverrideReset( void );

	CBaseTrainDoor	*m_pDoor;
	CPathTrack	*m_ppath;
	float		m_length;
	float		m_height;
	float		m_speed;
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
};

typedef enum
{
	TD_CLOSED,
	TD_SHIFT_UP,
	TD_SLIDING_UP,
	TD_OPENED,
	TD_SLIDING_DOWN,
	TD_SHIFT_DOWN
} TRAINDOOR_STATE;

#define SF_TRAINDOOR_INVERSE			BIT( 0 )
#define SF_TRAINDOOR_OPEN_IN_MOVING		BIT( 1 )
#define SF_TRAINDOOR_ONOFF_MODE		BIT( 2 )

class CBaseTrainDoor : public CBaseToggle
{
	DECLARE_CLASS( CBaseTrainDoor, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return ((CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; }
	virtual bool IsDoorControl( void ) { return !FBitSet( pev->spawnflags, SF_TRAINDOOR_OPEN_IN_MOVING ) && GetState() != STATE_OFF; }

	DECLARE_DATADESC();

	// local functions
	void FindTrain( void );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorSlideUp( void );
	void DoorSlideDown( void );
	void DoorSlideWait( void );		// wait before sliding
	void DoorHitBottom( void );
	void ActivateTrain( void );
	
	int		m_iMoveSnd;	// sound a door makes while moving
	int		m_iStopSnd;	// sound a door makes when it stops

	CFuncTrackTrain	*m_pTrain;	// my train pointer
	Vector		m_vecOldAngles;

	TRAINDOOR_STATE	door_state;
	virtual void	OverrideReset( void );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState ( void );
};

#endif//TRAINS_H
