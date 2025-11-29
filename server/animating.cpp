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

===== monsters.cpp ========================================================

  Monster-related utility code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "animation.h"
#include "saverestore.h"
#include "studio.h"
#include "bs_defs.h"

BEGIN_DATADESC( CBaseAnimating )
	DEFINE_FIELD( m_flFrameRate, FIELD_FLOAT ),
	DEFINE_FIELD( m_flGroundSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastEventCheck, FIELD_TIME ),
	DEFINE_FIELD( m_fSequenceFinished, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fSequenceLoops, FIELD_BOOLEAN ),
END_DATADESC()

void *CBaseAnimating :: GetModelPtr( void )
{
	return GET_MODEL_PTR( edict() );
}

void *CBaseAnimating :: GetModelPtr( int modelindex )
{
	if( !g_fPhysicInitialized || modelindex <= 1 )
		return NULL;

	model_t *mod = (model_t *)MODEL_HANDLE( modelindex );

	if( mod && mod->type == mod_studio )
		return mod->cache.data;
	return NULL;
}

//=========================================================
// StudioFrameAdvance - advance the animation frame up to the current time
// if an flInterval is passed in, only advance animation that number of seconds
//=========================================================
float CBaseAnimating :: StudioFrameAdvance ( float flInterval )
{
	if (flInterval == 0.0)
	{
		flInterval = (gpGlobals->time - pev->animtime);
		if (flInterval <= 0.001)
		{
			pev->animtime = gpGlobals->time;
			return 0.0;
		}
	}
	if (! pev->animtime)
		flInterval = 0.0;
	
	pev->frame += flInterval * m_flFrameRate * pev->framerate;
	pev->animtime = gpGlobals->time;

	if (pev->frame < 0.0 || pev->frame >= 256.0) 
	{
		if (m_fSequenceLoops)
			pev->frame -= (int)(pev->frame / 256.0) * 256.0;
		else
			pev->frame = (pev->frame < 0.0) ? 0 : 255;
		m_fSequenceFinished = TRUE;	// just in case it wasn't caught in GetEvents
	}

	return flInterval;
}

float CBaseAnimating :: StudioGaitFrameAdvance( void ) 
{
	if( !pev->gaitsequence || ( IsNetClient() && FBitSet( pev->fixangle, 1 )))
	{
		if( IsPlayer( ))
		{
			// reset torso controllers
			pev->controller[0] = 0x7F;
			pev->controller[1] = 0x7F;
			pev->controller[2] = 0x7F;
			pev->controller[3] = 0x7F;
		}
		return 0.0f;
	}

	float delta = gpGlobals->frametime;

	m_flGaitMovement = pev->velocity.Length() * delta;

	if( pev->velocity.Length2D() < 0.1f )
	{
		float flYawDiff = pev->angles[YAW] - m_flGaitYaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;

		if( flYawDiff > 180 ) flYawDiff -= 360;
		if( flYawDiff < -180 ) flYawDiff += 360;

		if( delta < 0.25f )
			flYawDiff *= delta * 4.0f;
		else flYawDiff *= delta;

		m_flGaitYaw += flYawDiff;
		m_flGaitYaw -= (int)(m_flGaitYaw / 360) * 360;

		m_flGaitMovement = 0.0f;
	}
	else
	{
		float gaityaw = RAD2DEG( atan2( pev->velocity.y, pev->velocity.x ));
		m_flGaitYaw = bound( -180.0f, gaityaw, 180.0f );
	}

	// calc side to side turning
	float flYaw = pev->angles[YAW] - m_flGaitYaw; // view direction relative to movement
	flYaw -= (int)(flYaw / 360) * 360;

	if( flYaw < -180.0f ) flYaw += 360.0f;
	if( flYaw > 180.0f ) flYaw -= 360.0f;

	flYaw = (int)flYaw;

	// kill the yaw jitter
	if( flYaw > -1.0f && flYaw < 1.0f )
		flYaw = 0.0f;

	if( flYaw > 120.0f )
	{
		m_flGaitYaw -= 180.0f;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw -= 180.0f;
	}
	else if( flYaw < -120.0f )
	{
		m_flGaitYaw += 180.0f;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw += 180.0f;
	}

	// classic Half-Life method
	byte iTorsoAdjust = (byte)bound( 0, ((flYaw / 4.0f) + 30) / (60.0f / 255.0f), 255 );

	// value it's already in range 0-255
	pev->controller[0] = iTorsoAdjust;
	pev->controller[1] = iTorsoAdjust;
	pev->controller[2] = iTorsoAdjust;
	pev->controller[3] = iTorsoAdjust;

	SetBlending( 0, (pev->angles[PITCH] * 3.0f));
	pev->angles[YAW] = m_flGaitYaw;

	if( pev->angles[YAW] < -0.0f )
		pev->angles[YAW] += 360.0f;

	int localMaxFrame = GetBoneSetup()->LocalMaxFrame(pev->gaitsequence);
	CalcGaitFrame(GetModelPtr(), pev->gaitsequence, pev->fuser1, m_flGaitMovement, localMaxFrame);

	return m_flGaitMovement;
}


