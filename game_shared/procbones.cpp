//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "mathlib.h"
#include "studio.h"

//-----------------------------------------------------------------------------
// Purpose: look at single column vector of another bones local transformation 
//	  and generate a procedural transformation based on how that column 
//	  points down the 6 cardinal axis (all negative weights are clamped to 0).
//-----------------------------------------------------------------------------
bool DoAxisInterpBone( mstudioaxisinterpbone_t *pProc, mstudiobone_t *pbones, int iBone, matrix3x4 *bonetransform )
{
	Vector	control;

	if( pbones[pProc->control].parent != -1 ) // invert it back into parent's space.
		control = bonetransform[pbones[pProc->control].parent].VectorIRotate( bonetransform[iBone][pProc->axis] );
	else control = bonetransform[iBone][pProc->axis];

	Vector4D	*q1, *q2, *q3;
	Vector	*p1, *p2, *p3;

	// find axial control inputs
	float a1 = control.x;
	float a2 = control.y;
	float a3 = control.z;

	if( a1 >= 0.0f ) 
	{ 
		q1 = &pProc->quat[0];
		p1 = &pProc->pos[0];
	} 
	else 
	{ 
		a1 = -a1; 
		q1 = &pProc->quat[1];
		p1 = &pProc->pos[1];
	}

	if( a2 >= 0.0f ) 
	{ 
		q2 = &pProc->quat[2]; 
		p2 = &pProc->pos[2];
	} 
	else 
	{ 
		a2 = -a2; 
		q2 = &pProc->quat[3]; 
		p2 = &pProc->pos[3];
	}

	if( a3 >= 0.0f ) 
	{ 
		q3 = &pProc->quat[4]; 
		p3 = &pProc->pos[4];
	} 
	else 
	{ 
		a3 = -a3; 
		q3 = &pProc->quat[5]; 
		p3 = &pProc->pos[5];
	}

	Vector4D	v, tmp;
	Vector	p = g_vecZero;

	// do a three-way blend
	if( a1 + a2 > 0.0f )
	{
		float t = 1.0 / (a1 + a2 + a3);

		// FIXME: do a proper 3-way Quat blend!
		QuaternionSlerp( *q2, *q1, a1 / (a1 + a2), tmp );
		QuaternionSlerp( tmp, *q3, a3 * t, v );
		p += *p1 * ( a1 * t );
		p += *p2 * ( a2 * t );
		p += *p3 * ( a3 * t );

		bonetransform[iBone] = bonetransform[pbones[iBone].parent].ConcatTransforms( matrix3x4( p, v ));

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Generate a procedural transformation based on how that another bones 
//			local transformation matches a set of target orientations.
//-----------------------------------------------------------------------------
bool DoQuatInterpBone( mstudioquatinterpbone_t *pProc, mstudiobone_t *pbones, mstudioquatinterpinfo_t *pTrigger, int iBone, matrix3x4 *bonetransform )
{
	matrix3x4	bonematrix;

	if( pbones[pProc->control].parent != -1 )
	{
		Vector4D	src;
		float	weight[32];	// !!! MAXSTUDIOBONETRIGGERS
		float	scale = 0.0f;
		Vector4D	quat;
		Vector	pos;
		int	i;

		matrix3x4	tmpmatrix = bonetransform[pbones[pProc->control].parent].Invert();
		matrix3x4	controlmatrix = tmpmatrix.ConcatTransforms( bonetransform[pProc->control] );

		src = controlmatrix.GetQuaternion();

		for( i = 0; i < pProc->numtriggers; i++ )
		{
			float dot = fabs( DotProduct( pTrigger[i].trigger, src ));

			// FIXME: a fast acos should be acceptable
			dot = bound( -1.0f, dot, 1.0f );
			weight[i] = 1.0f - ( 2.0f * acos( dot ) * pTrigger[i].inv_tolerance );
			weight[i] = Q_max( 0.0f, weight[i] );
			scale += weight[i];
		}

		if( scale <= 0.001f )  // EPSILON?
			return false;

		for( i = 0; i < pProc->numtriggers; i++ )
		{
			if( weight[i] != 0.0f )
				break;
		}

		// triggers are not triggered
		if( i == pProc->numtriggers )
			return false;

		scale = 1.0f / scale;
		quat.Init();
		pos.Init();

		for( i = 0; i < pProc->numtriggers; i++ )
		{
			if( weight[i] == 0.0f )
				continue;

			float s = weight[i] * scale;

			QuaternionAlign( pTrigger[i].quat, quat, quat );

			// g-cont. why valve don't use slerp here?..
			quat.x = quat.x + s * pTrigger[i].quat.x;
			quat.y = quat.y + s * pTrigger[i].quat.y;
			quat.z = quat.z + s * pTrigger[i].quat.z;
			quat.w = quat.w + s * pTrigger[i].quat.w;
			pos.x = pos.x + s * pTrigger[i].pos.x;
			pos.y = pos.y + s * pTrigger[i].pos.y;
			pos.z = pos.z + s * pTrigger[i].pos.z;
		}

		quat = quat.Normalize();	// g-cont. is this really needs?
		bonematrix = matrix3x4( pos, quat );
		bonetransform[iBone] = bonetransform[pbones[iBone].parent].ConcatTransforms( bonematrix );

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Generate a procedural transformation so that one bone points at
//			another point on the model
//-----------------------------------------------------------------------------
void DoAimAtBone( mstudioaimatbone_t *pProc, Vector4D &q1, mstudiobone_t *pbones, int iBone, matrix3x4 *bonetransform, const studiohdr_t *pStudioHdr )
{
	// The world matrix of the bone to change
	matrix3x4 boneMatrix;

	// Guaranteed to be unit length
	const Vector &userAimVector = pProc->aimvector;

	// Guaranteed to be unit length
	const Vector &userUpVector = pProc->upvector;

	// Get to get position of bone but also for up reference
	matrix3x4 parentSpace = bonetransform[pProc->parent];

	// World space position of the bone to aim
	Vector aimWorldPosition = parentSpace.VectorTransform( pProc->basepos );

	// The worldspace pos to aim at
	Vector aimAtWorldPosition;

	if( pStudioHdr )
	{
		// This means it's AIMATATTACH
		mstudioattachment_t *pattachment = (mstudioattachment_t *) ((byte *)pStudioHdr + pStudioHdr->attachmentindex) + pProc->aim;
		aimAtWorldPosition = bonetransform[pattachment->bone].VectorTransform( pattachment->org );
	}
	else
	{
		aimAtWorldPosition = bonetransform[pProc->aim].GetOrigin();
	}

	// The aim and up data is relative to this bone, not the parent bone
	matrix3x4 bonematrix, boneLocalToWorld;

	bonematrix = matrix3x4( pProc->basepos, q1 );
	boneLocalToWorld = bonetransform[pProc->parent].ConcatTransforms( bonematrix );

	Vector aimVector = (aimAtWorldPosition - aimWorldPosition).Normalize();

	Vector axis = CrossProduct( userAimVector, aimVector ).Normalize();
	float angle( acosf( DotProduct( userAimVector, aimVector )));
	Vector4D aimRotation;

	AxisAngleQuaternion( axis, RAD2DEG( angle ), aimRotation );

	if(( 1.0f - fabs( DotProduct( userUpVector, userAimVector ))) > FLT_EPSILON )
	{
		matrix3x4	aimRotationMatrix = matrix3x4( g_vecZero, aimRotation );
		Vector tmp_pUp = aimRotationMatrix.VectorRotate( userUpVector );
		Vector tmpV = aimVector * DotProduct( aimVector, tmp_pUp );
		Vector pUp = (tmp_pUp - tmpV).Normalize();

		Vector tmp_pParentUp = boneLocalToWorld.VectorRotate( userUpVector );
		tmpV = aimVector * DotProduct( aimVector, tmp_pParentUp );
		Vector pParentUp = (tmp_pParentUp - tmpV).Normalize();
		Vector4D upRotation;

		if( 1.0f - fabs( DotProduct( pUp, pParentUp )) > FLT_EPSILON )
		{
			angle = acos( DotProduct( pUp, pParentUp ));
			axis = CrossProduct( pUp, pParentUp );			
		}
		else
		{
			angle = 0;
			axis = pUp;
		}

		axis = axis.Normalize();
		AxisAngleQuaternion( axis, RAD2DEG( angle ), upRotation );
		Vector4D	boneRotation;

		QuaternionMult( upRotation, aimRotation, boneRotation );
		boneMatrix = matrix3x4( aimWorldPosition, boneRotation );
	}
	else
	{
		boneMatrix = matrix3x4( aimWorldPosition, aimRotation );
	}

	bonetransform[iBone] = boneMatrix;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CalcProceduralBone( const studiohdr_t *pStudioHdr, int iBone, matrix3x4 *bonetransform )
{
	if( !FBitSet( pStudioHdr->flags, STUDIO_HAS_BONEINFO ))
		return false; // info about procedural bones is absent

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)pStudioHdr + pStudioHdr->boneindex);
	mstudioboneinfo_t *pinfo = (mstudioboneinfo_t *)((byte *)pbones + pStudioHdr->numbones * sizeof( mstudiobone_t ));
	mstudioaxisinterpbone_t *pProcAxis;
	mstudioquatinterpbone_t *pProcQuat;
	mstudioquatinterpinfo_t *pTrigger;
	mstudioaimatbone_t *pProcAimAt;

	if( FBitSet( pbones[iBone].flags, BONE_ALWAYS_PROCEDURAL ) && pinfo[iBone].procindex )
	{
		Vector4D	quat = pinfo[iBone].quat;

		switch( pinfo[iBone].proctype )
		{
		case STUDIO_PROC_AXISINTERP:
			pProcAxis = (mstudioaxisinterpbone_t *)((byte *)pStudioHdr + pinfo[iBone].procindex);
			if( DoAxisInterpBone( pProcAxis, pbones, iBone, bonetransform ))
				return true;
			break;
		case STUDIO_PROC_QUATINTERP:
			pProcQuat = (mstudioquatinterpbone_t *)((byte *)pStudioHdr + pinfo[iBone].procindex);
			pTrigger = (mstudioquatinterpinfo_t *)((byte *)pStudioHdr + pProcQuat->triggerindex);
			if( DoQuatInterpBone( pProcQuat, pbones, pTrigger, iBone, bonetransform ))
				return true;
			break;
		case STUDIO_PROC_AIMATBONE:
			pProcAimAt = (mstudioaimatbone_t *)((byte *)pStudioHdr + pinfo[iBone].procindex);
			DoAimAtBone( pProcAimAt, quat, pbones, iBone, bonetransform, NULL );
			return true;
		case STUDIO_PROC_AIMATATTACH:
			pProcAimAt = (mstudioaimatbone_t *)((byte *)pStudioHdr + pinfo[iBone].procindex);
			DoAimAtBone( pProcAimAt, quat, pbones, iBone, bonetransform, pStudioHdr );
			return true;
		default:
			return false;
		}
	}

	return false;
}
