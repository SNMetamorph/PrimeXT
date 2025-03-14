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
#include "stringlib.h"

#ifndef ACTIVITY_H
#include "activity.h"
#endif

#ifndef ENGINECALLBACK_H
#include "enginecallback.h"
#endif

#ifndef PHYSCALLBACK_H
#include "physcallback.h"
#endif

#include "datamap.h"
#include "xashxt_strings.h"
#include "extdll.h"
#include "com_model.h"
#include "sprite.h"

// uncomment this to enable custom string pool
#define HAVE_STRINGPOOL

inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent );  // implementation later in this file

extern globalvars_t	*gpGlobals;

class CBaseEntity;

extern void	UTIL_Remove( CBaseEntity *pEntity );
extern CBaseEntity	*CreateEntityByName( const char *className, entvars_t *pev = NULL );

#define NULL_STRING		0

#define ALLOC_STRING	(*g_engfuncs.pfnAllocString)

#ifdef HAVE_STRINGPOOL
// TODO need some workarond for 64-bit string offset
#define STRING		(*g_engfuncs.pfnSzFromIndex)
#define MAKE_STRING		(*g_engfuncs.pfnAllocString)
#else
#define STRING( offset )	(const char *)(gpGlobals->pStringBase + (int)offset)
#define MAKE_STRING( str )	((int)str - (int)STRING(0))
#endif

class IEntityFactory
{
public:
	virtual CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL ) = 0;
	virtual void Destroy( CBaseEntity *pEntity ) = 0;
	virtual size_t GetEntitySize() = 0;
};

// This is the glue that hooks .MAP entity class names to our CPP classes
class IEntityFactoryDictionary
{
public:
	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName ) = 0;
	virtual CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL ) = 0;
	virtual void Destroy( const char *pClassName, CBaseEntity *pEntity ) = 0;
	virtual IEntityFactory *FindFactory( const char *pClassName ) = 0;
	virtual const char *GetCannonicalName( const char *pClassName ) = 0;
};

IEntityFactoryDictionary *EntityFactoryDictionary();

template <class T> class CEntityFactory : public IEntityFactory
{
public:
	CEntityFactory( const char *pClassName )
	{
		EntityFactoryDictionary()->InstallFactory( this, pClassName );
	}

	CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL )
	{
		T* pEnt = GetClassPtr((T*)pev, pClassName);
		return pEnt;
	}

	void Destroy( CBaseEntity *pEntity )
	{
		UTIL_Remove( pEntity );
	}

	virtual size_t GetEntitySize()
	{
		return sizeof(T);
	}
};

inline edict_t *FIND_ENTITY_BY_CLASSNAME(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "classname", pszName);
}	

inline edict_t *FIND_ENTITY_BY_TARGETNAME(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "targetname", pszName);
}	

// for doing a reverse lookup. Say you have a door, and want to find its button.
inline edict_t *FIND_ENTITY_BY_TARGET(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "target", pszName);
}	

// Makes these more explicit, and easier to find
#define FILE_GLOBAL static
#define DLL_GLOBAL

// Until we figure out why "const" gives the compiler problems, we'll just have to use
// this bogus "empty" define to mark things as constant.
#define CONSTANT

// More explicit than "int"
typedef int EOFFSET;

// In case it's not alread defined
typedef int BOOL;

// In case this ever changes
#define M_PI			3.14159265358979323846

