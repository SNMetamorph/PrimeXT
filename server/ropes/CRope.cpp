/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"
#include "CRope.h"
#include "CRopeSegment.h"
#include "studio.h"
#include "player.h"
#include "user_messages.h"

#define HOOK_CONSTANT	2500.0f
#define SPRING_DAMPING	0.1f	
#define ROPE_IGNORE_SAMPLES	4		// integrator may be hanging if less than

RopeSampleData g_pTempList[MAX_LIST_SEGMENTS][MAX_SEGMENTS];
	
static const char* const g_pszCreakSounds[] = 
{
	"items/rope1.wav",
	"items/rope2.wav",
	"items/rope3.wav"
};

void TruncateEpsilon( Vector& vec )
{
	vec = ( vec * 10.0f + 0.5f ) / 10.0f;
}

BEGIN_DATADESC( CRope )
	DEFINE_FIELD( m_iSegments, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecLastEndPos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGravity, FIELD_VECTOR ),
	DEFINE_FIELD( m_iNumSamples, FIELD_INTEGER ),
	DEFINE_FIELD( m_bSimulateBones, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bObjectAttached, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iAttachedObjectsSegment, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDetachTime, FIELD_TIME ),
	DEFINE_ARRAY( m_pSegments, FIELD_CLASSPTR, MAX_SEGMENTS ),
	DEFINE_FIELD( m_bDisallowPlayerAttachment, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iszBodyModel, FIELD_STRING ),
	DEFINE_FIELD( m_iszEndingModel, FIELD_STRING ),
	DEFINE_FIELD( m_flAttachedObjectsOffset, FIELD_FLOAT ),
	DEFINE_FIELD( m_bMakeSound, FIELD_BOOLEAN ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_rope, CRope );

CRope :: CRope()
{
	m_iszBodyModel = MAKE_STRING( "models/rope16.mdl" );
	m_iszEndingModel = MAKE_STRING( "models/rope16.mdl" );
}

void CRope :: KeyValue( KeyValueData* pkvd )
{
	if( FStrEq( pkvd->szKeyName, "segments" ))
	{
		m_iSegments = atoi( pkvd->szValue );

		if( m_iSegments >= MAX_SEGMENTS )
			m_iSegments = MAX_SEGMENTS - 1;
		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "bodymodel" ))
	{
		m_iszBodyModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "endingmodel" ))
	{
		m_iszEndingModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "disable" ))
	{
		m_bDisallowPlayerAttachment = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = true;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CRope :: Precache( void )
{
	BaseClass::Precache();

	PRECACHE_SOUND_ARRAY( g_pszCreakSounds );
}

void CRope :: Spawn( void )
{
	if( m_iszEndingModel == NULL_STRING )
		m_iszEndingModel = m_iszBodyModel;
	m_bMakeSound = true;

	int modelindex = PRECACHE_MODEL( STRING( m_iszBodyModel ));
	studiohdr_t *phdr = (studiohdr_t *)CBaseAnimating::GetModelPtr( modelindex );

	if( phdr && phdr->numbones > 4 )
	{
		SET_MODEL( edict(), STRING( m_iszBodyModel ));
		m_iSegments = phdr->numbones;
		m_bSimulateBones = true;
	}

	Precache();

	Vector vecOrigin = GetAbsOrigin();
	m_vecGravity = Vector( 0.0f, 0.0f, -50.0f );
	m_iNumSamples = m_iSegments + 1;

	for( int i = 0; i < m_iNumSamples; i++ )
	{
		CRopeSegment *pSample;

		// NOTE: last segment are invisible, first segment have an infinity mass
		if( i == m_iNumSamples - 2 )
			pSample = CRopeSegment :: CreateSegment( GetEndingModel( ), 1.0f );
		else if( i == m_iNumSamples - 1 )
			pSample = CRopeSegment :: CreateSegment( NULL_STRING, 0.2f );
		else pSample = CRopeSegment :: CreateSegment( GetBodyModel( ), i == 0 ? 0.0f : 1.0f );
		if( i < ROPE_IGNORE_SAMPLES ) pSample->SetCanBeGrabbed( false );

		// just to have something valid here
		pSample->SetAbsOrigin( vecOrigin );
		pSample->SetMasterRope( this );
		m_pSegments[i] = pSample;
	}

	if( m_bSimulateBones )
	{
		mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);

		for( int i = 0; i < m_iNumSamples; i++ )
		{
			if( i < ( phdr->numbones - 1 ))
			{
				Vector p0 = Vector( pbone[i].value );
				Vector p1 = Vector( pbone[i+1].value );
				m_pSegments[i]->m_Data.restLength = (p1 - p0).Length();
			}
			else if( i < phdr->numbones )
				m_pSegments[i]->m_Data.restLength = m_pSegments[i-1]->m_Data.restLength;
			// last segment is equal 0
		}

		SET_MODEL( edict(), STRING( m_iszBodyModel ));
		SetAbsAngles( Vector( 90.0f, 0.0f, 0.0f ));
	}

	Vector origin, angles;
	const Vector vecGravity = m_vecGravity.Normalize();

	// setup the segments position
	if( m_iNumSamples > 2 )
	{
		CRopeSegment *pPrev, *pCurr;

		for( int iSeg = 1; iSeg < m_iNumSamples; iSeg++ )
		{
			pPrev = m_pSegments[iSeg - 1];
			pCurr = m_pSegments[iSeg - 0];

			// calc direction from previous sample origin and attachment
			origin = pPrev->GetAbsOrigin() + pPrev->m_Data.restLength * vecGravity;

			pCurr->SetAbsOrigin( origin );
			m_vecLastEndPos = origin;
			SetSegmentAngles( pPrev, pCurr );
		}
	}

	// initialize position data
	for( int iSeg = 0; iSeg < m_iNumSamples; iSeg++ )
	{
		CRopeSegment* pSegment = m_pSegments[iSeg];
		pSegment->m_Data.mPosition = pSegment->GetAbsOrigin();
	}

	SetNextThink( 0.1 );
}

