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

Class Hierachy

CBaseEntity
	CBaseDelay
		CBaseToggle
			CBaseItem
			CBaseMonster
				CBaseCycler
				CBasePlayer
				CBaseGroup
*/
#pragma once
#include "util.h"

#define	MAX_PATH_SIZE	10 // max number of nodes available for a path.

// These are caps bits to indicate what an object's capabilities (currently used for save/restore and level transitions)
#define FCAP_SET_MOVEDIR		0x00000001	// convert initial angles into direction (doors used)
#define FCAP_ACROSS_TRANSITION	0x00000002	// should transfer between transitions
#define FCAP_MUST_SPAWN		0x00000004	// Spawn after restore
#define FCAP_IMPULSE_USE		0x00000008	// can be used by the player
#define FCAP_CONTINUOUS_USE		0x00000010	// can be used by the player
#define FCAP_ONOFF_USE		0x00000020	// can be used by the player
#define FCAP_DIRECTIONAL_USE		0x00000040	// Player sends +/- 1 when using (currently only tracktrains)
#define FCAP_FORCE_TRANSITION		0x00000080	// ALWAYS goes across transitions
#define FCAP_ONLYDIRECT_USE		0x00000100	// Can't use this entity through a wall.
#define FCAP_HOLDABLE_ITEM		0x00000200	// Item that can be picked up by player
#define FCAP_HOLD_ANGLES		0x00000400	// hold angles at spawn to let parent system right attach the child
#define FCAP_NOT_MASTER		0x00000800	// this entity can't be used as master directly (only through multi_watcher)
#define FCAP_IGNORE_PARENT		0x00001000	// this entity won't to attached

// UNDONE: This will ignore transition volumes (trigger_transition), but not the PVS!!!

#define FCAP_DONT_SAVE		0x80000000	// Don't save this

// goes into pEntity->m_iFlags
#define MF_POINTENTITY		BIT( 0 )		// not solid point entity (e.g. env_sprite, env_counter, etc)
#define MF_TRIGGER			BIT( 1 )		// mark entity as invisible solid for debug purposes
#define MF_GROUNDMOVE		BIT( 2 )		// my ground is pusher and it moving
#define MF_LADDER			BIT( 3 )		// ladders is sended to client for predict reasons
#define MF_TEMP_PARENT		BIT( 4 )		// temporare parent on teleport

#pragma once
#include "saverestore.h"
#include "schedule.h"

#ifndef MONSTEREVENT_H
#include "monsterevent.h"
#endif

#include "meshdesc.h"
#include "physic.h"

#include "exportdef.h"

// C functions for external declarations that call the appropriate C++ methods

extern "C" EXPORT int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
extern "C" EXPORT int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
extern "C" EXPORT int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
extern "C" EXPORT int Server_GetPhysicsInterface( int iVersion, server_physics_api_t *pfuncsFromEngine, physics_interface_t *pFunctionTable );
extern "C" EXPORT int Server_GetBlendingInterface( int, struct sv_blending_interface_s**, struct server_studio_api_s*, float (*t)[3][4], float (*b)[128][3][4] );

extern int DispatchSpawn( edict_t *pent );
extern void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd );
extern void DispatchTouch( edict_t *pentTouched, edict_t *pentOther );
extern void DispatchUse( edict_t *pentUsed, edict_t *pentOther );
extern void DispatchThink( edict_t *pent );
extern void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther );
extern void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData );
extern int  DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
extern void DispatchObjectCollsionBox( edict_t *pent );
extern void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, ENGTYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, ENGTYPEDESCRIPTION *pFields, int fieldCount );
extern void DispatchCreateEntitiesInRestoreList( SAVERESTOREDATA *pSaveData, int levelMask, qboolean create_world );
extern void SaveGlobalState( SAVERESTOREDATA *pSaveData );
extern void RestoreGlobalState( SAVERESTOREDATA *pSaveData );
extern void ResetGlobalState( void );

// XashXT physics interface
extern int DispatchCreateEntity( edict_t *pent, const char *szName );
extern int DispatchPhysicsEntity( edict_t *pEdict );
extern int DispatchSpawnEntities( const char *mapname, char *entities );
extern void DispatchUpdatePlayerBaseVelocity( edict_t *pEdict );

// For CLASSIFY
#define CLASS_NONE			0
#define CLASS_MACHINE		1
#define CLASS_PLAYER		2
#define CLASS_HUMAN_PASSIVE		3
#define CLASS_HUMAN_MILITARY		4
#define CLASS_ALIEN_MILITARY		5
#define CLASS_ALIEN_PASSIVE		6
#define CLASS_ALIEN_MONSTER		7
#define CLASS_ALIEN_PREY		8
#define CLASS_ALIEN_PREDATOR		9
#define CLASS_INSECT		10
#define CLASS_PLAYER_ALLY		11
#define CLASS_PLAYER_BIOWEAPON	12 // hornets and snarks.launched by players
#define CLASS_ALIEN_BIOWEAPON		13 // hornets and snarks.launched by the alien menace
#define CLASS_BARNACLE		99 // special because no one pays attention to it, and it eats a wide cross-section of creatures.

#define ACTOR_INVALID		0	// not a physic entity anyway
#define ACTOR_DYNAMIC		1	// as default. dynamic actor with convex hull
#define ACTOR_KINEMATIC		2	// kinematic actor (mover with SOLID_BSP)
#define ACTOR_CHARACTER		3	// player or monster physics shadow
#define ACTOR_STATIC		4	// static actor (env_static)
#define ACTOR_VEHICLE		5	// complex body (vehicle)
#define ACTOR_TRIGGER		6	// used for func_water