// This is the glue that hooks .MAP entity class names to our CPP classes
// The function is used to intialize / allocate the object for the entity
#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) \
	static CEntityFactory<DLLClassName> mapClassName( #mapClassName );

//
// Conversion among the three types of "entity", including identity-conversions.
//
#ifdef DEBUG
	extern edict_t *DBG_EntOfVars(const entvars_t *pev);
	inline edict_t *ENT(const entvars_t *pev)	{ return DBG_EntOfVars(pev); }
#else
	inline edict_t *ENT(const entvars_t *pev)	{ return pev->pContainingEntity; }
#endif
edict_t *ENT( CBaseEntity *pEnt );
inline edict_t *ENT(edict_t *pent)			{ return pent; }
inline edict_t *ENT(EOFFSET eoffset)			{ return (*g_engfuncs.pfnPEntityOfEntOffset)(eoffset); }
inline EOFFSET OFFSET(EOFFSET eoffset)			{ return eoffset; }
inline EOFFSET OFFSET(const edict_t *pent)	
{ 
#if _DEBUG
	if ( !pent )
		ALERT( at_error, "Bad ent in OFFSET()\n" );
#endif
	return (*g_engfuncs.pfnEntOffsetOfPEntity)(pent); 
}
inline EOFFSET OFFSET(entvars_t *pev)				
{ 
#if _DEBUG
	if ( !pev )
		ALERT( at_error, "Bad pev in OFFSET()\n" );
#endif
	return OFFSET(ENT(pev)); 
}
inline entvars_t *VARS(entvars_t *pev)					{ return pev; }

inline entvars_t *VARS(edict_t *pent)			
{ 
	if ( !pent )
		return NULL;

	return &pent->v; 
}

inline entvars_t* VARS(EOFFSET eoffset)				{ return VARS(ENT(eoffset)); }
inline int	  ENTINDEX(edict_t *pEdict)			{ return (*g_engfuncs.pfnIndexOfEdict)(pEdict); }
inline edict_t* INDEXENT( int iEdictNum )		{ return (*g_engfuncs.pfnPEntityOfEntIndexAllEntities)(iEdictNum); }
inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent ) {
	(*g_engfuncs.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ENT(ent));
}

// Testing the three types of "entity" for nullity
#define eoNullEntity 0
inline BOOL FNullEnt(EOFFSET eoffset)			{ return eoffset == 0; }
inline BOOL FNullEnt(const edict_t* pent)	{ return pent == NULL || FNullEnt(OFFSET(pent)); }
inline BOOL FNullEnt(entvars_t* pev)				{ return pev == NULL || FNullEnt(OFFSET(pev)); }
extern BOOL FNullEnt( CBaseEntity *ent );

// Testing strings for nullity
#define iStringNull 0
inline BOOL FStringNull(int iString)			{ return iString == iStringNull; }

// dll managment
bool Sys_LoadLibrary( const char *dllname, dllhandle_t *handle, const dllfunc_t *fcts = NULL );
void *Sys_GetProcAddress( dllhandle_t handle, const char *name );
void Sys_FreeLibrary( dllhandle_t *handle );

#define cchMapNameMost 32

// Dot products for view cone checking
#define VIEW_FIELD_FULL		(float)-1.0 // +-180 degrees
#define	VIEW_FIELD_WIDE		(float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks 
#define	VIEW_FIELD_NARROW	(float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define	VIEW_FIELD_ULTRA_NARROW	(float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

// All monsters need this data
#define		DONT_BLEED			-1
#define		BLOOD_COLOR_RED		(BYTE)247
#define		BLOOD_COLOR_YELLOW	(BYTE)195
#define		BLOOD_COLOR_GREEN	BLOOD_COLOR_YELLOW

typedef enum 
{
	MONSTERSTATE_NONE = 0,
	MONSTERSTATE_IDLE,
	MONSTERSTATE_COMBAT,
	MONSTERSTATE_ALERT,
	MONSTERSTATE_HUNT,
	MONSTERSTATE_PRONE,
	MONSTERSTATE_SCRIPT,
	MONSTERSTATE_PLAYDEAD,
	MONSTERSTATE_DEAD

} MONSTERSTATE;

typedef enum
{
	GLOBAL_OFF = 0,
	GLOBAL_ON = 1,
	GLOBAL_DEAD = 2
} GLOBALESTATE;

// Things that toggle (buttons/triggers/doors) need this
typedef enum
{
	TS_AT_TOP,
	TS_AT_BOTTOM,
	TS_GOING_UP,
	TS_GOING_DOWN
} TOGGLE_STATE;

typedef enum
{
	USE_OFF = 0,	// disable entity
	USE_ON,		// enable entity
	USE_SET,		// pass entity parm (value)
	USE_TOGGLE,	// toggle state
	USE_RESET,	// reset entity to initial position
	USE_REMOVE,	// remove entity from map
} USE_TYPE;