void CRope :: GetAlignmentAngles( const Vector& vecTop, const Vector& vecBottom, Vector& vecOut )
{
	Vector vecDist = vecBottom - vecTop;

	Vector vecResult = vecDist.Normalize();

	const float flRoll = RAD2DEG( acos( DotProduct( vecResult, Vector( 0.0f, 1.0f, 0.0f ))) );

	vecDist.y = 0;

	vecResult = vecDist.Normalize();

	const float flPitch = RAD2DEG( acos( DotProduct( vecResult, Vector( 0.0f, 0.0f, -1.0f ))) );

	vecOut.x = ( vecResult.x >= 0.0 ) ? -flPitch : flPitch;
	vecOut.y = 0.0f;
	vecOut.z = -flRoll;
}

void CRope :: SendUpdateBones( void )
{
	if( !m_bSimulateBones )
		return;

	studiohdr_t *phdr = (studiohdr_t *)CBaseAnimating::GetModelPtr( pev->modelindex );
	if( !phdr ) return;

	matrix4x4 localSpace = EntityToWorldTransform().Invert();
	Vector pos, mins, maxs;
	Radian ang;

	ClearBounds( mins, maxs );

	// TEST MESSAGE TO SETUP PROCEDURAL BONES
	// TODO: replace it with special delta-message in engine
	MESSAGE_BEGIN( MSG_PVS, gmsgSetupBones, pev->origin );
		WRITE_ENTITY( (short)ENTINDEX( edict() ) );
		WRITE_BYTE( phdr->numbones );

		for( int i = 0; i < phdr->numbones; i++ )
		{
			matrix4x4 local = m_pSegments[i]->EntityToWorldTransform();
			local = localSpace.ConcatTransforms( local );
			local.GetStudioTransform( pos, ang );
			AddPointToBounds( pos, mins, maxs );
			WRITE_SHORT( pos.x * 128 );
			WRITE_SHORT( pos.y * 128 );
			WRITE_SHORT( pos.z * 128 );
			WRITE_SHORT( ang.x * 512 );
			WRITE_SHORT( ang.y * 512 );
			WRITE_SHORT( ang.z * 512 );
		}

	MESSAGE_END();
	ExpandBounds( mins, maxs, 2.0f );
	UTIL_SetSize( pev, mins, maxs );
}