#define PARENT_FROZEN_POS_X		BIT( 1 )	// compatible with PhysX flags NX_BF_FROZEN_
#define PARENT_FROZEN_POS_Y		BIT( 2 )
#define PARENT_FROZEN_POS_Z		BIT( 3 )
#define PARENT_FROZEN_ROT_X		BIT( 4 )
#define PARENT_FROZEN_ROT_Y		BIT( 5 )
#define PARENT_FROZEN_ROT_Z		BIT( 6 )

class EHANDLE;
class CBaseEntity;
class CBaseMonster;
class CBasePlayerItem;
class CSquadMonster;
class CStudioBoneSetup;

#include "ehandle.h"

#define SF_NORESPAWN		( 1 << 30 ) // !!!set this bit on guns and stuff that should never respawn.

//
// Base Entity.  All entity types derive from this
//
class CBaseEntity 
{
	DECLARE_CLASS_NOBASE( CBaseEntity );
public:
	// Constructor.  Set engine to use C/C++ callback functions
	// pointers to engine data
	// g-cont. never move this pointer. It must be first in all cases!!!
	// so conversion CBaseEntity<->entvars_t will be working correctly
	entvars_t		*pev;		// Don't need to save/restore this pointer, the engine resets it

	DECLARE_DATADESC();

	// Always keep this virtual destructor, so derived classes can be properly destructed
	virtual ~CBaseEntity() {}

	// path corners
	CBaseEntity	*m_pGoalEnt;	// path corner we are heading towards
	CBaseEntity	*m_pLink;		// used for temporary link-list operations. 

	int		m_iOldSolid;	// for temporare change solidity
	int		m_iOldMoveType;	// for temporare change physics
	int		m_iFlags;		// entity misc flags

	// trace filter
	CBaseEntity	*m_pRootParent;	// don't save\restore this. It's temporary handler
	int		m_iParentFilter;	// we are moving by pusher
	int		m_iPushableFilter;	// we are moving by pushable
	int		m_iTeleportFilter;	// we are teleporting

	// local time the movement has ended
	float		m_flMoveDoneTime;
	float		m_flGaitYaw;	// player stuff

	// physics stuff
	unsigned int	m_iPhysicsFrame;	// to avoid executing one entity twice per frame

	// A counter to help quickly build a list of potentially pushed objects for physics
	int		m_iPushEnumCount;
	int		m_iGroundEnumCount;
	int		m_iStyle;		// generic lightstyles for any brush entity

	BOOL		m_fPicked;	// a bit who indicated if the object is already picked by another player
	BOOL		m_fSetAngles;	// some mappers ignore to set angles. Don't save restore this
	Vector		m_vecTempAngles;	// temporary angles stored here (doesn't need to save restore)

	Vector		m_vecEndPos;	// abs vector for beam end

	Vector		m_vecOrigin;	// this is local origin
	Vector		m_vecAngles;	// this is local angles
	Vector		m_vecVelocity;	// this is local velocity
	Vector		m_vecAvelocity;	// this is local avelocity

	// absolute origin and angles stay in v.origin and v.angles for now

	// NOTE: Setting the abs origin or angles will cause the local origin + angles to be set also
	void		SetAbsOrigin( const Vector& origin );
 	const Vector&	GetAbsOrigin( void ) const;
	void		SetAbsAngles( const Vector& angles );
	const Vector&	GetAbsAngles( void ) const;

	// Origin and angles in local space ( relative to parent )
	// NOTE: Setting the local origin or angles will cause the abs origin + angles to be set also
	void		SetLocalOrigin( const Vector& origin );
	const Vector&	GetLocalOrigin( void ) const;
	void		SetLocalAngles( const Vector& angles );
	const Vector&	GetLocalAngles( void ) const;

	// NOTE: Setting the abs velocity in either space will cause a recomputation
	// in the other space, so setting the abs velocity will also set the local vel
	void		SetLocalVelocity( const Vector &vecVelocity );
	const Vector&	GetLocalVelocity( void ) const;
	void		SetLocalAvelocity( const Vector &vecAvelocity );
	const Vector&	GetLocalAvelocity( void ) const;

	void		SetAbsVelocity( const Vector &vecVelocity );
	const Vector&	GetAbsVelocity( void ) const;
	void		SetAbsAvelocity( const Vector &vecAvelocity );
	const Vector&	GetAbsAvelocity( void ) const;

	const Vector&	GetBaseVelocity( void ) const { return pev->basevelocity; }
	Vector		GetScale() const;
	void		ApplyLocalVelocityImpulse( const Vector &vecImpulse );
	void		ApplyAbsVelocityImpulse( const Vector &vecImpulse );

	void		CalcAbsolutePosition();
	void		CalcAbsoluteVelocity();
	void		CalcAbsoluteAvelocity();

	// keep as ehandle because parent may through level
	EHANDLE		m_hParent;	// pointer to parent entity
	EHANDLE		m_hChild;		// pointer to children (may be NULL)
	EHANDLE		m_hNextChild;	// link to next chlidren in chain
	string_t		m_iParent;	// name of parent
	int		m_iParentFlags;	// flags that lock some axis or dimensions from parent

	// local entity matrix position
	matrix4x4		m_local;