typedef enum
{
	STATE_OFF = 0,	// disabled, inactive, invisible, closed, or stateless. Or non-alert monster.
	STATE_ON,		// enabled, active, visisble, or open. Or alert monster.
	STATE_TURN_ON,	// door opening, env_fade fading in, etc.
	STATE_TURN_OFF,	// door closing, monster dying (?).
	STATE_IN_USE,	// player is in control (train/tank/barney/scientist).
	STATE_DEAD,	// entity dead
} STATE;

//-----------------------------------------------------------------------------
// Purpose: Holds an entity's previous abs origin and angles at the time of
// teleportation. Used for child & constrained entity fixup to prevent
// lazy updates of abs origins and angles from messing things up.
//-----------------------------------------------------------------------------
struct TeleportListEntry_t
{
	CBaseEntity *pEntity;
	Vector prevAbsOrigin;
	Vector prevAbsAngles;
};

// Misc useful
inline BOOL FStrEq( const char *sz1, const char *sz2 )
	{ return (Q_strcmp( sz1, sz2 ) == 0); }
inline BOOL FClassnameIs(edict_t* pent, const char* szClassname)
{
	if( FNullEnt( pent )) return FALSE;
	return FStrEq( STRING( VARS( pent )->classname ), szClassname );
}

inline BOOL FClassnameIs(entvars_t* pev, const char* szClassname )
{
	if( FNullEnt( pev )) return FALSE;
	return FStrEq( STRING( pev->classname ), szClassname );
}

BOOL FClassnameIs( CBaseEntity *pEnt, const char *szClassname );

extern void DumpEntityNames_f( void );
extern void DumpEntitySizes_f( void );
extern void DumpStrings_f( void );

extern const char* GetStringForUseType( USE_TYPE useType );
extern const char* GetStringForState( STATE state );
extern const char* GetStringForGlobalState( GLOBALESTATE state );
extern const char* GetContentsString( int contents );
extern void PrintStringForDamage( int dmgbits );
extern STATE GetStateForString( const char *string );
extern void CheckForMultipleParents( CBaseEntity *pEntity, CBaseEntity *pParent );

// Misc. Prototypes
extern void			UTIL_SetSize			(entvars_t* pev, const Vector &vecMin, const Vector &vecMax);
extern float		UTIL_VecToYaw			(const Vector &vec);
extern Vector		UTIL_VecToAngles		(const Vector &vec);
extern float		UTIL_AngleMod			(float a);
extern float		UTIL_AngleDiff			( float destAngle, float srcAngle );

extern CBaseEntity	*UTIL_FindEntityInSphere(CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius);
extern CBaseEntity	*UTIL_FindEntityByString(CBaseEntity *pStartEntity, const char *szKeyword, const char *szValue );
extern CBaseEntity	*UTIL_FindEntityByClassname(CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityByTargetname(CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pActivator );
extern CBaseEntity	*UTIL_FindEntityGeneric(const char *szName, const Vector &vecSrc, float flRadius );
extern CBaseEntity	*UTIL_FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityByMonsterTarget( CBaseEntity *pStartEntity, const char *szName );

// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
extern CBaseEntity	*UTIL_PlayerByIndex( int playerIndex );

#define UTIL_EntitiesInPVS(pent)			(*g_engfuncs.pfnEntitiesInPVS)(pent)
extern void			UTIL_MakeVectors		(const Vector &vecAngles);

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
extern int			UTIL_MonstersInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius );
extern int			UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask );
extern const char*			UTIL_ButtonSound( int sound );

inline void UTIL_MakeVectorsPrivate( const Vector &vecAngles, float *p_vForward, float *p_vRight, float *p_vUp )
{
	g_engfuncs.pfnAngleVectors( vecAngles, p_vForward, p_vRight, p_vUp );
}

extern void			UTIL_MakeAimVectors		( const Vector &vecAngles ); // like MakeVectors, but assumes pitch isn't inverted
extern void			UTIL_MakeInvVectors		( const Vector &vec, globalvars_t *pgv );