void CRope :: Think( void )
{
	int subSteps = Q_rint( 100.0f * gpGlobals->frametime ) * 2;
	float delta = (1.0f / 100.0f) * 2.0f;
	subSteps = bound( 2, subSteps, 10 );

	if( m_hParent != NULL )
	{
		// get move origin from parent class
		CRopeSegment* pSegment = m_pSegments[0];
		pSegment->SetAbsOrigin( GetAbsOrigin() );
		pSegment->m_Data.mPosition = GetAbsOrigin();
	}

	// make ropes nonsense to sv_fps
	for( int idx = 0; idx < subSteps; idx++ )
	{
		ComputeForces( m_pSegments );
		RK4Integrate( delta );
	}

	TraceModels();

	if( ShouldCreak() )
	{
		EMIT_SOUND( edict(), CHAN_BODY, g_pszCreakSounds[RANDOM_LONG( 0, ARRAYSIZE( g_pszCreakSounds ) - 1 )], VOL_NORM, ATTN_NORM );
	}

	SendUpdateBones();
	SetNextThink( 0.0f );
}

void CRope :: ComputeForces( RopeSampleData* pSystem )
{
	int i;

	for( i = 0; i < m_iNumSamples; i++ )
	{
		ComputeSampleForce( pSystem[i] );
	}

	for( i = 0; i < m_iSegments; i++ )
	{
		ComputeSpringForce( pSystem[i+0], pSystem[i+1] );
	}
}

void CRope :: ComputeSampleForce( RopeSampleData& data )
{
	data.mForce = g_vecZero;

	if( data.mMassReciprocal != 0.0 )
	{
		data.mForce = data.mForce + ( m_vecGravity / data.mMassReciprocal );
	}

	if( data.mApplyExternalForce )
	{
		data.mForce += data.mExternalForce;
		data.mApplyExternalForce = false;
		data.mExternalForce = g_vecZero;
	}

	if( DotProduct( m_vecGravity, data.mVelocity ) >= 0 )
	{
		data.mForce += data.mVelocity * -0.04;
	}
	else
	{
		data.mForce -= data.mVelocity;
	}
}

void CRope :: ComputeSpringForce( RopeSampleData& first, RopeSampleData& second )
{
	Vector vecDist = first.mPosition - second.mPosition;

	const double flDistance = vecDist.Length();
	const double flForce = ( flDistance - first.restLength ) * HOOK_CONSTANT;

	const double flNewRelativeDist = DotProduct( first.mVelocity - second.mVelocity, vecDist ) * SPRING_DAMPING;

	vecDist = vecDist.Normalize();

	const double flSpringFactor = -( flNewRelativeDist / flDistance + flForce );
	const Vector vecForce = flSpringFactor * vecDist;

	first.mForce += vecForce;
	second.mForce -= vecForce;
}

void CRope :: ComputeForces( CRopeSegment** ppSystem )
{
	int i;

	for( i = 0; i < m_iNumSamples; i++ )
	{
		ComputeSampleForce( ppSystem[i]->m_Data );
	}

	for( i = 0; i < m_iSegments; i++ )
	{
		ComputeSpringForce( ppSystem[i+0]->m_Data, ppSystem[i+1]->m_Data );
	}
}