//=========================================================
// LookupActivity
//=========================================================
int CBaseAnimating :: LookupActivity ( int activity )
{
	ASSERT( activity != 0 );
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::LookupActivity( pmodel, activity );
}

//=========================================================
// LookupActivityHeaviest
//
// Get activity with highest 'weight'
//
//=========================================================
int CBaseAnimating :: LookupActivityHeaviest ( int activity )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::LookupActivityHeaviest( pmodel, activity );
}

//=========================================================
//=========================================================
int CBaseAnimating :: LookupSequence ( const char *label )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::LookupSequence( pmodel, label );
}


//=========================================================
//=========================================================
void CBaseAnimating :: ResetSequenceInfo ( )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	GetSequenceInfo( pmodel, pev->sequence, &m_flFrameRate, &m_flGroundSpeed );
	m_fSequenceLoops = ((GetSequenceFlags() & STUDIO_LOOPING) != 0);
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
	m_fSequenceFinished = FALSE;
	m_flLastEventCheck = gpGlobals->time;
}

//=========================================================
//=========================================================
void CBaseEntity :: ResetPoseParameters ( )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	CalcDefaultPoseParameters( pmodel, m_flPoseParameter );

	pev->vuser1[0] = m_flPoseParameter[0];
	pev->vuser1[1] = m_flPoseParameter[1];
	pev->vuser1[2] = m_flPoseParameter[2];

	pev->vuser2[0] = m_flPoseParameter[3];
	pev->vuser2[1] = m_flPoseParameter[4];
	pev->vuser2[2] = m_flPoseParameter[5];

	pev->vuser3[0] = m_flPoseParameter[6];
	pev->vuser3[1] = m_flPoseParameter[7];
	pev->vuser3[2] = m_flPoseParameter[8];

	pev->vuser4[0] = m_flPoseParameter[9];
	pev->vuser4[1] = m_flPoseParameter[10];
	pev->vuser4[2] = m_flPoseParameter[11];
}

int CBaseEntity :: LookupPoseParameter( const char *szName )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::LookupPoseParameter( pmodel, szName, m_flPoseParameter );
}

void CBaseEntity :: SetPoseParameter( int iParameter, float flValue )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	::SetPoseParameter( pmodel, iParameter, flValue, m_flPoseParameter );
}

float CBaseEntity :: GetPoseParameter( int iParameter )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::GetPoseParameter( pmodel, iParameter, m_flPoseParameter );
}

//=========================================================
//=========================================================
BOOL CBaseAnimating :: GetSequenceFlags( )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::GetSequenceFlags( pmodel, pev->sequence );
}

//=========================================================
// DispatchAnimEvents
//=========================================================
void CBaseAnimating :: DispatchAnimEvents ( float flInterval )
{
	MonsterEvent_t	event;

	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	if ( !pmodel )
	{
		ALERT( at_aiconsole, "Gibbed monster is thinking!\n" );
		return;
	}

	// FIXME: I have to do this or some events get missed, and this is probably causing the problem below
	flInterval = 0.1;

	// FIX: this still sometimes hits events twice
	float flStart = pev->frame + (m_flLastEventCheck - pev->animtime) * m_flFrameRate * pev->framerate;
	float flEnd = pev->frame + flInterval * m_flFrameRate * pev->framerate;
	m_flLastEventCheck = pev->animtime + flInterval;

	m_fSequenceFinished = FALSE;
	if (flEnd >= 256.f || flEnd <= 0.0f) 
		m_fSequenceFinished = TRUE;

	int index = 0;

	while ( (index = GetAnimationEvent( pmodel, pev->sequence, &event, flStart, flEnd, index ) ) != 0 )
	{
		HandleAnimEvent( &event );
	}
}


//=========================================================
//=========================================================
float CBaseAnimating :: SetBoneController ( int iController, float flValue )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return SetController( pmodel, pev->controller, iController, flValue );
}

//=========================================================
//=========================================================
void CBaseAnimating :: InitBoneControllers ( void )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	SetController( pmodel, pev->controller, 0, 0.0 );
	SetController( pmodel, pev->controller, 1, 0.0 );
	SetController( pmodel, pev->controller, 2, 0.0 );
	SetController( pmodel, pev->controller, 3, 0.0 );

	ResetPoseParameters();
}

//=========================================================
//=========================================================
float CBaseAnimating :: SetBlending ( int iBlender, float flValue )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	return ::SetBlending( pmodel, pev->sequence, pev->blending, iBlender, flValue );
}