extern void UTIL_SetOrigin		( CBaseEntity *pEnt, const Vector &vecOrigin );
extern void UTIL_SetAngles		( CBaseEntity *pEnt, const Vector &vecAngles );
extern void UTIL_EmitAmbientSound	( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch );
extern void UTIL_ParticleEffect		( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount );
extern void UTIL_CreateAuroraSystem ( CBaseEntity *pPlayer, CBaseEntity *pParent, const char *aurFile, int attachment );
extern void UTIL_ScreenShake		( const Vector &center, float amplitude, float frequency, float duration, float radius, bool bAllowInAir = false );
extern void UTIL_ScreenShakeAll		( const Vector &center, float amplitude, float frequency, float duration );
extern void UTIL_ShowMessage		( const char *pString, CBaseEntity *pPlayer );
extern void UTIL_ShowMessageAll		( const char *pString );
extern void UTIL_ScreenFadeAll		( const Vector &color, float fadeTime, float holdTime, int alpha, int flags );
extern void UTIL_ScreenFade			( CBaseEntity *pEntity, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags );
extern void UTIL_FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value = 0 );
extern void UTIL_FireTargets( string_t targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value = 0 );

typedef enum { ignore_monsters=1, dont_ignore_monsters=0, missile=2 } IGNORE_MONSTERS;
typedef enum { ignore_glass=1, dont_ignore_glass=0 } IGNORE_GLASS;
extern void		UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr);
extern void		UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr);
enum { point_hull=0, human_hull=1, large_hull=2, head_hull=3 };
extern void		UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr);
extern TraceResult		UTIL_GetGlobalTrace( void );
extern void		UTIL_CopyTraceToGlobal( TraceResult *trace );
extern void		UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr);
extern void		UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecStart, const Vector &vecEnd, TraceResult *ptr );
extern Vector		UTIL_GetAimVector( edict_t* pent, float flSpeed );
extern int		UTIL_PointContents( const Vector &vec );

extern int 		UTIL_IsMasterTriggered( string_t sMaster, CBaseEntity *pActivator );
extern void		UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount );
extern void		UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount );
extern Vector		UTIL_RandomBloodVector( void );
extern BOOL		UTIL_ShouldShowBlood( int bloodColor );
extern void		UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor );
extern void		UTIL_BloodStudioDecalTrace( TraceResult *pTrace, int bloodColor );
extern void		UTIL_DecalTrace( TraceResult *pTrace, const char *decalName );
extern bool		UTIL_TraceCustomDecal(TraceResult *pTrace, const char *name, float angle = 0.0f, int persistent = 0);
extern BOOL		UTIL_StudioDecalTrace(TraceResult *pTrace, const char *name, int flags = 0);
extern void		UTIL_RestoreCustomDecal(const Vector &vecPos, const Vector &vecNormal, int entityIndex, int modelIndex, const char *name, int flags, float angle);
extern void		UTIL_RestoreStudioDecal(const Vector &vecEnd, const Vector &vecNormal, int entityIndex, int modelIndex, const char *name, int flags, struct modelstate_s *state, int lightcache, const Vector &scale);
extern void		UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber );
extern void		UTIL_GunshotDecalTrace( TraceResult *pTrace, const char *decalNumber );
extern void		UTIL_Sparks( const Vector &position );
extern void		UTIL_DoSpark( entvars_t *pev, const Vector &location );
extern void		UTIL_Ricochet( const Vector &position, float scale );
extern void		UTIL_StringToVector( float *pVector, const char *pString );
extern void		UTIL_StringToIntArray( int *pVector, int count, const char *pString );
extern Vector		UTIL_ClampVectorToBox( const Vector &input, const Vector &clampSize );
extern float		UTIL_Approach( float target, float value, float speed );
extern float		UTIL_ApproachAngle( float target, float value, float speed );
extern float		UTIL_AngleDistance( float next, float cur );
extern CBaseEntity*		UTIL_FindClientInPVS( edict_t *pEdict );

extern char		*UTIL_VarArgs( char *format, ... );
extern BOOL		UTIL_IsValidEntity( edict_t *pent );
extern BOOL		UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 );
extern BOOL		UTIL_IsFacing( entvars_t *pevTest, const Vector &reference ); //LRC