void CRope :: RK4Integrate( const float flDeltaTime )
{
	const float flDeltas[MAX_LIST_SEGMENTS - 1] = 
	{
		flDeltaTime * 0.5f,
		flDeltaTime * 0.5f,
		flDeltaTime * 0.5f,
		flDeltaTime
	};

	RopeSampleData *pTemp1, *pTemp2, *pTemp3, *pTemp4;
	int i;

	pTemp1 = g_pTempList[0];
	pTemp2 = g_pTempList[1];

	for( i = 0; i < m_iNumSamples; i++, pTemp1++, pTemp2++ )
	{
		const RopeSampleData& data = m_pSegments[i]->m_Data;

		pTemp2->mForce = data.mMassReciprocal * data.mForce * flDeltas[0];
		pTemp2->mVelocity = data.mVelocity * flDeltas[0];
		pTemp2->restLength = data.restLength;

		pTemp1->mMassReciprocal = data.mMassReciprocal;
		pTemp1->mVelocity = data.mVelocity + pTemp2->mForce;
		pTemp1->mPosition = data.mPosition + pTemp2->mVelocity;
		pTemp1->restLength = data.restLength;
	}

	ComputeForces( g_pTempList[0] );

	for( int iStep = 2; iStep < MAX_LIST_SEGMENTS - 1; iStep++ )
	{
		pTemp1 = g_pTempList[0];
		pTemp2 = g_pTempList[iStep];

		for( i = 0; i < m_iNumSamples; i++, pTemp1++, pTemp2++ )
		{
			const RopeSampleData& data = m_pSegments[i]->m_Data;

			pTemp2->mForce = data.mMassReciprocal * pTemp1->mForce * flDeltas[iStep - 1];
			pTemp2->mVelocity = pTemp1->mVelocity * flDeltas[iStep - 1];
			pTemp2->restLength = data.restLength;

			pTemp1->mMassReciprocal = data.mMassReciprocal;
			pTemp1->mVelocity = data.mVelocity + pTemp2->mForce;
			pTemp1->mPosition = data.mPosition + pTemp2->mVelocity;
			pTemp1->restLength = data.restLength;
		}

		ComputeForces( g_pTempList[0] );
	}

	pTemp1 = g_pTempList[0];
	pTemp2 = g_pTempList[4];

	for( i = 0; i < m_iNumSamples; i++, pTemp1++, pTemp2++ )
	{
		const RopeSampleData& data = m_pSegments[i]->m_Data;

		pTemp2->mForce = data.mMassReciprocal * pTemp1->mForce * flDeltas[3];
		pTemp2->mVelocity = pTemp1->mVelocity * flDeltas[3];
	}

	pTemp1 = g_pTempList[1];
	pTemp2 = g_pTempList[2];
	pTemp3 = g_pTempList[3];
	pTemp4 = g_pTempList[4];

	for( i = 0; i < m_iNumSamples; i++, pTemp1++, pTemp2++, pTemp3++, pTemp4++ )
	{
		CRopeSegment *pSegment = m_pSegments[i];
		const Vector vecPosChange = 1.0f / 6.0f * ( pTemp1->mVelocity + ( pTemp2->mVelocity + pTemp3->mVelocity ) * 2 + pTemp4->mVelocity );
		const Vector vecVelChange = 1.0f / 6.0f * ( pTemp1->mForce + ( pTemp2->mForce + pTemp3->mForce ) * 2 + pTemp4->mForce );

		// store final changes for each segment
		pSegment->m_Data.mPosition += vecPosChange;
		pSegment->m_Data.mVelocity += vecVelChange;
	}
}

void CRope :: SetSegmentOrigin( CRopeSegment *pCurr, CRopeSegment *pNext )
{
}

void CRope :: SetSegmentAngles( CRopeSegment *pCurr, CRopeSegment *pNext )
{
	Vector vecAngles;

	GetAlignmentAngles( pCurr->m_Data.mPosition, pNext->m_Data.mPosition, vecAngles );

	if( UTIL_GetModelType( pCurr->pev->modelindex ) == mod_sprite )
		pCurr->SetAbsAngles( Vector( 0.0f, 0.0f, -vecAngles.x ));
	else pCurr->SetAbsAngles( vecAngles );
}

