/*
bone_setup.cpp - shared code for setup studio bones
This file is part of XashNT engine
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mathlib.h"
#include "const.h"
#include <studio.h>
#include "com_model.h"
#include "stringlib.h"
#include "bs_defs.h"
#include "ikcontext.h"
#include "iksolver.h"

//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: ExtractAnimValue( int frame, const mstudioanimvalue_t *panimvalue, float scale, float &v1, float &v2 )
{
	if( !panimvalue )
	{
		v1 = v2 = 0.0f;
		return;
	}

	// avoids a crash reading off the end of the data
	// g-cont. this solution is coming from Source 2007 and has no changes in Source 2013
	if(( panimvalue->num.total == 1 ) && ( panimvalue->num.valid == 1 ))
	{
		v1 = v2 = panimvalue[1].value * scale;
		return;
	}

	int k = frame;

	// find the data list that has the frame
	while( panimvalue->num.total <= k )
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;

		if( panimvalue->num.total == 0 )
		{
			debugMsg( "^3Error:^7 ExtractAnimValue: num.total == 0!\n" );
			v1 = v2 = 0.0f;
			return;
		}
	}

	// Bah, missing blend!
	if( panimvalue->num.valid > k )
	{
		// has valid animation data
		v1 = panimvalue[k+1].value * scale;

		if( panimvalue->num.valid > k + 1 )
		{
			// has valid animation blend data
			v2 = panimvalue[k+2].value * scale;
		}
		else
		{
			if( panimvalue->num.total > k + 1 ) v2 = v1; // data repeats, no blend
			else v2 = panimvalue[panimvalue->num.valid+2].value * scale; // pull blend from first data block in next list
		}
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;

		if( panimvalue->num.total > k + 1 ) v2 = v1; // data repeats, no blend
		else v2 = panimvalue[panimvalue->num.valid + 2].value * scale; // pull blend from first data block in next list
	}
}

//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone (usefully to decompress IK errors)
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: ExtractAnimValue( int frame, const mstudioanimvalue_t *panimvalue, float scale, float &v1 )
{
	if( !panimvalue )
	{
		v1 = 0.0f;
		return;
	}

	int k = frame;

	while( panimvalue->num.total <= k )
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;

		if( panimvalue->num.total == 0 )
		{
			debugMsg( "^3Error:^7 ExtractAnimValue: num.total == 0!\n" );
			v1 = 0.0f;
			return;
		}
	}

	// Bah, missing blend!
	if( panimvalue->num.valid > k )
	{
		v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

void CStudioBoneSetup :: AdjustBoneAngles( mstudiobone_t *pbone, Radian &angles1, Radian &angles2 )
{
	if( m_flBoneControllers == NULL )
		return;

	for( int j = 0; j < 3; j++ )
	{
		if( pbone->bonecontroller[j+3] != -1 )
		{
			angles1[j] += m_flBoneControllers[pbone->bonecontroller[j+3]];
			angles2[j] += m_flBoneControllers[pbone->bonecontroller[j+3]];
		}
	}
}

void CStudioBoneSetup :: AdjustBoneOrigin( mstudiobone_t *pbone, Vector &origin )
{
	if( m_flBoneControllers == NULL )
		return;

	for( int j = 0; j < 3; j++ )
	{
		if( pbone->bonecontroller[j] != -1 )
		{
			origin[j] += m_flBoneControllers[pbone->bonecontroller[j]];
		}
	}
}

Vector4D CStudioBoneSetup :: CalcBoneQuaternion( int frame, float s, int flags, mstudiobone_t *pbone, mstudioboneinfo_t *pinfo, mstudioanim_t *panim )
{
	Radian	angles1, angles2;
	Vector4D	q1, q2, q;

	if( !FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO ))
		pinfo = NULL;

	if( s > 0.001f )
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 3 ), pbone->scale[3], angles1.x, angles2.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 4 ), pbone->scale[4], angles1.y, angles2.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 5 ), pbone->scale[5], angles1.z, angles2.z );

		if( !FBitSet( flags, STUDIO_DELTA ))
		{
			angles1.x = angles1.x + pbone->value[3];
			angles1.y = angles1.y + pbone->value[4];
			angles1.z = angles1.z + pbone->value[5];
			angles2.x = angles2.x + pbone->value[3];
			angles2.y = angles2.y + pbone->value[4];
			angles2.z = angles2.z + pbone->value[5];
		}

		AdjustBoneAngles( pbone, angles1, angles2 );

		if( angles1 != angles2 )
		{
			AngleQuaternion( angles1, q1 );
			AngleQuaternion( angles2, q2 );
			QuaternionBlend( q1, q2, s, q );
		}
		else AngleQuaternion( angles1, q );
	}
	else
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 3 ), pbone->scale[3], angles1.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 4 ), pbone->scale[4], angles1.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 5 ), pbone->scale[5], angles1.z );
		angles2 = g_radZero; // dummy

		if( !FBitSet( flags, STUDIO_DELTA ))
		{
			angles1.x = angles1.x + pbone->value[3];
			angles1.y = angles1.y + pbone->value[4];
			angles1.z = angles1.z + pbone->value[5];
		}

		AdjustBoneAngles( pbone, angles1, angles2 );

		AngleQuaternion( angles1, q );
	}

	// align to unified bone
	if( !FBitSet( flags, STUDIO_DELTA ) && FBitSet( pbone->flags, BONE_FIXED_ALIGNMENT ) && ( pinfo != NULL ))
	{
		QuaternionAlign( pinfo->qAlignment, q, q );
	}

	return q;
}

//-----------------------------------------------------------------------------
// Purpose: return a sub frame position for a single bone
//-----------------------------------------------------------------------------
Vector CStudioBoneSetup :: CalcBonePosition( int frame, float s, int flags, mstudiobone_t *pbone, mstudioanim_t *panim )
{
	Vector	origin1, origin2;
	Vector	pos;

	if( s > 0.001f )
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 0 ), pbone->scale[0], origin1.x, origin2.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 1 ), pbone->scale[1], origin1.y, origin2.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 2 ), pbone->scale[2], origin1.z, origin2.z );

		if( origin1 != origin2 )
		{
			InterpolateOrigin( origin1, origin2, pos, s );
		}
		else pos = origin1;
	}
	else
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 0 ), pbone->scale[0], origin1.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 1 ), pbone->scale[1], origin1.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 2 ), pbone->scale[2], origin1.z );

		pos = origin1;
	}

	if( !FBitSet( flags, STUDIO_DELTA ))
	{
		pos.x = pos.x + pbone->value[0];
		pos.y = pos.y + pbone->value[1];
		pos.z = pos.z + pbone->value[2];
	}

	AdjustBoneOrigin( pbone, pos );

	return pos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcIKError( const mstudioikerror_t *panim, int frame, float s, Vector &pos, Vector4D &q )
{
	Radian	angles1, angles2;
	Vector	origin1, origin2;
	Vector4D	q1, q2;

	if( s > 0.0001f )
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 0 ), panim->scale[0], origin1.x, origin2.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 1 ), panim->scale[1], origin1.y, origin2.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 2 ), panim->scale[2], origin1.z, origin2.z );

		if( origin1 != origin2 )
		{
			InterpolateOrigin( origin1, origin2, pos, s );
		}
		else pos = origin1;

		ExtractAnimValue( frame, pAnimvalue( panim, 3 ), panim->scale[3], angles1.x, angles2.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 4 ), panim->scale[4], angles1.y, angles2.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 5 ), panim->scale[5], angles1.z, angles2.z );

		if( angles1 != angles2 )
		{
			AngleQuaternion( angles1, q1 );
			AngleQuaternion( angles2, q2 );
			QuaternionBlend( q1, q2, s, q );
		}
		else AngleQuaternion( angles1, q );
	}
	else
	{
		ExtractAnimValue( frame, pAnimvalue( panim, 0 ), panim->scale[0], origin1.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 1 ), panim->scale[1], origin1.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 2 ), panim->scale[2], origin1.z );

		pos = origin1;

		ExtractAnimValue( frame, pAnimvalue( panim, 3 ), panim->scale[3], angles1.x );
		ExtractAnimValue( frame, pAnimvalue( panim, 4 ), panim->scale[4], angles1.y );
		ExtractAnimValue( frame, pAnimvalue( panim, 5 ), panim->scale[5], angles1.z );

		AngleQuaternion( angles1, q );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
mstudioanim_t *CStudioBoneSetup :: GetAnimSourceData( mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	return NULL; // base implementation can't lookup for sequence groups
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
mstudioanim_t *CStudioBoneSetup :: FetchAnimation( mstudioseqdesc_t *pseqdesc, int animation )
{
	mstudioanim_t *panim = (mstudioanim_t *)GetAnimSourceData( pseqdesc );

	if( animation < 0 || animation > ( pseqdesc->numblends - 1 ))
		return panim;

	panim += animation * m_pStudioHeader->numbones;

	return panim;
}

//-----------------------------------------------------------------------------
// Purpose: returns animation description. Notice: may return NULL
//-----------------------------------------------------------------------------
mstudioanimdesc_t *CStudioBoneSetup :: FetchAnimDesc( mstudioseqdesc_t *pseqdesc, int animation )
{
	static mstudioanimdesc_t baseDesc; // for backward compatibility

	if( pseqdesc->animdescindex <= 0 || pseqdesc->animdescindex >= m_pStudioHeader->length )
	{
		Q_strncpy( baseDesc.label, pseqdesc->label, sizeof( baseDesc.label ));
		baseDesc.numframes = pseqdesc->numframes;
		baseDesc.flags = pseqdesc->flags;
		baseDesc.fps = pseqdesc->fps;

		return &baseDesc;
	}

	mstudioanimdesc_t *panimdesc = (mstudioanimdesc_t *)((byte *)m_pStudioHeader + pseqdesc->animdescindex);

	if( animation < 0 || animation > ( pseqdesc->numblends - 1 ))
		return panimdesc; // pointer to first anim description

	panimdesc += animation;

	return panimdesc;
}

//-----------------------------------------------------------------------------
// Purpose: Find and decode a sub-frame of animation
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcAnimation( Vector pos[], Vector4D q[], mstudioseqdesc_t *pseqdesc, int animation, float cycle )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	mstudioanimdesc_t	*animdesc = FetchAnimDesc( pseqdesc, animation );
	mstudioanim_t	*panim = FetchAnimation( pseqdesc, animation );
	mstudioboneinfo_t	*pboneinfo = NULL;

	if( FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO ))
		pboneinfo = (mstudioboneinfo_t *)((byte *)pbone + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

	float fFrame = cycle * (animdesc->numframes - 1);
	int iFrame = (int)fFrame;
	float s = (fFrame - iFrame); // cut fractional part

	const float *pweight = pBoneweight( pseqdesc );

	// BUGBUG: the sequence, the anim, and the model can have all different bone mappings.
	for( int i = 0; i < m_pStudioHeader->numbones; i++, pbone++, pboneinfo++, panim++ )
	{
		if( pweight[i] <= 0.0f || !IsBoneUsed( pbone ))
			continue;

		q[i] = CalcBoneQuaternion( iFrame, s, animdesc->flags, pbone, pboneinfo, panim );
		pos[i] = CalcBonePosition( iFrame, s, animdesc->flags, pbone, panim );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Inter-animation blend.  Assumes both types are identical.
//	  blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//	  0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: BlendBones( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	const float	*pweight = pBoneweight( pseqdesc );
	int		i;

	if( s <= 0.0f )
	{
		return;
	}
	else if( s >= 1.0 )
	{
		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			if( pweight[i] <= 0.0f || !IsBoneUsed( pbone + i ))
				continue;

			pos1[i] = pos2[i];
			q1[i] = q2[i];
		}
		return;
	}

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pweight[i] > 0.0f )
		{
			if( FBitSet( pbone[i].flags, BONE_FIXED_ALIGNMENT ))
				QuaternionBlendNoAlign( q1[i], q2[i], s, q1[i] );
			else QuaternionBlend( q1[i], q2[i], s, q1[i] );
			InterpolateOrigin( pos1[i], pos2[i], pos1[i], s );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: SlerpBones( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	const float	*pweight = pBoneweight( pseqdesc );
	float		s2;

	if( s <= 0.0f ) 
	{
		return;
	}
	else if( s > 1.0f )
	{
		s = 1.0f;		
	}

	if( FBitSet( pseqdesc->flags, STUDIO_WORLD ))
	{
		WorldSpaceSlerp( q1, pos1, pseqdesc, q2, pos2, s );
		return;
	}

	if( FBitSet( pseqdesc->flags, STUDIO_DELTA ))
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// skip unused bones
			if( !IsBoneUsed( pbone + i ))
				continue;

			s2 = s * pweight[i];	// blend in based on this bones weight
			if( s2 <= 0.0f ) continue;

			if( FBitSet( pseqdesc->flags, STUDIO_POST ))
			{
				QuaternionMA( q1[i], s2, q2[i], q1[i] );
				// FIXME: are these correct?
				pos1[i] = pos1[i] + pos2[i] * s2;
			}
			else
			{
				QuaternionSM( s2, q2[i], q1[i], q1[i] );
				// FIXME: are these correct?
				pos1[i] = pos1[i] + pos2[i] * s2;
			}
		}
	}
	else
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// skip unused bones
			if( !IsBoneUsed( pbone + i ))
				continue;

			s2 = s * pweight[i];	// blend in based on this bones weight
			if( s2 <= 0.0f ) continue;

			if( FBitSet( pbone[i].flags, BONE_FIXED_ALIGNMENT ))
				QuaternionSlerpNoAlign( q1[i], q2[i], s2, q1[i] );
			else QuaternionSlerp( q1[i], q2[i], s2, q1[i] );
			InterpolateOrigin( pos1[i], pos2[i], pos1[i], s2 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: blend together in world space q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: WorldSpaceSlerp( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	const float	*pweight = pBoneweight( pseqdesc );
	float		s1; // weight of parent for q2, pos2
	float		s2; // weight for q2, pos2

	// make fake root transform
	matrix3x4	rootXform;

	rootXform.Identity();

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		// skip unused bones
		if( !IsBoneUsed( pbone + i ))
			continue;

		int n = pbone[i].parent;
		s1 = 0.0f;

		s2 = s * pweight[i];	// blend in based on this bones weight
		if( n != -1 ) s1 = s * pweight[n];

		if( s1 == 1.0f && s2 == 1.0f )
		{
			pos1[i] = pos2[i];
			q1[i] = q2[i];
		}
		else if( s2 > 0.0f )
		{
			Vector4D srcQ, dstQ;
			Vector srcPos, dstPos;
			Vector4D targetQ;
			Vector targetPos;

			BuildBoneChain( rootXform, pos1, q1, i, dstBoneToWorld );
			BuildBoneChain( rootXform, pos2, q2, i, srcBoneToWorld );

			srcQ = srcBoneToWorld[i].GetQuaternion();
			dstQ = dstBoneToWorld[i].GetQuaternion();
			srcPos = srcBoneToWorld[i].GetOrigin();
			dstPos = dstBoneToWorld[i].GetOrigin();

			QuaternionSlerp( dstQ, srcQ, s2, targetQ );
			targetBoneToWorld[i] = matrix3x4( dstPos, targetQ );

			// back solve
			if( n == -1 )
			{
				q1[i] = targetBoneToWorld[i].GetQuaternion();
			}
			else
			{
				matrix3x4	worldToBone = targetBoneToWorld[n].Invert();
				matrix3x4	local = worldToBone.ConcatTransforms( targetBoneToWorld[i] );
				q1[i] = local.GetQuaternion();

				// blend bone lengths (local space)
				InterpolateOrigin( pos1[i], pos2[i], pos1[i], s2 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: build boneToWorld transforms for a specific bone
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: BuildBoneChain( const matrix3x4 &rootxform, const Vector pos[], const Vector4D q[], int iBone, matrix3x4 *pBoneToWorld, byte *pBoneSet )
{
	mstudiobone_t	*pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	matrix3x4		bonematrix = matrix3x4( pos[iBone], q[iBone] );
	int		iParent = pbones[iBone].parent;

	if( pBoneSet && pBoneSet[iBone] )
		return;

	if( iParent == -1 ) 
	{
		pBoneToWorld[iBone] = rootxform.ConcatTransforms( bonematrix );
	}
	else
	{
		// evil recursive!!!
		BuildBoneChain( rootxform, pos, q, iParent, pBoneToWorld, pBoneSet );
		pBoneToWorld[iBone] = pBoneToWorld[iParent].ConcatTransforms( bonematrix );
	}

	if( pBoneSet ) pBoneSet[iBone] = 1;
}

//-----------------------------------------------------------------------------
// Purpose: turn a specific bones boneToWorld transform into a pos and q in parents bonespace
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: SolveBone( int iBone, matrix3x4 *pBoneToWorld, Vector pos[], Vector4D q[] )
{
	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	int iParent = pbones[iBone].parent;

	matrix3x4	worldToBone = pBoneToWorld[iParent].Invert();
	matrix3x4	local = worldToBone.ConcatTransforms( pBoneToWorld[iBone] );

	q[iBone] = local.GetQuaternion();
	pos[iBone] = local.GetOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: for a 2 bone chain, find the IK solution and reset the matrices
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: SolveIK( const mstudioikchain_t *pikchain, Vector &targetFoot, matrix3x4 *pBoneToWorld )
{
	const mstudioiklink_t *link0 = pIKLink( pikchain, 0 );
	const mstudioiklink_t *link1 = pIKLink( pikchain, 1 );
	const mstudioiklink_t *link2 = pIKLink( pikchain, 2 );

	if( link0->kneeDir.LengthSqr() > 0.0f )
	{
		Vector targetKneeDir, targetKneePos;
		// FIXME: knee length should be as long as the legs
		targetKneeDir = pBoneToWorld[link0->bone].VectorRotate( link0->kneeDir );
		targetKneePos = pBoneToWorld[link1->bone].GetOrigin();

		return SolveIK( link0->bone, link1->bone, link2->bone, targetFoot, targetKneePos, targetKneeDir, pBoneToWorld );
	}

	return SolveIK( link0->bone, link1->bone, link2->bone, targetFoot, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Solve Knee position for a known hip and foot location, but no specific knee direction preference
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: SolveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, matrix3x4 *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	worldThigh = pBoneToWorld[iThigh].GetOrigin();
	worldKnee = pBoneToWorld[iKnee].GetOrigin();
	worldFoot = pBoneToWorld[iFoot].GetOrigin();

	// debugLine( worldThigh, worldKnee, 0, 0, 255, true, 0 );
	// debugLine( worldKnee, worldFoot, 0, 0, 255, true, 0 );

	Vector ikFoot, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = worldKnee - worldThigh;

	float l1 = (worldKnee - worldThigh).Length();
	float l2 = (worldFoot - worldKnee).Length();
	float l3 = (worldFoot - worldThigh).Length();

	// leg too straight to figure out knee?
	if( l3 > (l1 + l2) * KNEEMAX_EPSILON )
	{
		return false;
	}

	Vector ikHalf = (worldFoot-worldThigh) * (l1 / l3);

	// FIXME: what to do when the knee completely straight?
	Vector ikKneeDir = (ikKnee - ikHalf).Normalize();

	return SolveIK( iThigh, iKnee, iFoot, targetFoot, worldKnee, ikKneeDir, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Realign the matrix so that its X axis points along the desired axis.
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: AlignIKMatrix( matrix3x4 &mMat, const Vector &vAlignTo )
{
	Vector tmp1, tmp2, tmp3;

	// Column 0 (X) becomes the vector.
	tmp1 = vAlignTo.Normalize();
	mMat.SetForward( tmp1 );

	// Column 1 (Y) is the cross of the vector and column 2 (Z).
	tmp3 = mMat.GetUp();
	tmp2 = CrossProduct( tmp3, tmp1 ).Normalize();

	// FIXME: check for X being too near to Z
	mMat.SetRight( tmp2 );

	// Column 2 (Z) is the cross of columns 0 (X) and 1 (Y).
	tmp3 = CrossProduct( tmp1, tmp2 );
	mMat.SetUp( tmp3 );
}

//-----------------------------------------------------------------------------
// Purpose: Solve Knee position for a known hip and foot location, and a known knee direction
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: SolveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, Vector &targetKneePos, Vector &targetKneeDir, matrix3x4 *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	worldThigh = pBoneToWorld[iThigh].GetOrigin();
	worldKnee = pBoneToWorld[iKnee].GetOrigin();
	worldFoot = pBoneToWorld[iFoot].GetOrigin();

	// debugLine( worldThigh, worldKnee, 0, 0, 255, true, 0 );
	// debugLine( worldThigh, worldThigh + targetKneeDir, 0, 0, 255, true, 0 );
	// debugLine( worldKnee, targetKnee, 0, 0, 255, true, 0 );

	Vector ikFoot, ikTargetKnee, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = targetKneePos - worldThigh;

	float l1 = (worldKnee-worldThigh).Length();
	float l2 = (worldFoot-worldKnee).Length();

	// exaggerate knee targets for legs that are nearly straight
	// FIXME: should be configurable, and the ikKnee should be from the original animation, not modifed
	float d = (targetFoot - worldThigh).Length() - Q_min( l1, l2 );
	d = Q_max( l1 + l2, d );
	// FIXME: too short knee directions cause trouble
	d = d * 100.0f;

	ikTargetKnee = ikKnee + targetKneeDir * d;

	// debugLine( worldKnee, worldThigh + ikTargetKnee, 0, 0, 255, true, 0 );

	int color[3] = { 0, 255, 0 };

	// too far away? (0.9998 is about 1 degree)
	if( ikFoot.Length() > ( l1 + l2 ) * KNEEMAX_EPSILON )
	{
		ikFoot = ikFoot.Normalize();
		ikFoot *= (l1 + l2) * KNEEMAX_EPSILON;
		color[0] = 255; color[1] = 0; color[2] = 0;
	}

	// too close?
	// limit distance to about an 80 degree knee bend
	float minDist = Q_max( fabs( l1 - l2 ) * 1.15f, Q_min( l1, l2 ) * 0.15f );

	if( ikFoot.Length() < minDist )
	{
		// too close to get an accurate vector, just use original vector
		ikFoot = (worldFoot - worldThigh);
		ikFoot = ikFoot.Normalize();
		ikFoot *= minDist;
	}

	CIKSolver ik;	// heart of all inverse kinematics

	if( ik.solve( l1, l2, ikFoot, ikTargetKnee, ikKnee ))
	{
		matrix3x4& mWorldThigh = pBoneToWorld[iThigh];
		matrix3x4& mWorldKnee = pBoneToWorld[iKnee];
		matrix3x4& mWorldFoot = pBoneToWorld[iFoot];

		// debugLine( worldThigh, ikKnee + worldThigh, 255, 0, 0, true, 0 );
		// debugLine( ikKnee + worldThigh, ikFoot + worldThigh, 255, 0, 0, true,0 );

		// debugLine( worldThigh, ikKnee + worldThigh, color[0], color[1], color[2], true, 0 );
		// debugLine( ikKnee + worldThigh, ikFoot + worldThigh, color[0], color[1], color[2], true,0 );

		// build transformation matrix for thigh
		AlignIKMatrix( mWorldThigh, ikKnee );
		AlignIKMatrix( mWorldKnee, ikFoot - ikKnee );

		mWorldKnee[3][0] = ikKnee.x + worldThigh.x;
		mWorldKnee[3][1] = ikKnee.y + worldThigh.y;
		mWorldKnee[3][2] = ikKnee.z + worldThigh.z;

		mWorldFoot[3][0] = ikFoot.x + worldThigh.x;
		mWorldFoot[3][1] = ikFoot.y + worldThigh.y;
		mWorldFoot[3][2] = ikFoot.z + worldThigh.z;

		return true;
	}
	else
	{
#if 0
		debugLine( worldThigh, worldThigh + ikKnee, 255, 0, 0, true, 0 );
		debugLine( worldThigh + ikKnee, worldThigh + ikFoot, 255, 0, 0, true, 0 );
		debugLine( worldThigh + ikFoot, worldThigh, 255, 0, 0, true, 0 );
		debugLine( worldThigh + ikKnee, worldThigh + ikTargetKnee, 255, 0, 0, true, 0 );
#endif
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: InitPose( Vector pos[], Vector4D q[] )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	mstudioboneinfo_t	*pboneinfo = (mstudioboneinfo_t *)((byte *)pbone + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

	for( int i = 0; i < m_pStudioHeader->numbones; i++, pbone++ )
	{
		// skip unused bones
		if( !IsBoneUsed( pbone + i ))
			continue;

		// check if we can use aligned quaternion instead of euler angles
		if( FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO )) q[i] = pboneinfo[i].quat;
		else AngleQuaternion( Radian( pbone->value[3], pbone->value[4], pbone->value[5] ), q[i] );
		pos[i] = Vector( pbone->value ); // grab three first values
	}
}

//-----------------------------------------------------------------------------
// Purpose: turn a 2x2 blend into a 3 way triangle blend
// Returns: returns the animination indices and barycentric coordinates of a triangle
//	  the triangle is a right triangle, and the diagonal is between elements [0] and [2]
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: Calc9WayBlendIndices( int i0, int i1, float s0, float s1, const mstudioseqdesc_t *pseqdesc, int *pAnimIndices, float *pWeight )
{
	// figure out which bi-section direction we are using to make triangles.
	bool	bEven = ((( i0 + i1 ) & 0x1 ) == 0 );
	int	x1, y1;
	int	x2, y2;
	int	x3, y3;

	// diagonal is between elements 1 & 3

	if( bEven )
	{
		// TL to BR
		if( s0 > s1 )
		{
			// B
			x1 = 0; y1 = 0;
			x2 = 1; y2 = 0;
			x3 = 1; y3 = 1;
			pWeight[0] = (1.0f - s0);
			pWeight[1] = s0 - s1;
		}
		else
		{
			// C
			x1 = 1; y1 = 1;
			x2 = 0; y2 = 1;
			x3 = 0; y3 = 0;
			pWeight[0] = s0;
			pWeight[1] = s1 - s0;
		}
	}
	else
	{
		float flTotal = s0 + s1;

		// BL to TR
		if( flTotal > 1.0f )
		{
			// D
			x1 = 1; y1 = 0;
			x2 = 1; y2 = 1;
			x3 = 0; y3 = 1;
			pWeight[0] = (1.0f - s1);
			pWeight[1] = s0 - 1.0f + s1;
		}
		else
		{
			// A
			x1 = 0; y1 = 1;
			x2 = 0; y2 = 0;
			x3 = 1; y3 = 0;
			pWeight[0] = s1;
			pWeight[1] = 1.0f - s0 - s1;
		}
	}

	pAnimIndices[0] = iAnimBlend( pseqdesc, i0 + x1, i1 + y1 );
	pAnimIndices[1] = iAnimBlend( pseqdesc, i0 + x2, i1 + y2 );
	pAnimIndices[2] = iAnimBlend( pseqdesc, i0 + x3, i1 + y3 );

	// clamp the diagonal
	if( pWeight[1] < 0.001f ) pWeight[1] = 0.0f;
	pWeight[2] = 1.0f - pWeight[0] - pWeight[1];
}

//-----------------------------------------------------------------------------
// Purpose: Calculates default values for the pose parameters
// Output: 	fills in an array
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcDefaultPoseParameters( float flPoseParams[] )
{
	int nPoseCount = CountPoseParameters();

	for( int i = 0; i < MAXSTUDIOPOSEPARAM; i++ )
	{
		// default to middle of the pose parameter range
		flPoseParams[i] = 0.5f;

		if( i < nPoseCount )
		{
			const mstudioposeparamdesc_t *pPose = pPoseParameter( i );

			// want to try for a zero state. If one doesn't exist set it to .5 by default.
			if( pPose->start < 0.0f && pPose->end > 0.0f )
			{
				float flPoseDelta = pPose->end - pPose->start;
				flPoseParams[i] = -pPose->start / flPoseDelta;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: resolve a global pose parameter to the specific setting for this sequence
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: LocalPoseParameter( mstudioseqdesc_t *pseqdesc, int iLocalIndex, float &flSetting, int &index )
{
	// first check for traditional GoldSource blenders
	if( !FBitSet( pseqdesc->flags, STUDIO_BLENDPOSE ))
	{
		flSetting = m_flPoseParams[iLocalIndex];
		index = 0; // unused
		return;
	}
	else
	{
		int iPose = pseqdesc->blendtype[iLocalIndex];

		if( iPose == -1 )
		{
			flSetting = 0;
			index = 0;
			return;
		}

		const mstudioposeparamdesc_t *pPose = pPoseParameter( iPose );

		if( pPose == NULL )
		{
			flSetting = 0;
			index = 0;
			return;
		}

		float flValue = m_flPoseParams[iPose];

		if( pPose->loop )
		{
			float wrap = (pPose->start + pPose->end) / 2.0 + pPose->loop / 2.0;
			float shift = pPose->loop - wrap;
			flValue = flValue - pPose->loop * floor((flValue + shift) / pPose->loop);
		}

		if( pseqdesc->posekeyindex == 0 )
		{
			float flLocalStart	= ((float)pseqdesc->blendstart[iLocalIndex] - pPose->start) / (pPose->end - pPose->start);
			float flLocalEnd	= ((float)pseqdesc->blendend[iLocalIndex] - pPose->start) / (pPose->end - pPose->start);

			// convert into local range
			flSetting = (flValue - flLocalStart) / (flLocalEnd - flLocalStart);

			// clamp.  This shouldn't ever need to happen if it's looping.
			flSetting = bound( 0.0f, flSetting, 1.0f );

			index = 0;
			if( pseqdesc->groupsize[iLocalIndex] > 2 )
			{
				// estimate index
				index = (int)(flSetting * (pseqdesc->groupsize[iLocalIndex] - 1));
				if( index == pseqdesc->groupsize[iLocalIndex] - 1 )
					index = pseqdesc->groupsize[iLocalIndex] - 2;
				flSetting = flSetting * (pseqdesc->groupsize[iLocalIndex] - 1) - index;
			}
		}
		else
		{
			flValue = flValue * (pPose->end - pPose->start) + pPose->start;
			index = 0;
			
			// FIXME: this shouldn't be a linear search
			while( 1 )
			{
				flSetting = (flValue - flPoseKey( pseqdesc, iLocalIndex, index ));
				flSetting /= (flPoseKey( pseqdesc, iLocalIndex, index + 1 ) - flPoseKey( pseqdesc, iLocalIndex, index ));

				if( index < pseqdesc->groupsize[iLocalIndex] - 2 && flSetting > 1.0f )
				{
					index++;
					continue;
				}
				break;
			}

			// clamp.
			flSetting = bound( 0.0f, flSetting, 1.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns array of animations and weightings for a sequence based on current pose parameters
//-----------------------------------------------------------------------------

void CStudioBoneSetup :: LocalSeqAnims( int sequence, mstudioanimdesc_t *panim[4], float *weight )
{
	if( !m_pStudioHeader || sequence < 0 || sequence >= m_pStudioHeader->numseq ) 
	{
		weight[0] = weight[1] = 0.0f;
		weight[2] = weight[3] = 0.0f;
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;
	float s0 = 0.0f, s1 = 0.0f;
	int i0 = 0, i1 = 0;	

	LocalPoseParameter( pseqdesc, 0, s0, i0 );
	LocalPoseParameter( pseqdesc, 1, s1, i1 );

	panim[0] = FetchAnimDesc( pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+0 ));
	weight[0] = (1.0f - s0) * (1.0f - s1);

	panim[1] = FetchAnimDesc( pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+0 ));
	weight[1] = (s0) * (1.0f - s1);

	panim[2] = FetchAnimDesc( pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+1 ));
	weight[2] = (1.0f - s0) * (s1);

	panim[3] = FetchAnimDesc( pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+1 ));
	weight[3] = (s0) * (s1);
}

//-----------------------------------------------------------------------------
// Purpose: returns max frame number for a sequence
//-----------------------------------------------------------------------------
int CStudioBoneSetup :: LocalMaxFrame( int sequence )
{
	mstudioanimdesc_t	*panim[4];
	float		weight[4];
	float		maxFrame = 0;

	LocalSeqAnims( sequence, panim, weight );

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] > 0.0f )
		{
			maxFrame += panim[i]->numframes * weight[i];
		}
	}

	if( maxFrame > 1 )
		maxFrame -= 1;

	// FIXME: why does the weights sometimes not exactly add it 1.0 and this sometimes rounds down?
	return (maxFrame + 0.01);
}

//-----------------------------------------------------------------------------
// Purpose: returns frames per second of a sequence
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: LocalFPS( int sequence )
{
	mstudioanimdesc_t	*panim[4];
	float		weight[4];
	float		t = 0.0f;

	LocalSeqAnims( sequence, panim, weight );

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] > 0.0f )
		{
			t += panim[i]->fps * weight[i];
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
// Purpose: returns cycles per second of a sequence (cycles/second)
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: LocalCPS( int sequence )
{
	mstudioanimdesc_t	*panim[4];
	float		weight[4];
	float		t = 0.0f;

	LocalSeqAnims( sequence, panim, weight );

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] > 0.0f && panim[i]->numframes > 1 )
		{
			t += (panim[i]->fps / (panim[i]->numframes - 1)) * weight[i];
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
// Purpose: converts a ranged bone controller value into a 0..1 encoded value
// Output: 	ctlValue contains 0..1 encoding.
//			returns clamped ranged value
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: SetController( int iController, float flValue, float &ctlValue )
{
	int i;

	if( !m_pStudioHeader )
		return flValue;

	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	// find first controller that matches the index
	for( i = 0; i < m_pStudioHeader->numbonecontrollers; i++, pbonecontroller++ )
	{
		if( pbonecontroller->index == iController )
			break;
	}

	if( i >= m_pStudioHeader->numbonecontrollers )
	{
		ctlValue = 0.0f;
		return flValue;
	}

	// wrap 0..360 if it's a rotational controller
	if( FBitSet( pbonecontroller->type, STUDIO_XR|STUDIO_YR|STUDIO_ZR ))
	{
		// ugly hack, invert value if end < start
		if( pbonecontroller->end < pbonecontroller->start )
			flValue = -flValue;

		// does the controller not wrap?
		if( pbonecontroller->start + 359.0f >= pbonecontroller->end )
		{
			if( flValue > (( pbonecontroller->start + pbonecontroller->end) / 2.0f ) + 180.0f )
				flValue = flValue - 360.0f;
			if( flValue < (( pbonecontroller->start + pbonecontroller->end) / 2.0f ) - 180.0f )
				flValue = flValue + 360.0f;
		}
		else
		{
			if( flValue > 360.0f )
				flValue = flValue - (int)(flValue / 360.0f) * 360.0f;
			else if( flValue < 0.0f )
				flValue = flValue + (int)((flValue / -360.0f) + 1.0f) * 360.0f;
		}
	}

	ctlValue = (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);
	ctlValue = bound( 0.0f, ctlValue, 1.0f );

	float flReturnVal = ((1.0f - ctlValue) * pbonecontroller->start + ctlValue * pbonecontroller->end);

	// ugly hack, invert value if a rotational controller and end < start
	if( FBitSet( pbonecontroller->type, STUDIO_XR | STUDIO_YR | STUDIO_ZR ) && pbonecontroller->end < pbonecontroller->start )
	{
		flReturnVal *= -1.0f;
	}
	
	return flReturnVal;
}

//-----------------------------------------------------------------------------
// Purpose: converts a 0..1 encoded bone controller value into a ranged value
// Output: 	returns ranged value
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: GetController( int iController, float ctlValue )
{
	int i;

	if( !m_pStudioHeader )
		return 0.0f;

	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	// find first controller that matches the index
	for( i = 0; i < m_pStudioHeader->numbonecontrollers; i++, pbonecontroller++ )
	{
		if( pbonecontroller->index == iController )
			break;
	}

	if( i >= m_pStudioHeader->numbonecontrollers )
		return 0.0f;

	return ctlValue * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}


//-----------------------------------------------------------------------------
// Purpose: converts a ranged pose parameter value into a 0..1 encoded value
// Output: 	ctlValue contains 0..1 encoding.
//			returns clamped ranged value
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: SetPoseParameter( int iParameter, float flValue, float &ctlValue )
{
	if( iParameter < 0 || iParameter >= CountPoseParameters( ))
		return 0.0f;

	const mstudioposeparamdesc_t *pPose = pPoseParameter( iParameter );

	if( pPose->loop )
	{
		float wrap = (pPose->start + pPose->end) / 2.0f + pPose->loop / 2.0f;
		float shift = pPose->loop - wrap;
		flValue = flValue - pPose->loop * floor(( flValue + shift ) / pPose->loop );
	}

	ctlValue = (flValue - pPose->start) / (pPose->end - pPose->start);
	ctlValue = bound( 0.0f, ctlValue, 1.0f );

	return ctlValue * (pPose->end - pPose->start) + pPose->start;
}


//-----------------------------------------------------------------------------
// Purpose: converts a 0..1 encoded pose parameter value into a ranged value
// Output: 	returns ranged value
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: GetPoseParameter( int iParameter, float ctlValue )
{
	if( iParameter < 0 || iParameter >= CountPoseParameters( ))
		return 0.0f;

	const mstudioposeparamdesc_t *pPose = pPoseParameter( iParameter );

	return ctlValue * (pPose->end - pPose->start) + pPose->start;
}

//-----------------------------------------------------------------------------
// Purpose: returns length (in seconds) of a sequence (seconds/cycle)
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: LocalDuration( int sequence )
{
	float cps = LocalCPS( sequence );

	if( cps == 0.0f )
		return 0.0f;
	return 1.0f / cps;
}

//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle relative to the start of an animations cycle
// Output:	updated position and angle, relative to the origin
//		returns false if animation is not a movement animation
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: AnimPosition( mstudioanimdesc_t *panim, float flCycle, Vector &vecPos, Vector &vecAngle )
{
	float	prevframe = 0;
	int	iLoops = 0;

	vecPos.Init( );
	vecAngle.Init( );

	if( panim->nummovements == 0 )
		return false;

	if( flCycle > 1.0f )
		iLoops = (int)flCycle;
	else if( flCycle < 0.0f )
		iLoops = (int)flCycle - 1;

	flCycle = flCycle - iLoops;

	float flFrame = flCycle * (panim->numframes - 1);

	for( int i = 0; i < panim->nummovements; i++ )
	{
		const mstudiomovement_t *pmove = pMovement( panim, i );

		if( pmove->endframe >= flFrame )
		{
			float f = (flFrame - prevframe) / (pmove->endframe - prevframe);
			float d = pmove->v0 * f + 0.5 * (pmove->v1 - pmove->v0) * f * f;

			vecPos = vecPos + d * pmove->vector;
			vecAngle.y = vecAngle.y * (1.0f - f) + pmove->angle * f;

			if( iLoops != 0 )
			{
				const mstudiomovement_t *pmove = pMovement( panim, panim->nummovements - 1 );
				vecPos = vecPos + iLoops * pmove->position; 
				vecAngle.y = vecAngle.y + iLoops * pmove->angle; 
			}
			return true;
		}
		else
		{
			prevframe = pmove->endframe;
			vecPos = pmove->position;
			vecAngle.y = pmove->angle;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: calculate instantaneous velocity in ips at a given point 
//			in the animations cycle
// Output:	velocity vector, relative to identity orientation
//			returns false if animation is not a movement animation
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: AnimVelocity( mstudioanimdesc_t *panim, float flCycle, Vector &vecVelocity )
{
	float	flFrame = flCycle * (panim->numframes - 1);
	float	prevframe = 0.0f;
	
	flFrame = flFrame - (int)(flFrame / (panim->numframes - 1));

	for( int i = 0; i < panim->nummovements; i++ )
	{
		const mstudiomovement_t *pmove = pMovement( panim, i );

		if( pmove->endframe >= flFrame )
		{
			float f = (flFrame - prevframe) / (pmove->endframe - prevframe);
			float vel = pmove->v0 * (1 - f) + pmove->v1 * f;

			// scale from per block to per sec velocity
			vel = vel * panim->fps / (pmove->endframe - prevframe);
			vecVelocity = pmove->vector * vel;

			return true;
		}
		else
		{
			prevframe = pmove->endframe;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle between two points in an animation cycle
// Output:	updated position and angle, relative to CycleFrom being at the origin
//			returns false if animation is not a movement animation
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: AnimMovement( mstudioanimdesc_t *panim, float flCycleFrom, float flCycleTo, Vector &deltaPos, Vector &deltaAngle )
{
	if( panim->nummovements == 0 )
		return false;

	Vector startPos;
	Vector startA;

	AnimPosition( panim, flCycleFrom, startPos, startA );

	Vector endPos;
	Vector endA;

	AnimPosition( panim, flCycleTo, endPos, endA );

	Vector tmp = endPos - startPos;
	deltaAngle.y = endA.y - startA.y;

	deltaPos = VectorYawRotate( tmp, -startA.y );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: finds how much of an animation to play to move given linear distance
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: FindAnimDistance( mstudioanimdesc_t *panim, float flDist )
{
	float	prevframe = 0;

	if( flDist <= 0 )
		return 0.0;

	for( int i = 0; i < panim->nummovements; i++ )
	{
		const mstudiomovement_t *pmove = pMovement( panim, i );
		float flMove = (pmove->v0 + pmove->v1) * 0.5f;

		if( flMove >= flDist )
		{
			float root1, root2;

			// d = V0 * t + 1/2 (V1-V0) * t^2
			if( SolveQuadratic( 0.5f * ( pmove->v1 - pmove->v0 ), pmove->v0, -flDist, root1, root2 ))
			{
				float cpf = 1.0f / (panim->numframes - 1);  // cycles per frame
				return (prevframe + root1 * (pmove->endframe - prevframe)) * cpf;
			}
			return 0.0f;
		}
		else
		{
			flDist -= flMove;
			prevframe = pmove->endframe;
		}
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle between two points in a sequences cycle
// Output:	updated position and angle, relative to CycleFrom being at the origin
//			returns false if sequence is not a movement sequence
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: SeqMovement( int sequence, float flCycleFrom, float flCycleTo, Vector &deltaPos, Vector &deltaAngles )
{
	mstudioseqdesc_t	*pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;
	mstudioanimdesc_t	*panim[4];
	float		weight[4];

	LocalSeqAnims( sequence, panim, weight );

	deltaPos = g_vecZero;
	deltaAngles = g_vecZero;

	bool found = false;

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] )
		{
			Vector localPos = g_vecZero;
			Vector localAngles = g_vecZero;

			const float *pweight = pBoneweight( pseqdesc );

			if( AnimMovement( panim[i], flCycleFrom, flCycleTo, localPos, localAngles ))
			{
				found = true;
				deltaPos = deltaPos + localPos * weight[i];
				// FIXME: this makes no sense
				deltaAngles = deltaAngles + localAngles * weight[i];
			}
			else if( !FBitSet( panim[i]->flags, STUDIO_DELTA ) && panim[i]->nummovements == 0 && pweight[0] > 0.0f )
			{
//				found = true;
			}
		}
	}

	// simple movement from GoldSource
	if( !found && pseqdesc->linearmovement != g_vecZero )
	{
		Vector startPos = pseqdesc->linearmovement * flCycleFrom;
		Vector endPos = pseqdesc->linearmovement * flCycleTo;

		deltaPos = endPos - startPos;
		found = true;
	}

	return found;
}

//-----------------------------------------------------------------------------
// Purpose: calculate instantaneous velocity in ips at a given point in the sequence's cycle
// Output:	velocity vector, relative to identity orientation
//			returns false if sequence is not a movement sequence
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: SeqVelocity( int sequence, float flCycle, Vector &vecVelocity )
{
	mstudioanimdesc_t	*panim[4];
	float		weight[4];

	LocalSeqAnims( sequence, panim, weight );
	
	vecVelocity = g_vecZero;
	bool found = false;

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] )
		{
			Vector	vecLocalVelocity;

			if( AnimVelocity( panim[i], flCycle, vecLocalVelocity ))
			{
				vecVelocity = vecVelocity + vecLocalVelocity * weight[i];
				found = true;
			}
		}
	}

	return found;
}

//-----------------------------------------------------------------------------
// Purpose: finds how much of an sequence to play to move given linear distance
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: FindSeqDistance( int sequence, float flDist )
{
	mstudioanimdesc_t	*panim[4];
	float		weight[4];

	LocalSeqAnims( sequence, panim, weight );
	
	float flCycle = 0;

	for( int i = 0; i < 4; i++ )
	{
		if( weight[i] )
		{
			float flLocalCycle = FindAnimDistance( panim[i], flDist );
			flCycle = flCycle + flLocalCycle * weight[i];
		}
	}

	return flCycle;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: IKRuleWeight( const mstudioikrule_t *ikRule, const mstudioanimdesc_t *panim, float flCycle, int &iFrame, float &fraq )
{
	if( ikRule->end > 1.0f && flCycle < ikRule->start )
	{
		flCycle = flCycle + 1.0f;
	}

	float value = 0.0f;
	fraq = (panim->numframes - 1) * (flCycle - ikRule->start) + ikRule->iStart;
	iFrame = (int)fraq;
	fraq = fraq - iFrame;

	if( flCycle < ikRule->start )
	{
		iFrame = ikRule->iStart;
		fraq = 0.0f;
		return 0.0f;
	}
	else if( flCycle < ikRule->peak )
	{
		value = ( flCycle - ikRule->start ) / ( ikRule->peak - ikRule->start );
	}
	else if( flCycle < ikRule->tail )
	{
		return 1.0f;
	}
	else if( flCycle < ikRule->end )
	{
		value = 1.0f - (( flCycle - ikRule->tail ) / ( ikRule->end - ikRule->tail ));
	}
	else
	{
		fraq = (panim->numframes - 1) * (ikRule->end - ikRule->start) + ikRule->iStart;
		iFrame = (int)fraq;
		fraq = fraq - iFrame;
	}

	return SimpleSpline( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: IKRuleWeight( ikcontextikrule_t *ikRule, float flCycle )
{
	if( ikRule->end > 1.0f && flCycle < ikRule->start )
	{
		flCycle = flCycle + 1.0f;
	}

	float value = 0.0f;

	if( flCycle < ikRule->start )
	{
		return 0.0f;
	}
	else if( flCycle < ikRule->peak )
	{
		value = ( flCycle - ikRule->start ) / ( ikRule->peak - ikRule->start );
	}
	else if( flCycle < ikRule->tail )
	{
		return 1.0f;
	}
	else if( flCycle < ikRule->end )
	{
		value = 1.0f - ((flCycle - ikRule->tail) / (ikRule->end - ikRule->tail));
	}

	return SimpleSpline( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: IKShouldLatch( ikcontextikrule_t *ikRule, float flCycle )
{
	if( ikRule->end > 1.0f && flCycle < ikRule->start )
	{
		flCycle = flCycle + 1.0f;
	}

	if( flCycle < ikRule->peak )
	{
		return false;
	}
	else if( flCycle < ikRule->end )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStudioBoneSetup :: IKTail( ikcontextikrule_t *ikRule, float flCycle )
{
	if( ikRule->end > 1.0f && flCycle < ikRule->start )
	{
		flCycle = flCycle + 1.0f;
	}

	if( flCycle <= ikRule->tail )
	{
		return 0.0f;
	}
	else if( flCycle < ikRule->end )
	{
		return (( flCycle - ikRule->tail ) / ( ikRule->end - ikRule->tail ));
	}

	return 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: IKAnimError( const mstudioikrule_t *pRule, mstudioanimdesc_t *panim, float flCycle, Vector &pos, Vector4D &q, float &flWeight )
{
	float fraq;
	int iFrame;

	flWeight = IKRuleWeight( pRule, panim, flCycle, iFrame, fraq );
	flWeight = bound( 0.0f, flWeight, 1.0f );

	if( pRule->type != IK_GROUND && flWeight < 0.0001f )
		return false;

	const mstudioikerror_t *pError = pCompressedError( pRule );

	if( pError != NULL )
	{
		CalcIKError( pError, iFrame - pRule->iStart, fraq, pos, q );
		return true;
	}

	// no data, disable IK rule
	flWeight = 0.0f;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For a specific sequence:rule, find where it starts, stops, and what 
//	  the estimated offset from the connection point is.
//	  return true if the rule is within bounds.
//-----------------------------------------------------------------------------
bool CStudioBoneSetup :: IKSequenceError( int iSeq, float flCycle, int iRule, mstudioanimdesc_t *panim[4], float weight[4], ikcontextikrule_t *ikRule )
{
	int	i;

	memset( ikRule, 0, sizeof( ikcontextikrule_t ));
	ikRule->start = ikRule->peak = ikRule->tail = ikRule->end = 0;

	const mstudioikrule_t *prevRule = NULL;

	// find overall influence
	for( i = 0; i < 4; i++ )
	{
		if( weight[i] )
		{
			if( iRule >= panim[i]->numikrules || panim[i]->numikrules != panim[0]->numikrules )
				return false;

			const mstudioikrule_t *pRule = pIKRule( panim[i], iRule );

			if( pRule == NULL )
				return false;

			float dt = 0.0f;

			if( prevRule != NULL )
			{
				if( pRule->start - prevRule->start > 0.5f )
				{
					dt = -1.0f;
				}
				else if( pRule->start - prevRule->start < -0.5f )
				{
					dt = 1.0;
				}
			}
			else
			{
				prevRule = pRule;
			}

			ikRule->start += (pRule->start + dt) * weight[i];
			ikRule->peak += (pRule->peak + dt) * weight[i];
			ikRule->tail += (pRule->tail + dt) * weight[i];
			ikRule->end += (pRule->end + dt) * weight[i];
		}
	}

	if( ikRule->start > 1.0f )
	{
		ikRule->start -= 1.0f;
		ikRule->peak -= 1.0f;
		ikRule->tail -= 1.0f;
		ikRule->end -= 1.0f;
	}
	else if( ikRule->start < 0.0f )
	{
		ikRule->start += 1.0f;
		ikRule->peak += 1.0f;
		ikRule->tail += 1.0f;
		ikRule->end += 1.0f;
	}

	ikRule->flWeight = IKRuleWeight( ikRule, flCycle );

	if( ikRule->flWeight <= 0.001f )
	{
		// go ahead and allow IK_GROUND rules a virtual looping section
		if( pIKRule( panim[0], iRule ) == NULL ) 
			return false;

		if(( panim[0]->flags & STUDIO_LOOPING ) && pIKRule( panim[0], iRule )->type == IK_GROUND && ikRule->end - ikRule->start > 0.75f )
		{
			ikRule->flWeight = 0.001f;
			flCycle = ikRule->end - 0.001f;
		}
		else
		{
			return false;
		}
	}

	ikRule->pos.Init();
	ikRule->q.Init();

	// find target error
	float total = 0.0f;
	for( i = 0; i < 4; i++ )
	{
		if( weight[i] )
		{
			Vector	pos1;
			Vector4D	q1;
			float	w;

			const mstudioikrule_t *pRule = pIKRule( panim[i], iRule );
			if( pRule == NULL )
				return false;

			ikRule->chain = pRule->chain;	// FIXME: this is anim local
			ikRule->bone = pRule->bone;	// FIXME: this is anim local
			ikRule->type = pRule->type;
			ikRule->slot = pRule->slot;

			ikRule->height += pRule->height * weight[i];
			ikRule->floor += pRule->floor * weight[i];
			ikRule->radius += pRule->radius * weight[i];
			ikRule->drop += pRule->drop * weight[i];
			ikRule->top += pRule->top * weight[i];

			// keep track of tail condition
			ikRule->release += IKTail( ikRule, flCycle ) * weight[i];

			// only check rules with error values
			switch( ikRule->type )
			{
			case IK_SELF:
			case IK_WORLD:
			case IK_GROUND:
			case IK_ATTACHMENT:
				if( IKAnimError( pRule, panim[i], flCycle, pos1, q1, w ))
				{
					ikRule->pos = ikRule->pos + pos1 * weight[i];
					QuaternionAccumulate( ikRule->q, weight[i], q1, ikRule->q );
					total += weight[i];
				}
				break;
			default:
				total += weight[i];
				break;
			}

			ikRule->latched = IKShouldLatch( ikRule, flCycle ) * ikRule->flWeight;

			if( ikRule->type == IK_ATTACHMENT )
			{
				ikRule->iAttachment = pRule->attachment;
			}
		}
	}

	if( total <= 0.0001f )
	{
		return false;
	}

	if( total < 0.999f )
	{
		QuaternionScale( ikRule->q, 1.0f / total, ikRule->q );
		ikRule->pos *= ( 1.0f / total );
	}

	ikRule->q = ikRule->q.Normalize();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcPoseSingle( Vector pos[], Vector4D q[], int sequence, float cycle )
{
	static Vector	pos2[MAXSTUDIOBONES];
	static Vector4D	q2[MAXSTUDIOBONES];
	static Vector	pos3[MAXSTUDIOBONES];
	static Vector4D	q3[MAXSTUDIOBONES];
	static Vector	pos4[MAXSTUDIOBONES];
	static Vector4D	q4[MAXSTUDIOBONES];
	bool		anim_4wayblend = true;	// FIXME: get 9-way for gold-source
	mstudioseqdesc_t	*pseqdesc;

	if( sequence < 0 || sequence >= m_pStudioHeader->numseq ) 
	{
		debugMsg( "^2Warning:^7 sequence %i/%i out of range for model %s\n", sequence, m_pStudioHeader->numseq, m_pStudioHeader->name );
		sequence = 0;
          }

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	int i0 = 0, i1 = 0;
	float s0 = 0, s1 = 0;

	LocalPoseParameter( pseqdesc, 0, s0, i0 );
	LocalPoseParameter( pseqdesc, 1, s1, i1 );

	if( FBitSet( pseqdesc->flags, STUDIO_REALTIME ))
	{
		float cps = LocalCPS( sequence );
		cycle = m_flTime * cps;
		cycle = cycle - (int)cycle;
	}
	else if( FBitSet( pseqdesc->flags, STUDIO_CYCLEPOSE ))
	{
		int iPose = bound( 0, pseqdesc->cycleposeindex, MAXSTUDIOPOSEPARAM - 1 );
		cycle = m_flPoseParams[iPose];
	}
	else if( cycle < 0.0f || cycle >= 1.0f )
	{
		if( FBitSet( pseqdesc->flags, STUDIO_LOOPING ))
		{
			cycle = cycle - (int)cycle;
			if( cycle < 0.0f ) cycle += 1.0f;
		}
		else
		{
			cycle = bound( 0.0f, cycle, 1.0f );
		}
	}

	// GoldSource blending
	if( !FBitSet( pseqdesc->flags, STUDIO_BLENDPOSE ) && ( pseqdesc->numblends > 1 ))
	{
		if( pseqdesc->numblends == 9 )
		{
			// blending is 0 - 0.5 == Left to Middle, 0.5 to 1.0 == Middle to Right
			if( s1 <= 0.5f )
			{
				// Scale 0-0.5 blending up to 0-1.0
				s1 = ( s1 * 2.0f );

				if( s0 <= 0.5f )
				{
					// Blending is 0-127 == Top to Middle, 128 to 255 == Middle to Bottom
					s0 = ( s0 * 2.0f );

					// need to blend 0 - 1 - 3 - 4
					CalcAnimation( pos, q, pseqdesc, 0, cycle );
					CalcAnimation( pos2, q2, pseqdesc, 1, cycle );
					CalcAnimation( pos3, q3, pseqdesc, 3, cycle );
					CalcAnimation( pos4, q4, pseqdesc, 4, cycle );
				}
				else
				{
					// Scale 0.5-1.0 blending up to 0.0-1.0
					s0 = 2.0f * ( s0 - 0.5f );

					// need to blend 3 - 4 - 6 - 7
					CalcAnimation( pos, q, pseqdesc, 3, cycle );
					CalcAnimation( pos2, q2, pseqdesc, 4, cycle );
					CalcAnimation( pos3, q3, pseqdesc, 6, cycle );
					CalcAnimation( pos4, q4, pseqdesc, 7, cycle );
				}
			}
			else
			{		
				// Scale 0.5-1.0 blending up to 0-1.0
				s1 = 2.0f * ( s1 - 0.5f );

				if ( s0 <= 0.5f )
				{
					// Blending is 0-0.5 == Top to Middle, 0.5 to 1.0 == Middle to Bottom
					s0 = ( s0 * 2.0f );

					// need to blend 1 - 2 - 4 - 5
					CalcAnimation( pos, q, pseqdesc, 1, cycle );
					CalcAnimation( pos2, q2, pseqdesc, 2, cycle );
					CalcAnimation( pos3, q3, pseqdesc, 4, cycle );
					CalcAnimation( pos4, q4, pseqdesc, 5, cycle );
				}
				else
				{
					// Scale 0.5-1.0 blending up to 0-1.0
					s0 = 2.0 * ( s0 - 0.5 );

					// need to blend 4 - 5 - 7 - 8
					CalcAnimation( pos, q, pseqdesc, 4, cycle );
					CalcAnimation( pos2, q2, pseqdesc, 5, cycle );
					CalcAnimation( pos3, q3, pseqdesc, 7, cycle );
					CalcAnimation( pos4, q4, pseqdesc, 8, cycle );
				}
			}

			// Spherically interpolate the bones
			SlerpBones( q, pos, pseqdesc, q2, pos2, s1 );
			SlerpBones( q3, pos3, pseqdesc, q4, pos4, s1 );
			SlerpBones( q, pos, pseqdesc, q3, pos3, s0 );
		}
		else
		{
			CalcAnimation( pos,  q,  pseqdesc, 0, cycle );
			CalcAnimation( pos2, q2, pseqdesc, 1, cycle );
			BlendBones( q,  pos, pseqdesc, q2, pos2, s0 );

			if( pseqdesc->numblends == 4 )
			{
				CalcAnimation( pos3, q3, pseqdesc, 2, cycle );
				CalcAnimation( pos4, q4, pseqdesc, 3, cycle );
				BlendBones( q3, pos3, pseqdesc, q4, pos4, s0 );
				BlendBones( q, pos, pseqdesc, q3, pos3, s1 );
			}
		}
		return;
	}

	if( s0 < 0.001f )
	{
		if( s1 < 0.001f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+0 ), cycle );
		}
		else if( s1 > 0.999f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+1 ), cycle );
		}
		else
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+0 ), cycle );
			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+1 ), cycle );
			BlendBones( q,  pos,  pseqdesc, q2, pos2, s1 );
		}
	}
	else if( s0 > 0.999f )
	{
		if( s1 < 0.001f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+0 ), cycle );
		}
		else if( s1 > 0.999f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+1 ), cycle );
		}
		else
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+0 ), cycle );
			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+1 ), cycle );
			BlendBones( q,  pos,  pseqdesc, q2, pos2, s1 );
		}
	}
	else
	{
		if( s1 < 0.001f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+0 ), cycle );
			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+0 ), cycle );
			BlendBones( q,  pos,  pseqdesc, q2, pos2, s0 );
		}
		else if( s1 > 0.999f )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+1 ), cycle );
			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+1 ), cycle );
			BlendBones( q,  pos,  pseqdesc, q2, pos2, s0 );
		}
		else if( anim_4wayblend )
		{
			CalcAnimation( pos,  q,  pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+0 ), cycle );
			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+0 ), cycle );
			BlendBones( q,  pos,  pseqdesc, q2, pos2, s0 );

			CalcAnimation( pos2, q2, pseqdesc, iAnimBlend( pseqdesc, i0+0, i1+1 ), cycle );
			CalcAnimation( pos3, q3, pseqdesc, iAnimBlend( pseqdesc, i0+1, i1+1 ), cycle );
			BlendBones( q2, pos2, pseqdesc, q3, pos3, s0 );

			BlendBones( q,  pos,  pseqdesc, q2, pos2, s1 );
		}
		else
		{
			int	iAnimIndices[3];
			float	weight[3];

			Calc9WayBlendIndices( i0, i1, s0, s1, pseqdesc, iAnimIndices, weight );

			if( weight[1] < 0.001f )
			{
				// on diagonal
				CalcAnimation( pos,  q,  pseqdesc, iAnimIndices[0], cycle );
				CalcAnimation( pos2, q2, pseqdesc, iAnimIndices[2], cycle );
				BlendBones( q, pos, pseqdesc, q2, pos2, weight[2] / ( weight[0] + weight[2] ));
			}
			else
			{
				CalcAnimation( pos,  q,  pseqdesc, iAnimIndices[0], cycle );
				CalcAnimation( pos2, q2, pseqdesc, iAnimIndices[1], cycle );
				BlendBones( q, pos, pseqdesc, q2, pos2, weight[1] / ( weight[0] + weight[1] ));

				CalcAnimation( pos3, q3, pseqdesc, iAnimIndices[2], cycle );
				BlendBones( q, pos, pseqdesc, q3, pos3, weight[2] );
			}
		}
	}

	// list is cleared
	SetBoneControllers( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//	  adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: AddSequenceLayers( CIKContext *pIKContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight )
{
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	for( int i = 0; i < pseqdesc->numautolayers; i++ )
	{
		mstudioautolayer_t *pLayer = (mstudioautolayer_t *)((byte *)m_pStudioHeader + pseqdesc->autolayerindex) + i;

		if( FBitSet( pLayer->flags, STUDIO_AL_LOCAL ))
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if( pLayer->start != pLayer->end )
		{
			float s = 1.0;
			float index;

			if( !FBitSet( pLayer->flags, STUDIO_AL_POSE ))
			{
				index = cycle;
			}
			else
			{
				int iSequence = pLayer->iSequence;
				int iPose = pLayer->iPose;

				if( iPose != -1 )
				{
					const mstudioposeparamdesc_t *pPose = pPoseParameter( iPose );
					index = m_flPoseParams[iPose] * (pPose->end - pPose->start) + pPose->start;
				}
				else
				{
					index = 0;
				}
			}

			if( index < pLayer->start )
				continue;
			if( index >= pLayer->end )
				continue;

			if( index < pLayer->peak && pLayer->start != pLayer->peak )
			{
				s = (index - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if( index > pLayer->tail && pLayer->end != pLayer->tail )
			{
				s = (pLayer->end - index) / (pLayer->end - pLayer->tail);
			}

			if( FBitSet( pLayer->flags, STUDIO_AL_SPLINE ))
			{
				s = SimpleSpline( s );
			}

			if( FBitSet( pLayer->flags, STUDIO_AL_XFADE ) && ( index > pLayer->tail ))
			{
				layerWeight = ( s * flWeight ) / ( 1.0f - flWeight + s * flWeight );
			}
			else if( FBitSet( pLayer->flags, STUDIO_AL_NOBLEND ))
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			if( !FBitSet( pLayer->flags, STUDIO_AL_POSE ))
			{
				layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
			}
		}

		AccumulatePose( pIKContext, pos, q, pLayer->iSequence, layerCycle, layerWeight );
	}
}


//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//	  adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: AddLocalLayers( CIKContext *pIKContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight )
{
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	if( !FBitSet( pseqdesc->flags, STUDIO_LOCAL ))
	{
		return;
	}

	for( int i = 0; i < pseqdesc->numautolayers; i++ )
	{
		mstudioautolayer_t *pLayer = (mstudioautolayer_t *)((byte *)m_pStudioHeader + pseqdesc->autolayerindex) + i;

		if( !FBitSet( pLayer->flags, STUDIO_AL_LOCAL ))
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if( pLayer->start != pLayer->end )
		{
			float s = 1.0f;

			if( cycle < pLayer->start )
				continue;

			if( cycle >= pLayer->end )
				continue;

			if( cycle < pLayer->peak && pLayer->start != pLayer->peak )
			{
				s = (cycle - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if( cycle > pLayer->tail && pLayer->end != pLayer->tail )
			{
				s = (pLayer->end - cycle) / (pLayer->end - pLayer->tail);
			}

			if( FBitSet( pLayer->flags, STUDIO_AL_SPLINE ))
			{
				s = SimpleSpline( s );
			}

			if( FBitSet( pLayer->flags, STUDIO_AL_XFADE ) && ( cycle > pLayer->tail ))
			{
				layerWeight = ( s * flWeight ) / ( 1.0f - flWeight + s * flWeight );
			}
			else if( FBitSet( pLayer->flags, STUDIO_AL_NOBLEND ))
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
		}

		AccumulatePose( pIKContext, pos, q, pLayer->iSequence, layerCycle, layerWeight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: accumulate a pose for a single sequence on top of existing animation
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: AccumulatePose( CIKContext *pIKContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight )
{
	Vector	pos2[MAXSTUDIOBONES];
	Vector4D	q2[MAXSTUDIOBONES];

	flWeight = bound( 0.0f, flWeight, 1.0f );
	if( sequence < 0 ) return;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	// add any IK locks to prevent extremities from moving
	CIKContext seq_ik;

	if( pseqdesc->numiklocks )
	{
		// local space relative so absolute position doesn't mater
		seq_ik.Init( this, g_vecZero, g_vecZero, 0.0f, 0 );
		seq_ik.AddSequenceLocks( pseqdesc, pos, q );
	}

	if( FBitSet( pseqdesc->flags, STUDIO_LOCAL ))
	{
		InitPose( pos2, q2 );
	}

	CalcPoseSingle( pos2, q2, sequence, cycle );

	// this weight is wrong, the IK rules won't composite at the correct intensity
	AddLocalLayers( pIKContext, pos2, q2, sequence, cycle, 1.0 );
	SlerpBones( q, pos, pseqdesc, q2, pos2, flWeight );

	if( pIKContext )
	{
		pIKContext->AddDependencies( pseqdesc, sequence, cycle, flWeight );
	}

	AddSequenceLayers( pIKContext, pos, q, sequence, cycle, flWeight );

	if( pseqdesc->numiklocks )
	{
		seq_ik.SolveSequenceLocks( pseqdesc, pos, q );
	}
}

//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcBoneAdj( Vector pos[], Vector4D q[], const byte controllers[], byte mouthopen )
{
	mstudiobonecontroller_t	*pbonecontroller;
	int			i, j, k;
	float			value;
	Vector			p0;
	Radian			a0;
	Vector4D			q0;
	
	for( j = 0; j < m_pStudioHeader->numbonecontrollers; j++ )
	{
		pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex) + j;
		k = pbonecontroller->bone;

		if( IsBoneUsed( k ))
		{
			i = pbonecontroller->index;

#if 1
			if( i == STUDIO_MOUTH )
				value = bound( 0.0f, ( mouthopen / 64.0f ), 1.0f );				
			else value = bound( 0.0f, (float)controllers[i] / 255.0f, 1.0f );
#else
			if (i != STUDIO_MOUTH) 
			{
				//value = bound(0.0f, (float)controllers[i] / 255.0f, 1.0f);
				//value = (1.0f - value) * pbonecontroller->start + value * pbonecontroller->end;
				if (pbonecontroller->type & STUDIO_RLOOP)
				{
					value = controllers[j] * (360.0/256.0) + pbonecontroller->start;
				}
				else 
				{
					value = controllers[j] / 255.0;
					if (value < 0) value = 0;
					if (value > 1.0) value = 1.0;
					value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
				}
			}
			else 
			{
				value = bound(0.0f, (mouthopen / 64.0f), 1.0f);
				value = (1.0f - value) * pbonecontroller->start + value * pbonecontroller->end;
			}
#endif

			switch( pbonecontroller->type & STUDIO_TYPES )
			{
			case STUDIO_XR: 
				a0.Init( DEG2RAD( value ), 0.0f, 0.0f ); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0f, q0, q[k], q[k] );
				break;
			case STUDIO_YR: 
				a0.Init( 0.0f, DEG2RAD( value ), 0.0f ); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0f, q0, q[k], q[k] );
				break;
			case STUDIO_ZR: 
				a0.Init( 0.0f, 0.0f, DEG2RAD( value )); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0f, q0, q[k], q[k] );
				break;
			case STUDIO_X:	
				pos[k].x += value;
				break;
			case STUDIO_Y:
				pos[k].y += value;
				break;
			case STUDIO_Z:
				pos[k].z += value;
				break;
			}
		}
	}
}

void CStudioBoneSetup :: CalcBoneAdj( float adj[], const byte controllers[], byte mouthopen )
{
	mstudiobonecontroller_t	*pbonecontroller;
	int			i, j, k;
	float			value;
	Vector			p0;
	Radian			a0;
	Vector4D			q0;
	
	for( j = 0; j < m_pStudioHeader->numbonecontrollers; j++ )
	{
		pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex) + j;
		k = pbonecontroller->bone;

		if( IsBoneUsed( k ))
		{
			i = pbonecontroller->index;

#if 1
			if( i == STUDIO_MOUTH )
				value = bound( 0.0f, ( mouthopen / 64.0f ), 1.0f );				
			else value = bound( 0.0f, (float)controllers[i] / 255.0f, 1.0f );
#else
			if (i != STUDIO_MOUTH) 
			{
				//value = bound(0.0f, (float)controllers[i] / 255.0f, 1.0f);
				//value = (1.0f - value) * pbonecontroller->start + value * pbonecontroller->end;
				if (pbonecontroller->type & STUDIO_RLOOP)
				{
					value = controllers[j] * (360.0/256.0) + pbonecontroller->start;
				}
				else 
				{
					value = controllers[j] / 255.0;
					if (value < 0) value = 0;
					if (value > 1.0) value = 1.0;
					value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
				}
			}
			else 
			{
				value = bound(0.0f, (mouthopen / 64.0f), 1.0f);
				value = (1.0f - value) * pbonecontroller->start + value * pbonecontroller->end;
			}
#endif

			switch( pbonecontroller->type & STUDIO_TYPES )
			{
			case STUDIO_YR: 
			case STUDIO_ZR: 
			case STUDIO_XR: 
				adj[j] = DEG2RAD( value ); 
				break;
			case STUDIO_X:	
			case STUDIO_Y:
			case STUDIO_Z:
				adj[j] = value;
				break;
			}
		}
	}

	// list is installed
	SetBoneControllers( adj );
}

//-----------------------------------------------------------------------------
// Purpose: run all animations that automatically play and are driven off of poseParameters
//-----------------------------------------------------------------------------
void CStudioBoneSetup :: CalcAutoplaySequences( CIKContext *pIKContext, Vector pos[], Vector4D q[] )
{
	if( pIKContext )
	{
		pIKContext->AddAutoplayLocks( pos, q );
	}

	for( int i = 0; i < m_pStudioHeader->numseq; i++ )
	{
		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + i;

		if( !FBitSet( pseqdesc->flags, STUDIO_AUTOPLAY ))
			continue;

		float cps = LocalCPS( i );
		float cycle = m_flTime * cps;
		cycle = cycle - (int)cycle;

		AccumulatePose( NULL, pos, q, i, cycle, 1.0 );
	}

	if( pIKContext )
	{
		pIKContext->SolveAutoplayLocks( pos, q );
	}
}