// Use for ease-in, ease-out style interpolation (accel/decel)
extern float		UTIL_SplineFraction( float value, float scale );

// Search for water transition along a vertical line
extern float		UTIL_WaterLevel( const Vector &position, float minz, float maxz );
extern void		UTIL_Bubbles( Vector mins, Vector maxs, int count );
extern void		UTIL_BubbleTrail( Vector from, Vector to, int count );

// allows precacheing of other entities
extern void		UTIL_PrecacheOther( const char *szClassname );
extern int		UTIL_PrecacheSound( const char* s );
extern int		UTIL_PrecacheSound( string_t s );
extern unsigned short	UTIL_PrecacheMovie( const char *s, int allow_sound = 0 );
extern unsigned short	UTIL_PrecacheMovie( string_t iString, int allow_sound = 0 );

// prints a message to each client
extern void		UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );
inline void		UTIL_CenterPrintAll( const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL ) 
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, msg_name, param1, param2, param3, param4 );
}

class CBasePlayerItem;
class CBasePlayer;
extern BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon );

// prints messages through the HUD
extern void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

// prints a message to the HUD say (chat)
extern void	UTIL_SayText( const char *pText, CBaseEntity *pEntity );
extern void	UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity );

extern void	UTIL_SetView( string_t iszViewEntity );
extern void	UTIL_SetView( CBaseEntity *pActivator, string_t iszViewEntity );
extern void	UTIL_SetView( CBaseEntity *pActivator, CBaseEntity *pViewEnt = NULL );

typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;

// prints as transparent 'title' to the HUD
extern void	UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage );
extern void	UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage );

// for handy use with ClientPrint params
extern char *UTIL_dtos1( int d );
extern char *UTIL_dtos2( int d );
extern char *UTIL_dtos3( int d );
extern char *UTIL_dtos4( int d );

// Writes message to console with timestamp and FragLog header.
extern void			UTIL_LogPrintf( char *fmt, ... );

// Sorta like FInViewCone, but for nonmonsters. 
extern float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir );

extern void UTIL_StripToken( const char *pKey, char *pDest );// for redundant keynames

// Misc functions
void UTIL_SetMovedir( CBaseEntity *pEnt );
extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern int BuildChangeList( LEVELLIST *pLevelList, int maxList );

//
// How did I ever live without ASSERT?
//
#ifdef	DEBUG
void DBG_AssertFunction(BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage);
#define ASSERT(f)		DBG_AssertFunction(f, #f, __FILE__, __LINE__, NULL)
#define ASSERTSZ(f, sz)	DBG_AssertFunction(f, #f, __FILE__, __LINE__, sz)
#else	// !DEBUG
#define ASSERT(f)
#define ASSERTSZ(f, sz)
#endif	// !DEBUG


extern DLL_GLOBAL ULONG	g_ulFrameCount;

//
// Constants that were used only by QC (maybe not used at all now)
//
// Un-comment only as needed
//
#define LANGUAGE_ENGLISH			0
#define LANGUAGE_GERMAN			1
#define LANGUAGE_FRENCH			2
#define LANGUAGE_BRITISH			3

extern DLL_GLOBAL int		g_Language;
extern DLL_GLOBAL BOOL		g_fPhysicInitialized;
extern DLL_GLOBAL BOOL		g_fTouchLinkSemaphore;
extern DLL_GLOBAL int		g_iXashEngineBuildNumber;	// may be 0 for old versions or GoldSource

#define AMBIENT_SOUND_STATIC			0	// medium radius attenuation
#define AMBIENT_SOUND_EVERYWHERE		1
#define AMBIENT_SOUND_SMALLRADIUS		2
#define AMBIENT_SOUND_MEDIUMRADIUS		4
#define AMBIENT_SOUND_LARGERADIUS		8
#define AMBIENT_SOUND_START_SILENT		16
#define AMBIENT_SOUND_NOT_LOOPING		32

#define SPEAKER_START_SILENT			1	// wait for trigger 'on' to start announcements

#define SND_SPAWNING		(1<<8)		// duplicated in protocol.h we're spawing, used in some cases for ambients 
#define SND_STOP			(1<<5)		// duplicated in protocol.h stop sound
#define SND_CHANGE_VOL		(1<<6)		// duplicated in protocol.h change sound vol
#define SND_CHANGE_PITCH	(1<<7)		// duplicated in protocol.h change sound pitch