void CRope :: TraceModels( void )
{
	if( m_iSegments <= 0 )
		return;

	if( m_iSegments > 1 )
		SetSegmentAngles( m_pSegments[0], m_pSegments[1] );

	TraceResult tr;

	if( m_bObjectAttached )
	{
		for( int iSeg = 1; iSeg < m_iNumSamples; iSeg++ )
		{
			CRopeSegment* pSegment = m_pSegments[iSeg];

			Vector vecDist = pSegment->m_Data.mPosition - m_pSegments[iSeg]->GetAbsOrigin();
			
			vecDist = vecDist.Normalize();

			const float flTraceDist = ( iSeg - m_iAttachedObjectsSegment + 2 ) < 5 ? 50 : 10;

			const Vector vecTraceDist = vecDist * flTraceDist;

			const Vector vecEnd = pSegment->m_Data.mPosition + vecTraceDist;

			UTIL_TraceLine( pSegment->GetAbsOrigin(), vecEnd, ignore_monsters, edict(), &tr );
			
			if( tr.flFraction == 1.0 && tr.fAllSolid )
			{
				break;
			}

			if( tr.flFraction != 1.0 || tr.fStartSolid || !tr.fInOpen )
			{
				Vector vecOrigin = tr.vecEndPos - vecTraceDist;

				TruncateEpsilon( vecOrigin );

				pSegment->SetAbsOrigin( vecOrigin );
				
				Vector vecNormal = tr.vecPlaneNormal.Normalize() * 20000.0;

				RopeSampleData& data = pSegment->m_Data;

				data.mApplyExternalForce = true;
				data.mExternalForce = vecNormal;
				data.mVelocity = g_vecZero;
			}
			else
			{
				Vector vecOrigin = pSegment->m_Data.mPosition;

				TruncateEpsilon( vecOrigin );

				pSegment->SetAbsOrigin( vecOrigin );
			}
		}
	}
	else
	{
		for( int iSeg = 1; iSeg < m_iNumSamples; iSeg++ )
		{
			CRopeSegment* pSegment = m_pSegments[iSeg];

			UTIL_TraceLine( pSegment->GetAbsOrigin(), pSegment->m_Data.mPosition, ignore_monsters, edict(), &tr );

			if( tr.flFraction == 1.0 )
			{
				Vector vecOrigin = pSegment->m_Data.mPosition;

				TruncateEpsilon( vecOrigin );

				pSegment->SetAbsOrigin( vecOrigin );
			}
			else
			{
				const Vector vecNormal = tr.vecPlaneNormal.Normalize();

				Vector vecOrigin = tr.vecEndPos + vecNormal * 10.0;

				TruncateEpsilon( vecOrigin );

				pSegment->SetAbsOrigin( vecOrigin );
				pSegment->m_Data.mApplyExternalForce = true;
				pSegment->m_Data.mExternalForce = vecNormal * 40000.0;
			}
		}
	}

	Vector vecAngles;

	for( int iSeg = 1; iSeg < m_iNumSamples; iSeg++ )
	{
		SetSegmentAngles( m_pSegments[iSeg - 1], m_pSegments[iSeg] );
	}

	if( m_iSegments > 1 )
	{
		CRopeSegment *pSegment = m_pSegments[m_iNumSamples - 1];

		UTIL_TraceLine( m_vecLastEndPos, pSegment->m_Data.mPosition, ignore_monsters, edict(), &tr );
	
		if( tr.flFraction == 1.0 )
		{
			m_vecLastEndPos = pSegment->m_Data.mPosition;
		}
		else
		{
			m_vecLastEndPos = tr.vecEndPos;
			pSegment->m_Data.mApplyExternalForce = true;
			pSegment->m_Data.mExternalForce = tr.vecPlaneNormal.Normalize() * 40000.0;
		}
	}
}

bool CRope :: MoveUp( const float flDeltaTime )
{
	if( m_iAttachedObjectsSegment > ROPE_IGNORE_SAMPLES )
	{
		float flDistance = flDeltaTime * 128.0f;

		while( 1 )
		{
			float flOldDist = flDistance;

			flDistance = 0;

			if( flOldDist <= 0 )
				break;

			if( m_iAttachedObjectsSegment <= 3 )
				break;

			if( flOldDist > m_flAttachedObjectsOffset )
			{
				flDistance = flOldDist - m_flAttachedObjectsOffset;

				m_iAttachedObjectsSegment--;

				float flNewOffset = 0.0f;

				if( m_iAttachedObjectsSegment < m_iSegments )
					flNewOffset = m_pSegments[m_iAttachedObjectsSegment]->m_Data.restLength;
				m_flAttachedObjectsOffset = flNewOffset;
			}
			else
			{
				m_flAttachedObjectsOffset -= flOldDist;
			}
		}
	}

	return true;
}

bool CRope :: MoveDown( const float flDeltaTime )
{
	if( !m_bObjectAttached )
		return false;

	float flDistance = flDeltaTime * 128.0f;
	bool bDoIteration = true;
	bool bOnRope = true;

	while( bDoIteration )
	{
		bDoIteration = false;

		if( flDistance > 0.0f )
		{
			float flNewDist = flDistance;
			float flSegLength = 0.0f;

			while( bOnRope )
			{
				if( m_iAttachedObjectsSegment < m_iSegments )
					flSegLength = m_pSegments[m_iAttachedObjectsSegment]->m_Data.restLength;

				const float flOffset = flSegLength - m_flAttachedObjectsOffset;

				if( flNewDist <= flOffset )
				{
					m_flAttachedObjectsOffset += flNewDist;
					flDistance = 0;
					bDoIteration = true;
					break;
				}

				if( m_iAttachedObjectsSegment + 1 == m_iSegments )
					bOnRope = false;
				else m_iAttachedObjectsSegment++;

				flNewDist -= flOffset;
				flSegLength = 0;

				m_flAttachedObjectsOffset = 0;

				if( flNewDist <= 0 )
					break;
			}
		}
	}

	return bOnRope;
}

Vector CRope :: GetAttachedObjectsVelocity( void ) const
{
	if( !m_bObjectAttached )
		return g_vecZero;

	return m_pSegments[m_iAttachedObjectsSegment]->m_Data.mVelocity;
}