	// PhysX description
	void		*m_pUserData;	// pointer to rigid body. may be NULL
	unsigned char	m_iActorType;	// static, kinetic or dynamic
	uint32_t		m_iActorFlags;	// NxActor->flags
	uint32_t		m_iBodyFlags;	// NxBodyDesc->flags
	uint32_t		m_iFilterData[4];	// NxActor->group
	float		m_flBodyMass;	// NxActor->mass
	BOOL		m_fFreezed;	// is body sleeps?
	bool		m_isChaining;
	Vector		m_vecOldPosition;	// don't save this
	Vector		m_vecOldBounds;

	float		m_flShowHostile;	// for sprite monsters wake-up

	float		m_flPoseParameter[MAXSTUDIOPOSEPARAM];

	matrix4x4		GetParentToWorldTransform( void );

	// returns the entity-to-world transform
	matrix4x4		&EntityToWorldTransform();
	const matrix4x4	&EntityToWorldTransform() const;

	// to prevent rotate brushes without 'origin' brush
	void		CheckAngles( void );

	// link operations
	void		UnlinkFromParent( void );
	void		UnlinkAllChildren( void );
	void		UnlinkChild( CBaseEntity *pParent );
	void		LinkChild( CBaseEntity *pParent );
	CBaseEntity*	GetRootParent( void );

	// Invalidates the abs state of the entity and all children
	// Specify the second flag if you want additional flags applied to all children
	void		InvalidatePhysicsState( int flags, int childflags = 0 );
	void		SetChaining( bool chaining ) { m_isChaining = chaining; }
	BOOL		ShouldToggle( USE_TYPE useType );

	const char*	GetClassname() const { return STRING( pev->classname ); } 
	const char*	GetGlobalname() const { return STRING( pev->globalname ); }
	const char*	GetTargetname() const { return STRING( pev->targetname ); }
	const char*	GetTarget() const { return STRING( pev->target ); }
	const char*	GetMessage() const { return STRING( pev->message ); }
	const char*	GetNetname() const { return STRING( pev->netname ); }
	const char*	GetModel() const { return STRING( pev->model ); }
	void		SetModel( const char *model );
	void		ReportInfo( void );

	const char*	GetDebugName()
	{
		if( this == nullptr || pev == nullptr)
			return "null";

		if( pev->targetname != NULL_STRING ) 
			return GetTargetname();
		return GetClassname();
	}

	void		SetClassname( const char *pszClassName ) { pev->classname = MAKE_STRING( pszClassName ); }

	float		GetLocalTime( void ) const;
	void		IncrementLocalTime( float flTimeDelta );
	float		GetMoveDoneTime( ) const;
	void		SetMoveDoneTime( float flTime );
		
	void		SetParent( string_t newParent, CBaseEntity *pActivator );
	
	// Set the movement parent. Your local origin and angles will become relative to this parent.
	// If iAttachment is a valid attachment on the parent, then your local origin and angles 
	// are relative to the attachment on this entity.
	void		SetParent( const CBaseEntity* pNewParent, int iAttachment = 0 );
	void		SetParent ( int m_iNewParent, int m_iAttachment = 0 );
	void		SetParent ( CBaseEntity* pParent, int m_iAttachment = 0 );
	BOOL		HasAttachment( void );

	void		ResetPoseParameters( void );
	int		LookupPoseParameter( const char *szName );
	void		SetPoseParameter( int iParameter, float flValue );
	float		GetPoseParameter( int iParameter );