#define	LFO_SQUARE			1
#define LFO_TRIANGLE		2
#define LFO_RANDOM			3

// func_rotating
#define SF_BRUSH_ROTATE_Y_AXIS		0
#define SF_BRUSH_ROTATE_INSTANT		1
#define SF_BRUSH_ROTATE_BACKWARDS	2
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8


#define SF_BRUSH_ROTATE_SMALLRADIUS	128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS 256
#define SF_BRUSH_ROTATE_LARGERADIUS 512

#define PUSH_BLOCK_ONLY_X	1
#define PUSH_BLOCK_ONLY_Y	2

#define VEC_HULL_MIN		Vector(-16, -16, -36)
#define VEC_HULL_MAX		Vector( 16,  16,  36)
#define VEC_HUMAN_HULL_MIN	Vector( -16, -16, 0 )
#define VEC_HUMAN_HULL_MAX	Vector( 16, 16, 72 )
#define VEC_HUMAN_HULL_DUCK	Vector( 16, 16, 36 )

#define VEC_VIEW		Vector( 0, 0, 28 )

#define VEC_DUCK_HULL_MIN	Vector(-16, -16, -18 )
#define VEC_DUCK_HULL_MAX	Vector( 16,  16,  18)
#define VEC_DUCK_VIEW	Vector( 0, 0, 12 )

#define CIN_FRAMETIME	(1.0f / 30.0f)

#define SVC_TEMPENTITY	23
#define SVC_INTERMISSION	30
#define SVC_CDTRACK		32
#define SVC_WEAPONANIM	35
#define SVC_ROOMTYPE	37
#define SVC_DIRECTOR	51

#define SF_LIGHT_START_OFF		1

#define SPAWNFLAG_NOMESSAGE	1
#define SPAWNFLAG_NOTOUCH	1
#define SPAWNFLAG_DROIDONLY	4

#define SPAWNFLAG_USEONLY	1		// can't be touched, must be used (buttons)

#define TELE_PLAYER_ONLY	1
#define TELE_SILENT			2

// Sound Utilities

// sentence groups
#define CBSENTENCENAME_MAX		32
#define CVOXFILESENTENCEMAX		4096		// max number of sentences in game. NOTE: this must match
						// MAX_SENTENCES in engine\vox.h!!!

extern char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
extern int gcallsentences;

int USENTENCEG_Pick(int isentenceg, char *szfound);
int USENTENCEG_PickSequential(int isentenceg, char *szfound, int ipick, int freset);
void USENTENCEG_InitLRU(unsigned char *plru, int count);

void SENTENCEG_Init();
void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick);
int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlayRndSz(edict_t *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch, int ipick, int freset);
int SENTENCEG_GetIndex(const char *szrootname);
int SENTENCEG_Lookup(const char *sample, char *sentencenum);

void TEXTURETYPE_Init();
float TEXTURETYPE_PlaySound(TraceResult *ptr,  Vector vecSrc, Vector vecEnd, int iBulletType);

// NOTE: use EMIT_SOUND_DYN to set the pitch of a sound. Pitch of 100
// is no pitch shift.  Pitch > 100 up to 255 is a higher pitch, pitch < 100
// down to 1 is a lower pitch.   150 to 70 is the realistic range.
// EMIT_SOUND_DYN with pitch != 100 should be used sparingly, as it's not quite as
// fast as EMIT_SOUND (the pitchshift mixer is not native coded).

void EMIT_SOUND_DYN(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch );

inline void EMIT_SOUND( edict_t *entity, int channel, const char *sample, float volume, float attenuation )
{
	EMIT_SOUND_DYN( entity, channel, sample, volume, attenuation, 0, PITCH_NORM );
}

inline void STOP_SOUND( edict_t *entity, int channel, const char *sample )
{
	EMIT_SOUND_DYN( entity, channel, sample, 0, 0, SND_STOP, PITCH_NORM );
}