//=========================================================
//=========================================================
void CBaseAnimating :: GetBonePosition ( int iBone, Vector &origin, Vector &angles )
{
	GET_BONE_POSITION( ENT(pev), iBone, origin, angles );
}

//=========================================================
//=========================================================
void CBaseAnimating :: GetAttachment ( int iAttachment, Vector &origin, Vector &angles )
{
	GET_ATTACHMENT( ENT(pev), iAttachment, origin, angles );

	if( m_hParent != NULL )
	{
		matrix4x4 parentSpace = GetParentToWorldTransform();
		origin = parentSpace.VectorITransform( origin );
	}
}

int CBaseAnimating :: GetAttachment ( const char *pszAttachment, Vector &origin, Vector &angles )
{
	CStudioBoneSetup *pStudioHdr = GetBoneSetup();
	if( !pStudioHdr ) return -1;

	int nAttachment = pStudioHdr->FindAttachment( pszAttachment );
	if( nAttachment == -1 ) return -1;

	GET_ATTACHMENT( edict(), nAttachment, origin, angles );

	if( m_hParent != NULL )
	{
		matrix4x4 parentSpace = GetParentToWorldTransform();
		origin = parentSpace.VectorITransform( origin );
	}

	return nAttachment;
}

//=========================================================
//=========================================================
int CBaseAnimating :: FindTransition( int iEndingSequence, int iGoalSequence, int *piDir )
{
	void *pmodel = GET_MODEL_PTR( ENT(pev) );
	
	if (piDir == NULL)
	{
		int iDir;
		int sequence = ::FindTransition( pmodel, iEndingSequence, iGoalSequence, &iDir );
		if (iDir != 1)
			return -1;
		else
			return sequence;
	}

	return ::FindTransition( pmodel, iEndingSequence, iGoalSequence, piDir );
}

//=========================================================
//=========================================================
void CBaseAnimating :: GetAutomovement( Vector &origin, Vector &angles, float flInterval )
{

}

int CBaseAnimating :: GetHitboxSetByName( const char *szName )
{
	return ::FindHitboxSetByName(  GET_MODEL_PTR( ENT(pev) ), szName );
}

void CBaseAnimating :: SetBodygroup( int iGroup, int iValue )
{
	::SetBodygroup( GET_MODEL_PTR( ENT(pev) ), pev->body, iGroup, iValue );
}

int CBaseAnimating :: GetBodygroup( int iGroup )
{
	return ::GetBodygroup( GET_MODEL_PTR( ENT(pev) ), pev->body, iGroup );
}


int CBaseAnimating :: ExtractBbox( int sequence, Vector &mins, Vector &maxs )
{
	return ::ExtractBbox( GET_MODEL_PTR( ENT(pev) ), sequence, mins, maxs );
}

CStudioBoneSetup *CBaseAnimating :: GetBoneSetup( void )
{
	return ::GetBaseBoneSetup( pev->modelindex, m_flPoseParameter );
}

//=========================================================
//=========================================================

void CBaseAnimating :: SetSequenceBox( void )
{
	Vector mins, maxs;

	// Get sequence bbox
	if ( ExtractBbox( pev->sequence, mins, maxs ) )
	{
		// expand box for rotation
		// find min / max for rotations
		float yaw = GetAbsAngles().y * (M_PI / 180.0);
		
		Vector xvector, yvector;
		xvector.x = cos(yaw);
		xvector.y = sin(yaw);
		yvector.x = -sin(yaw);
		yvector.y = cos(yaw);
		Vector bounds[2];

		bounds[0] = mins;
		bounds[1] = maxs;
		
		Vector rmin( 9999, 9999, 9999 );
		Vector rmax( -9999, -9999, -9999 );
		Vector base, transformed;

		for (int i = 0; i <= 1; i++ )
		{
			base.x = bounds[i].x;
			for ( int j = 0; j <= 1; j++ )
			{
				base.y = bounds[j].y;
				for ( int k = 0; k <= 1; k++ )
				{
					base.z = bounds[k].z;
					
				// transform the point
					transformed.x = xvector.x*base.x + yvector.x*base.y;
					transformed.y = xvector.y*base.x + yvector.y*base.y;
					transformed.z = base.z;
					
					if (transformed.x < rmin.x)
						rmin.x = transformed.x;
					if (transformed.x > rmax.x)
						rmax.x = transformed.x;
					if (transformed.y < rmin.y)
						rmin.y = transformed.y;
					if (transformed.y > rmax.y)
						rmax.y = transformed.y;
					if (transformed.z < rmin.z)
						rmin.z = transformed.z;
					if (transformed.z > rmax.z)
						rmax.z = transformed.z;
				}
			}
		}
		rmin.z = 0;
		rmax.z = rmin.z + 1;
		UTIL_SetSize( pev, rmin, rmax );
	}
}