void CRope :: ApplyForceFromPlayer( const Vector& vecForce )
{
	if( !m_bObjectAttached )
		return;

	float flForce = 20000.0;

	if( m_iSegments < 26 )
		flForce *= ( m_iSegments / 26.0 );

	const Vector vecScaledForce = vecForce * flForce;

	ApplyForceToSegment( vecScaledForce, m_iAttachedObjectsSegment );
}

void CRope :: ApplyForceToSegment( const Vector& vecForce, const int iSegment )
{
	if( iSegment < m_iSegments )
	{
		m_pSegments[iSegment]->ApplyExternalForce( vecForce );
	}
	else if( iSegment == m_iSegments )
	{
		// Apply force to the last sample.
		m_pSegments[iSegment - 1]->ApplyExternalForce( vecForce );
	}
}

void CRope :: AttachObjectToSegment( CRopeSegment* pSegment )
{
	SetAttachedObjectsSegment( pSegment );

	m_flAttachedObjectsOffset = 0;
	m_bObjectAttached = true;
	m_flDetachTime = 0;
}

void CRope :: DetachObject( void )
{
	m_flDetachTime = gpGlobals->time;
	m_bObjectAttached = false;
}

bool CRope :: IsAcceptingAttachment( void ) const
{
	if( gpGlobals->time - m_flDetachTime > 2.0 && !m_bObjectAttached )
		return !m_bDisallowPlayerAttachment;
	return false;
}

bool CRope :: ShouldCreak( void ) const
{
	if( m_bObjectAttached && m_bMakeSound )
	{
		RopeSampleData& data = m_pSegments[m_iAttachedObjectsSegment]->m_Data;

		if( data.mVelocity.Length() > 20.0 )
			return RANDOM_LONG( 1, 5 ) == 1;
	}

	return false;
}

float CRope :: GetSegmentLength( int iSegmentIndex ) const
{
	if( iSegmentIndex < m_iSegments )
		return m_pSegments[iSegmentIndex]->m_Data.restLength;
	return 0;
}

float CRope :: GetRopeLength( void ) const
{
	float flLength = 0;

	for( int i = 0; i < m_iSegments; i++ )
		flLength += m_pSegments[i]->m_Data.restLength;

	return flLength;
}

Vector CRope :: GetRopeOrigin( void ) const
{
	return m_pSegments[0]->m_Data.mPosition;
}

bool CRope :: IsValidSegmentIndex( const int iSegment ) const
{
	return iSegment < m_iSegments;
}

Vector CRope :: GetSegmentOrigin( const int iSegment ) const
{
	if( !IsValidSegmentIndex( iSegment ))
		return g_vecZero;

	return m_pSegments[iSegment]->m_Data.mPosition;
}

Vector CRope :: GetSegmentAttachmentPoint( const int iSegment ) const
{
	if( !IsValidSegmentIndex( iSegment ) )
		return g_vecZero;

	Vector vecOrigin, vecAngles;

	if( m_bSimulateBones )
	{
		return m_pSegments[iSegment]->pev->origin;
	}

	CRopeSegment* pSegment = m_pSegments[iSegment];

	pSegment->GetAttachment( 0, vecOrigin, vecAngles );

	return vecOrigin;
}

void CRope :: SetAttachedObjectsSegment( CRopeSegment* pSegment )
{
	for( int i = 0; i < m_iSegments; i++ )
	{
		if( m_pSegments[i] == pSegment )
		{
			m_iAttachedObjectsSegment = i;
			break;
		}
	}
}

Vector CRope :: GetSegmentDirFromOrigin( const int iSegmentIndex ) const
{
	if( iSegmentIndex >= m_iSegments )
		return g_vecZero;

	// there is one more sample than there are segments, so this is fine.
	const Vector vecResult = m_pSegments[iSegmentIndex + 1]->m_Data.mPosition - m_pSegments[iSegmentIndex]->m_Data.mPosition;

	return vecResult.Normalize();
}

Vector CRope :: GetAttachedObjectsPosition( void ) const
{
	if( !m_bObjectAttached )
		return g_vecZero;

	Vector vecResult;

	if( m_iAttachedObjectsSegment < m_iSegments )
		vecResult = m_pSegments[m_iAttachedObjectsSegment]->m_Data.mPosition;

	vecResult = vecResult + ( m_flAttachedObjectsOffset * GetSegmentDirFromOrigin( m_iAttachedObjectsSegment ));

	return vecResult;
}