	// initialization functions
	virtual void	Spawn( void ) { }
	virtual void	Precache( void ) { }
	virtual void	KeyValue( KeyValueData *pkvd )
	{
		// get support for spirit field too
		if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
		{
			m_iParent = ALLOC_STRING(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "parentflags" ))
		{
			m_iParentFlags = Q_atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "style" ))
		{
			m_iStyle = Q_atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "reflection" ))
		{
			switch( Q_atoi( pkvd->szValue ))
			{
			case 1: pev->effects |= EF_NOREFLECT; break;
			case 2: pev->effects |= EF_REFLECTONLY; break;
			}
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq(pkvd->szKeyName, "vlight_cache"))
		{
			pev->iuser3 = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else pkvd->fHandled = FALSE;
	}

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	virtual int	ObjectCaps( void ) { return FCAP_ACROSS_TRANSITION; }
	virtual void	Activate( void ) {}
	virtual void	OnChangeLevel( void ) {}
	virtual void	OnTeleport( void ) {}
	virtual void	PortalSleep( float seconds ) {}
          virtual void	StartMessage( CBasePlayer *pPlayer ) {}
	virtual float	GetPosition( void ) { return 0.0f; }
	virtual void	OnChangeParent( void ) {}
	virtual void	OnClearParent( void ) {}
	virtual void	OnRemove( void ) {}

	// Setup the object->object collision box (pev->mins / pev->maxs is the object->world collision box)
	virtual void	SetObjectCollisionBox( void );
	void		EntityAABBToWorldAABB( const Vector &entityMins, const Vector &entityMaxs, Vector &pWorldMins, Vector &pWorldMaxs ) const;
	void		WorldSpaceAABB( Vector &pWorldMins, Vector &pWorldMaxs ) const;
	void		CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const;
	const Vector	&CollisionToWorldSpace( const Vector &in, Vector *pResult ) const; 
	const Vector	&WorldToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Classify - returns the type of group (i.e, "houndeye", or "human military" so that monsters with different classnames
	// still realize that they are teammates. (overridden for monsters that form groups)
	virtual int Classify ( void ) { return CLASS_NONE; };
	virtual void DeathNotice ( entvars_t *pevChild ) { } // monster maker children use this to tell the monster maker that they have died.
	virtual BOOL IsRigidBody( void ) { return (m_iActorType == ACTOR_DYNAMIC); } 

	void SetMoveDir( const Vector& v ) { pev->movedir = v; }
	void SetBaseVelocity( const Vector& v ) { pev->basevelocity = v; }

	virtual void	SetNextThink( float delay );
	void		DontThink( void );

	// global concept of "entities with states", so that state_watchers and
	// mastership (mastery? masterhood?) can work universally.
	// g-cont. thanks Laurie Cheers for nice idea
	virtual STATE GetState ( void ) { return STATE_OFF; };

	// For team-specific doors in multiplayer, etc: a master's state depends on who wants to know.
	virtual STATE GetState ( CBaseEntity* pEnt ) { return GetState(); };

	virtual void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int	TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int	TakeHealth( float flHealth, int bitsDamageType );
	virtual int	TakeArmor( float flArmor, int suit = 0 );
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void	TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual BOOL	IsTriggered( CBaseEntity *pActivator ) {return TRUE;}
	virtual CBaseMonster *MyMonsterPointer( void ) { return NULL;}
	virtual CSquadMonster *MySquadMonsterPointer( void ) { return NULL;}
	virtual CBaseEntity *GetVehicleDriver( void ) { return NULL; }
	virtual int	GetToggleState( void ) { return TS_AT_TOP; }
	virtual void	AddPoints( int score, BOOL bAllowNegativeScore ) {}
	virtual void	AddPointsToTeam( int score, BOOL bAllowNegativeScore ) {}
	virtual BOOL	AddPlayerItem( CBasePlayerItem *pItem ) { return 0; }
	virtual BOOL	RemovePlayerItem( CBasePlayerItem *pItem ) { return 0; }
	virtual int 	GiveAmmo( int iAmount, char *szName, int iMax ) { return -1; };
	virtual float	GetDelay( void ) { return 0; }
	virtual int	IsMoving( void ) { return GetAbsVelocity() != g_vecZero; }
	virtual void	OverrideReset( void ) {}
	virtual void	TransferReset( void ) {}
	virtual const char *DamageDecal( int bitsDamageType );
	// This is ONLY used by the node graph to test movement through a door
	virtual void	SetToggleState( int state ) {}
	virtual void	StartSneaking( void ) {}
	virtual void	StopSneaking( void ) {}
	virtual BOOL	OnControls( CBaseEntity *pTest ) { return FALSE; }
	virtual BOOL	IsSneaking( void ) { return FALSE; }
	virtual BOOL	IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL	IsBSPModel( void ) { return UTIL_GetModelType( pev->modelindex ) == mod_brush; }
	virtual BOOL	IsCustomModel( void ) { return pev->solid == SOLID_CUSTOM; }
	virtual BOOL	ReflectGauss( void ) { return (( IsBSPModel() || IsCustomModel()) && !pev->takedamage ); }
	virtual BOOL	HasTarget( string_t targetname ) { return FStrEq(STRING(targetname), STRING(pev->targetname) ); }
	virtual BOOL	IsInWorld( BOOL checkVelocity = TRUE );
	virtual BOOL	IsPlayer( void ) { return FALSE; }
	virtual BOOL	IsNetClient( void ) { return FALSE; }
	virtual BOOL	IsMonster( void ) { return (pev->flags & FL_MONSTER ? TRUE : FALSE); }
	virtual BOOL	IsPushable( void ) { return FALSE; }
	virtual BOOL	IsProjectile( void ) { return FALSE; }
	virtual BOOL	IsFuncScreen( void ) { return FALSE; }
	virtual BOOL	IsPortal( void ) { return FALSE; }
	virtual BOOL	IsTank( void ) { return FALSE; }
	virtual BOOL	IsMover( void ) { return FALSE; }
	virtual BOOL	IsBreakable( void ) { return FALSE; }
	virtual const char	*TeamID( void ) { return ""; }

	virtual CBaseEntity *GetNextTarget( void );
	
	// fundamental callbacks
	void (CBaseEntity ::*m_pfnThink)(void);
	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnMoveDone)( void );

	virtual void Think( void ) { if (m_pfnThink) (this->*m_pfnThink)(); };

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if (m_pfnUse) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
	}

	virtual void Touch( CBaseEntity *pOther )
	{
		if( m_pfnTouch )
			(this->*m_pfnTouch)( pOther );

		// forward the blocked event to our parent, if any.
		if( m_hParent != nullptr && !m_isChaining )
			m_hParent->Touch( pOther );
	}

	virtual void Blocked( CBaseEntity *pOther )
	{
		if( m_pfnBlocked )
			(this->*m_pfnBlocked)( pOther );

		// forward the blocked event to our parent, if any.
		if( m_hParent != nullptr )
			m_hParent->Blocked( pOther );
	}

	virtual void MoveDone( void ) { if( m_pfnMoveDone )(this->*m_pfnMoveDone)(); };

	// allow engine to allocate instance data
	void *operator new( size_t stAllocateBlock, entvars_t *pev )
	{
		return (void *)ALLOC_PRIVATE(ENT(pev), static_cast<int>(stAllocateBlock));
	};

	// don't use this.
#if _MSC_VER >= 1200 // only build this code if MSVC++ 6.0 or higher
	void operator delete(void *pMem, entvars_t *pev)
	{
		pev->flags |= FL_KILLME;
	};