void EMIT_SOUND_SUIT(edict_t *entity, const char *sample);
void EMIT_GROUPID_SUIT(edict_t *entity, int isentenceg);
void EMIT_GROUPNAME_SUIT(edict_t *entity, const char *groupname);

#define PRECACHE_SOUND_ARRAY( a ) \
	{ for (int i = 0; i < ARRAYSIZE( a ); i++ ) PRECACHE_SOUND((char *) a [i]); }

#define EMIT_SOUND_ARRAY_DYN( chan, array ) \
	EMIT_SOUND_DYN ( ENT(pev), chan , array [ RANDOM_LONG(0,ARRAYSIZE( array )-1) ], 1.0, ATTN_NORM, 0, RANDOM_LONG(95,105) ); 

#define RANDOM_SOUND_ARRAY( array ) (array) [ RANDOM_LONG(0,ARRAYSIZE( (array) )-1) ]

#define PLAYBACK_EVENT( flags, who, index ) PLAYBACK_EVENT_FULL( flags, who, index, 0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );
#define PLAYBACK_EVENT_DELAY( flags, who, index, delay ) PLAYBACK_EVENT_FULL( flags, who, index, delay, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

#define GROUP_OP_AND	0
#define GROUP_OP_NAND	1

extern int g_groupmask;
extern int g_groupop;

class UTIL_GroupTrace
{
public:
	UTIL_GroupTrace( int groupmask, int op );
	~UTIL_GroupTrace( void );

private:
	int m_oldgroupmask, m_oldgroupop;
};

void UTIL_SetGroupTrace( int groupmask, int op );
void UTIL_UnsetGroupTrace( void );

#define UNSTICK_VELOCITY	100.0f	// FIXME: temporary solution

// physic engine utils
extern BOOL UTIL_CanRotate( CBaseEntity *pEntity );
extern BOOL UTIL_CanRotateBModel( CBaseEntity *pEntity );
extern modtype_t UTIL_GetModelType( int modelindex );
extern void SV_Impact( CBaseEntity *pEntity1, CBaseEntity *pEntity2, TraceResult *trace );
extern BOOL SV_TestEntityPosition( CBaseEntity *pEntity, CBaseEntity *pBlocker );
extern void SV_UpdateBaseVelocity( CBaseEntity *pEntity );
extern BOOL UITL_ExternalBmodel( int modelindex );
extern angletype_t UTIL_GetSpriteType( int modelindex );
extern BOOL UTIL_AllowHitboxTrace( CBaseEntity *pEntity );
extern void UTIL_Teleport( CBaseEntity *pSource, TeleportListEntry_t &entry, const Vector *newPosition, const Vector *newAngles, const Vector *newVelocity );
extern void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Vector &vecAngles );
extern void UTIL_GetModelBounds( int modelIndex, Vector &mins, Vector &maxs );
extern void UTIL_SetSize( CBaseEntity *pEntity, const Vector &min, const Vector &max );

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );
typedef void (CBaseEntity::*USEPTR)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
typedef void (__cdecl *LINK_ENTITY_FN)( entvars_t *pev );
typedef void (*AREACHECK)( CBaseEntity *pCheck );

#define AREA_SOLID		0
#define AREA_TRIGGERS	1
#define AREA_WATER		2

struct areaclip_t
{
	Vector		vecAbsMin;
	Vector		vecAbsMax;
	AREACHECK		m_pfnCallback;
	int		area_type;
};

int UTIL_HullPointContents( hull_t *hull, int num, const Vector &p );
hull_t *UTIL_HullForBsp( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, Vector &offset );
void UTIL_AreaNode(	Vector vecAbsMin, Vector vecAbsMax, int type, AREACHECK pfnCallback );
trace_t UTIL_CombineTraces( trace_t *cliptrace, trace_t *trace, CBaseEntity *pTouch );
int UTIL_DropToFloor( CBaseEntity *pEntity );
void UTIL_WaterMove( CBaseEntity *pEntity );
int UTIL_LoadSoundPreset( int iString );

const char *UTIL_FunctionToName( DATAMAP *pMap, void *function );
void *UTIL_FunctionFromName( DATAMAP *pMap, const char *pName );

string_t GetStdLightStyle( int iStyle );