#endif
	void UpdateOnRemove( void );

	// common member functions
	void SUB_Remove( void );
	void SUB_DoNothing( void );
	void SUB_StartFadeOut ( void );
	void SUB_FadeOut ( void );
	void SUB_CallUseToggle( void ) { this->Use( this, this, USE_TOGGLE, 0 ); }
	int ShouldToggle( USE_TYPE useType, BOOL currentState );
	void FireBullets( ULONG cShots, Vector vecSrc, Vector vecDirShooting,	Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq = 4, int iDamage = 0, entvars_t *pevAttacker = nullptr);
	void Teleport( const Vector *newPosition, const Vector *newAngles, const Vector *newVelocity );
	void GetVectors( Vector *pForward, Vector *pRight, Vector *pUp ) const;

	virtual CBaseEntity *Respawn( void ) { return nullptr; }

	void	SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value, string_t m_iszAltTarget = NULL_STRING );
	// Do the bounding boxes of these two intersect?
	int	Intersects( CBaseEntity *pOther );
	int	AreaIntersect( Vector mins, Vector maxs );
	int	TriggerIntersects( CBaseEntity *pOther );
	void	MakeDormant( void );
	int	IsDormant( void );
	int	IsWater( void );
	BOOL	IsLockedByMaster( void ) { return FALSE; }

	static CBaseEntity *Instance( edict_t *pent )
	{ 
		if ( !pent )
			pent = ENT(0);
		CBaseEntity *pEnt = (CBaseEntity *)GET_PRIVATE(pent); 
		return pEnt; 
	}

	static CBaseEntity *Instance( entvars_t *pev ) { return Instance( ENT( pev ) ); }
	static CBaseEntity *Instance( int eoffset) { return Instance( ENT( eoffset) ); }

	static CBaseMonster *GetMonsterPointer( entvars_t *pevMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pevMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}
	static CBaseMonster *GetMonsterPointer( edict_t *pentMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pentMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}

	// Ugly code to lookup all functions to make sure they are exported when set.
#ifdef _DEBUG
	void FunctionCheck( void *pFunction, char *name ) 
	{ 
		if (pFunction && !UTIL_FunctionToName( GetDataDescMap(), pFunction ) )
			ALERT( at_warning, "No EXPORT: %s:%s (%08lx)\n", GetClassname(), name, (size_t)pFunction );
	}

	BASEPTR ThinkSet( BASEPTR func, char *name ) 
	{ 
		m_pfnThink = func; 
		FunctionCheck( *(reinterpret_cast<void **>(&m_pfnThink)), name ); 
		return func;
	}

	ENTITYFUNCPTR TouchSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnTouch = func; 
		FunctionCheck( *(reinterpret_cast<void **>(&m_pfnTouch)), name ); 
		return func;
	}

	USEPTR UseSet( USEPTR func, char *name ) 
	{ 
		m_pfnUse = func; 
		FunctionCheck( *(reinterpret_cast<void **>(&m_pfnUse)), name ); 
		return func;
	}

	ENTITYFUNCPTR BlockedSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnBlocked = func; 
		FunctionCheck( *(reinterpret_cast<void **>(&m_pfnBlocked)), name ); 
		return func;
	}

	BASEPTR MoveDoneSet( BASEPTR func, char *name )
	{
		m_pfnMoveDone = func;
		FunctionCheck( *(reinterpret_cast<void **>(&m_pfnMoveDone)), name );
		return func;
	}
#endif
	void RelinkEntity( BOOL touch_triggers = FALSE, const Vector *pPrevOrigin = NULL, BOOL sleep_portals = FALSE );
	void TouchLinks( edict_t *ent, const Vector &entmins, const Vector &entmaxs, const Vector *pPrevOrigin, areanode_t *node );
	void ClipLinks( edict_t *ent, const Vector &entmins, const Vector &entmaxs, areanode_t *node );
	void SleepPortals( edict_t *ent, const Vector &entmins, const Vector &entmaxs, areanode_t *node );

	CBaseEntity *GetGroundEntity( void ) { return Instance( pev->groundentity ); }
	void SetGroundEntity( CBaseEntity *pGround )
	{
		if( pGround != nullptr )
		{
			pev->groundentity = ENT( pGround->pev );
			pev->flags |= FL_ONGROUND;
		}
		else ClearGroundEntity();
	}

	void SetGroundEntity( edict_t *pentGround )
	{
		if( pentGround != nullptr )
		{
			pev->groundentity = pentGround;
			pev->flags |= FL_ONGROUND;
		}
		else ClearGroundEntity();
	}

	void SetGroundEntity( entvars_t *pevGround )
	{
		if( pevGround != nullptr )
		{
			pev->groundentity = ENT( pevGround );
			pev->flags |= FL_ONGROUND;
		}
		else ClearGroundEntity();
	}

	void ClearGroundEntity( void )
	{
		pev->groundentity = nullptr;
		pev->flags &= ~FL_ONGROUND;
	}

	void MakeNonSolid( void )
	{
		if( m_iOldSolid == SOLID_NOT && pev->solid != SOLID_NOT )
		{
			m_iOldSolid = pev->solid;
			pev->solid = SOLID_NOT;
		}
	}

	void RestoreSolid( void )
	{
		if( m_iOldSolid != SOLID_NOT && pev->solid == SOLID_NOT )
		{
			pev->solid = m_iOldSolid;
			m_iOldSolid = SOLID_NOT;
		}
	}

	void MakeNonMoving( void )
	{
		if( m_iOldMoveType == MOVETYPE_NONE && pev->movetype != MOVETYPE_NONE )
		{
			m_iOldMoveType = pev->movetype;
			pev->movetype = MOVETYPE_NONE;
		}
	}

	void RestoreMoveType( void )
	{
		if( m_iOldMoveType != MOVETYPE_NONE && pev->movetype == MOVETYPE_NONE )
		{
			pev->movetype = m_iOldMoveType;
			m_iOldMoveType = MOVETYPE_NONE;
		}
	}

	void MakeTempParent( CBaseEntity *pParent )
	{
		if(( m_hParent != nullptr ) || FBitSet( m_iFlags, MF_TEMP_PARENT ))
			return;

		SetBits( m_iFlags, MF_TEMP_PARENT );
		SetParent( pParent );
	}

	void ClearTempParent( void )
	{
		if( !FBitSet( m_iFlags, MF_TEMP_PARENT ))
			return;

		ClearBits( m_iFlags, MF_TEMP_PARENT );
		SetParent( 0 );
	}

	inline modtype_t GetModelType( void )
	{
		return UTIL_GetModelType( pev->modelindex );
	}

	inline bool IsStudioModel( void )
	{
		return (GetModelType() == mod_studio) ? true : false;
	}

	// virtual functions used by a few classes
	
	// used by monsters that are created by the MonsterMaker
	virtual	void UpdateOwner( void ) { return; };

	static CBaseEntity *Create( char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner = nullptr );

	virtual BOOL FBecomeProne( void ) {return FALSE;};
	edict_t *edict() { return ENT( pev ); };
	EOFFSET eoffset( ) { return OFFSET( pev ); };
	int entindex( ) { return ENTINDEX( edict() ); };

	virtual Vector Center( ) { return (pev->absmax + pev->absmin) * 0.5; }; // center point of entity
	virtual Vector EyePosition( ) { return GetAbsOrigin() + pev->view_ofs; };			// position of eyes
	virtual Vector EarPosition( ) { return GetAbsOrigin() + pev->view_ofs; };			// position of ears
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ); };		// position to shoot at
	virtual BOOL IsPointSized() const { return (pev->size == g_vecZero) ? true : false; }

	virtual int Illumination( ) { return GETENTITYILLUM( ENT( pev ) ); };

	virtual BOOL FVisible ( CBaseEntity *pEntity );
	virtual BOOL FVisible ( const Vector &vecOrigin );

	virtual BOOL ShouldCollide( CBaseEntity *pOther );
};

//-----------------------------------------------------------------------------
// Returns the entity-to-world transform
//-----------------------------------------------------------------------------
inline matrix4x4 &CBaseEntity::EntityToWorldTransform( void ) 
{ 
	if( IsPlayer( ))
	{
		m_local = matrix4x4( pev->origin, Vector( 0.0f, pev->angles.y, 0.0f ));
	}
	else if( pev->flags & FL_ABSTRANSFORM )
	{
		CalcAbsolutePosition();
	}
	return m_local; 
}

inline const matrix4x4 &CBaseEntity :: EntityToWorldTransform( void ) const
{ 
	if( const_cast<CBaseEntity*>(this)->IsPlayer( ))
	{
		const_cast<CBaseEntity*>(this)->m_local = matrix4x4( pev->origin, Vector( 0.0f, pev->angles.y, 0.0f ));
	}
	else if( pev->flags & FL_ABSTRANSFORM )
	{
		const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	}
	return m_local; 
}

inline const Vector &CBaseEntity :: CollisionToWorldSpace( const Vector &in, Vector *pResult ) const 
{
	// Makes sure we don't re-use the same temp twice
	if( UTIL_GetModelType( pev->modelindex ) != mod_brush )
	{
		// boxes can't rotate
		*pResult = in + GetAbsOrigin();
	}
	else
	{
		*pResult = EntityToWorldTransform().VectorTransform( in );
	}

	return *pResult;
}

inline const Vector &CBaseEntity :: WorldToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	if( UTIL_GetModelType( pev->modelindex ) != mod_brush )
	{
		// boxes can't rotate
		*pResult = in - GetAbsOrigin();
	}
	else
	{
		*pResult = EntityToWorldTransform().VectorITransform( in );
	}
	return *pResult;
}

inline float CBaseEntity::GetLocalTime( void ) const
{ 
	return pev->ltime; 
}

inline void CBaseEntity::IncrementLocalTime( float flTimeDelta )
{ 
	pev->ltime += flTimeDelta; 
}

inline void CBaseEntity::SetMoveDoneTime( float flDelay )
{
	if( flDelay >= 0 )
		m_flMoveDoneTime = GetLocalTime() + flDelay;
	else m_flMoveDoneTime = -1;
}

inline float CBaseEntity::GetMoveDoneTime( void ) const
{
	return (m_flMoveDoneTime >= 0) ? m_flMoveDoneTime - GetLocalTime() : -1;
}

// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a 
// member function of a base class.  static_cast is a sleezy way around that problem.

#ifdef _DEBUG

#define SetThink( a ) ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), #a )
#define SetTouch( a ) TouchSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )
#define SetUse( a ) UseSet( static_cast <void (CBaseEntity::*)(	CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a), #a )
#define SetBlocked( a ) BlockedSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )
#define SetMoveDone( a ) MoveDoneSet( static_cast <void (CBaseEntity::*)(void)> (a), #a )

#else

#define SetThink( a ) m_pfnThink = static_cast <void (CBaseEntity::*)(void)> (a)
#define SetTouch( a ) m_pfnTouch = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetUse( a ) m_pfnUse = static_cast <void (CBaseEntity::*)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a)
#define SetBlocked( a ) m_pfnBlocked = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetMoveDone( a ) m_pfnMoveDone = static_cast <void (CBaseEntity::*)(void)> (a)

#endif


class CPointEntity : public CBaseEntity
{
	DECLARE_CLASS( CPointEntity, CBaseEntity );
public:
	void Spawn( void );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};


typedef struct locksounds			// sounds that doors and buttons make when locked/unlocked
{
	string_t	sLockedSound;		// sound a door makes when it's locked
	string_t	sLockedSentence;	// sentence group played when door is locked
	string_t	sUnlockedSound;		// sound a door makes when it's unlocked
	string_t	sUnlockedSentence;	// sentence group played when door is unlocked

	int		iLockedSentence;		// which sentence in sentence group to play next
	int		iUnlockedSentence;		// which sentence in sentence group to play next

	float	flwaitSound;			// time delay between playing consecutive 'locked/unlocked' sounds
	float	flwaitSentence;			// time delay between playing consecutive sentences
	BYTE	bEOFLocked;				// true if hit end of list of locked sentences
	BYTE	bEOFUnlocked;			// true if hit end of list of unlocked sentences
} locksound_t;

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton);

//
// MultiSouce
//

#define MAX_MASTER_TARGETS	32
#define MAX_MULTI_TARGETS	64 // maximum number of targets for a multi_manager, multi_watcher or multisource

//
// generic Delay entity.
//
class CBaseDelay : public CBaseEntity
{
	DECLARE_CLASS( CBaseDelay, CBaseEntity );
public:
	float		m_flWait;
	float		m_flDelay;
	string_t		m_iszKillTarget;
	string_t		m_sMaster;
	EHANDLE		m_hActivator;
	STATE		m_iState;

	virtual void KeyValue( KeyValueData* pkvd);
	virtual STATE GetState( void ) { return m_iState; };

	BOOL IsLockedByMaster( void );
	BOOL IsLockedByMaster( CBaseEntity *pActivator );

	// common member functions
	void SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value, string_t m_iszAltTarget = NULL_STRING );
	void DelayThink( void );

	DECLARE_DATADESC();
};


class CBaseAnimating : public CBaseDelay
{
	DECLARE_CLASS( CBaseAnimating, CBaseDelay );
public:
	// Basic Monster Animation functions
	float StudioFrameAdvance( float flInterval = 0.0 ); // accumulate animation frame time from last time called until now
	float StudioGaitFrameAdvance( void );	// accumulate gait frame time from last time called until now
	int GetSequenceFlags( void );
	int LookupActivity ( int activity );
	int LookupActivityHeaviest ( int activity );
	int LookupSequence ( const char *label );
	static void *GetModelPtr( int modelindex );
	void *GetModelPtr( void );
	void ResetSequenceInfo ( );
	void DispatchAnimEvents ( float flFutureInterval = 0.1 ); // Handle events that have happend since last time called up until X seconds into the future
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent ) { return; };
	float SetBoneController ( int iController, float flValue );
	void InitBoneControllers ( void );
	float SetBlending ( int iBlender, float flValue );
	void GetBonePosition ( int iBone, Vector &origin, Vector &angles );
	void GetAutomovement( Vector &origin, Vector &angles, float flInterval = 0.1 );
	int  FindTransition( int iEndingSequence, int iGoalSequence, int *piDir );
	void GetAttachment ( int iAttachment, Vector &origin, Vector &angles );
	int GetAttachment ( const char *pszAttachment, Vector &origin, Vector &angles );
	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	int ExtractBbox( int sequence, Vector &mins, Vector &maxs );
	int GetHitboxSetByName( const char *szName );
	void SetSequenceBox( void );
	CStudioBoneSetup *GetBoneSetup();

	// animation needs
	float	m_flFrameRate;	// computed FPS for current sequence
	float	m_flGroundSpeed;	// computed linear movement rate for current sequence
	float	m_flLastEventCheck;	// last time the event list was checked
	BOOL	m_fSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	BOOL	m_fSequenceLoops;	// true if the sequence loops

	// gaitsequence needs
	float	m_flGaitMovement;

	DECLARE_DATADESC();
};


//
// generic Toggle entity.
//
#define	SF_ITEM_USE_ONLY	256 //  ITEM_USE_ONLY = BUTTON_USE_ONLY = DOOR_USE_ONLY!!! 

class CBaseToggle : public CBaseAnimating
{
	DECLARE_CLASS( CBaseToggle, CBaseAnimating );
public:
	void		KeyValue( KeyValueData *pkvd );

	TOGGLE_STATE	m_toggle_state;
	float		m_flActivateFinished;//like attack_finished, but for doors
	float		m_flMoveDistance;// how far a door should slide or rotate
	float		m_flLip;
	float		m_flTWidth;// for plats
	float		m_flTLength;// for plats

	Vector		m_vecPosition1;
	Vector		m_vecPosition2;
	Vector		m_vecPosition3;
	Vector		m_vecAngle1;
	Vector		m_vecAngle2;

	int		m_cTriggersLeft;		// trigger_counter only, # of activations remaining
	float		m_flHeight;
	float		m_flWidth;

	Vector		m_vecFinalDest;
	Vector		m_vecFinalAngle;
	Vector		m_vecFloor;	// func_platform destination floor
	int		m_movementType;	// movement type
	int		m_bitsDamageInflict;// DMG_ damage type that the door or tigger does

	virtual int Restore( CRestore &restore );

	DECLARE_DATADESC();

	virtual STATE GetState( void );
	virtual int GetToggleState( void ) { return m_toggle_state; }
	virtual float GetDelay( void ) { return m_flWait; }
	virtual void MoveDone( void );

	// common member functions
	// common member functions
	void LinearMove( const Vector &vecInput, float flSpeed );
	void AngularMove( const Vector &vecDestAngle, float flSpeed );
	void ComplexMove( const Vector &vecDestOrigin, const Vector &vecDestAngle, float flSpeed );
	void LinearMoveDone( void );
	void AngularMoveDone( void );
	void ComplexMoveDone( void );

	static float	AxisValue( int flags, const Vector &angles );
	static void	AxisDir( entvars_t *pev );
	static float	AxisDelta( int flags, const Vector &angle1, const Vector &angle2 );
};

// people gib if their health is <= this at the time of death
#define GIB_HEALTH_VALUE		-30

#define ROUTE_SIZE			8 // how many waypoints a monster can store at one time
#define MAX_OLD_ENEMIES		4 // how many old enemies to remember

#define bits_CAP_DUCK		( 1 << 0 ) // crouch
#define bits_CAP_JUMP		( 1 << 1 ) // jump/leap
#define bits_CAP_STRAFE		( 1 << 2 ) // strafe ( walk/run sideways)
#define bits_CAP_SQUAD		( 1 << 3 ) // can form squads
#define bits_CAP_SWIM		( 1 << 4 ) // proficiently navigate in water
#define bits_CAP_CLIMB		( 1 << 5 ) // climb ladders/ropes
#define bits_CAP_USE		( 1 << 6 ) // open doors/push buttons/pull levers
#define bits_CAP_HEAR		( 1 << 7 ) // can hear forced sounds
#define bits_CAP_AUTO_DOORS		( 1 << 8 ) // can trigger auto doors
#define bits_CAP_OPEN_DOORS		( 1 << 9 ) // can open manual doors
#define bits_CAP_TURN_HEAD		( 1 << 10) // can turn head, always bone controller 0
#define bits_CAP_RANGE_ATTACK1	( 1 << 11) // can do a range attack 1
#define bits_CAP_RANGE_ATTACK2	( 1 << 12) // can do a range attack 2
#define bits_CAP_MELEE_ATTACK1	( 1 << 13) // can do a melee attack 1
#define bits_CAP_MELEE_ATTACK2	( 1 << 14) // can do a melee attack 2
#define bits_CAP_FLY		( 1 << 15) // can fly, move all around

#define bits_CAP_DOORS_GROUP		(bits_CAP_USE | bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS)

#include <dmg_types.h>

// NOTE: tweak these values based on gameplay feedback:

#define PARALYZE_DURATION		2		// number of 2 second intervals to take damage
#define PARALYZE_DAMAGE		1.0		// damage to take each 2 second interval

#define NERVEGAS_DURATION		2
#define NERVEGAS_DAMAGE		5.0

#define POISON_DURATION		5
#define POISON_DAMAGE		2.0

#define RADIATION_DURATION		2
#define RADIATION_DAMAGE		1.0

#define ACID_DURATION		2
#define ACID_DAMAGE			5.0

#define SLOWBURN_DURATION		2
#define SLOWBURN_DAMAGE		1.0

#define SLOWFREEZE_DURATION		2
#define SLOWFREEZE_DAMAGE		1.0

#define itbd_Paralyze		0		
#define itbd_NerveGas		1
#define itbd_Poison			2
#define itbd_Radiation		3
#define itbd_DrownRecover		4
#define itbd_Acid			5
#define itbd_SlowBurn		6
#define itbd_SlowFreeze		7
#define CDMG_TIMEBASED		8

// when calling KILLED(), a value that governs gib behavior is expected to be 
// one of these three values
#define GIB_NORMAL			0 // gib if entity was overkilled
#define GIB_NEVER			1 // never gib, no matter how much death damage is done ( freezing, etc )
#define GIB_ALWAYS			2 // always gib ( Houndeye Shock, Barnacle Bite )

class CBaseMonster;
class CCineMonster;
class CSound;

#include "basemonster.h"

//
// Converts a entvars_t * to a class pointer
// It will allocate the class and entity if necessary
//
template <class T> T * GetClassPtr( T *a )
{
	entvars_t *pev = (entvars_t *)a;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	a = (T *)GET_PRIVATE(ENT(pev));

	if (a == NULL) 
	{
		// allocate private data 
		a = new(pev) T;
		a->pev = pev;
	}
	return a;
}

template <class T> T * GetClassPtr( T *newEnt, const char *className )
{
	entvars_t *pev = (entvars_t *)newEnt;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	newEnt = (T *)GET_PRIVATE(ENT(pev));

	if (newEnt == NULL) 
	{
		// allocate private data 
		newEnt = new(pev) T;
		newEnt->pev = pev;
	}
	newEnt->SetClassname( className );

	return newEnt;
}

#define TRACER_FREQ		4			// Tracers fire every 4 bullets

typedef struct _SelAmmo
{
	BYTE	Ammo1Type;
	BYTE	Ammo1;
	BYTE	Ammo2Type;
	BYTE	Ammo2;
} SelAmmo;

// this moved here from world.cpp, to allow classes to be derived from it
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================
class CWorld : public CBaseEntity
{
	DECLARE_CLASS( CWorld, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
};

extern CBaseEntity *g_pWorld;
