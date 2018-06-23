/*
bs_desf.h - Bone Setup defines
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

#ifndef BS_DEFS_H
#define BS_DEFS_H

#include <alert.h>

struct ikcontextikrule_t;
class CIKContext;

/*
====================
CStudioBoneSetup

====================
*/
class CStudioBoneSetup
{
public:
	CStudioBoneSetup()
	{
		InitBoneWeights();
		m_pStudioHeader = NULL;
		m_flBoneControllers = NULL;
		m_flPoseParams = NULL;
		m_iBoneMask = 0;
	} 
//protected:
	const mstudioanimvalue_t *pAnimvalue( const mstudioanim_t *panim, int dof )
	{
		if( !panim || panim->offset[dof] == 0 )
			return NULL;
		return (mstudioanimvalue_t *)((byte *)panim + panim->offset[dof]);
	}

	const mstudioanimvalue_t *pAnimvalue( const mstudioikerror_t *panim, int dof )
	{
		if( !panim || panim->offset[dof] == 0 )
			return NULL;
		return (mstudioanimvalue_t *)((byte *)panim + panim->offset[dof]);
	}

	const mstudiomovement_t *pMovement( const mstudioanimdesc_t *panim, int movement )
	{
		if( !panim || panim->nummovements <= 0 )
			return NULL;
		return (mstudiomovement_t *)((byte *)m_pStudioHeader + panim->movementindex) + movement;
	}

	const mstudioikrule_t *pIKRule( const mstudioanimdesc_t *panim, int iRule )
	{
		if( !panim || panim->numikrules <= 0 )
			return NULL;
		return (mstudioikrule_t *)((byte *)m_pStudioHeader + panim->ikruleindex) + iRule;
	}

	const mstudioikchain_t *pIKChain( int chain )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && chain >= 0 && chain < phdr2->numikchains )
			return (mstudioikchain_t *)((byte *)m_pStudioHeader + phdr2->ikchainindex) + chain;

		return NULL;
	}

	const mstudioiklock_t *pIKAutoplayLock( int lock )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && lock >= 0 && lock < phdr2->numikautoplaylocks )
			return (mstudioiklock_t *)((byte *)m_pStudioHeader + phdr2->ikautoplaylockindex) + lock;

		return NULL;
	}

	const mstudioiklink_t *pIKLink( const mstudioikchain_t *pchain, int link )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && link >= 0 && link < pchain->numlinks )
			return (mstudioiklink_t *)((byte *)m_pStudioHeader + pchain->linkindex) + link;

		return NULL;
	}

	const mstudioiklock_t *pIKLock( const mstudioseqdesc_t *pseqdesc, int lock )
	{
		if( !pseqdesc || pseqdesc->iklockindex <= 0 )
			return NULL;
		return (mstudioiklock_t *)((byte *)m_pStudioHeader + pseqdesc->iklockindex) + lock;
	}

	const mstudioikerror_t *pCompressedError( const mstudioikrule_t *pRule )
	{
		if( !pRule || pRule->ikerrorindex <= 0 )
			return NULL;
		return (mstudioikerror_t *)((byte *)m_pStudioHeader + pRule->ikerrorindex);
	}

	const float *pBoneweight( const mstudioseqdesc_t *pseqdesc )
	{
		if( !pseqdesc || pseqdesc->weightlistindex <= 0 )
		{
			if( m_flCustomBoneWeight != NULL )
				return m_flCustomBoneWeight;
			return m_flDefaultBoneWeight;
		}
		return (float *)((byte *)m_pStudioHeader + pseqdesc->weightlistindex);
	}

	const mstudioposeparamdesc_t *pPoseParameter( int iPose )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && phdr2->numposeparameters > iPose )
			return (mstudioposeparamdesc_t *)((byte *)m_pStudioHeader + phdr2->poseparamindex) + iPose;

		return NULL; // poseparams is missed
	}

	int CountPoseParameters( void )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && phdr2->numposeparameters > 0 )
			return phdr2->numposeparameters;
		return 0; // poseparams is missed
	}

	int GetNumIKChains( void )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && phdr2->numikchains > 0 )
			return phdr2->numikchains;
		return 0; // no IK chains
	}

	int GetNumIKAutoplayLocks( void )
	{
		studiohdr2_t *phdr2 = NULL;

		if( m_pStudioHeader->studiohdr2index > 0 && m_pStudioHeader->studiohdr2index < m_pStudioHeader->length )
			phdr2 = (studiohdr2_t *)((byte *)m_pStudioHeader + m_pStudioHeader->studiohdr2index);

		if( phdr2 && phdr2->numikautoplaylocks > 0 )
			return phdr2->numikautoplaylocks;
		return 0; // no IK autoplay locks
	}

	const float flPoseKey( const mstudioseqdesc_t *pseqdesc, int iParam, int iAnim )
	{
		float *poseKey = (float *)((byte *)m_pStudioHeader + pseqdesc->posekeyindex);
		return poseKey[iParam * pseqdesc->groupsize[0] + iAnim];
	}

	int iAnimBlend( const mstudioseqdesc_t *pseqdesc, int x, int y )
	{
		if( x >= pseqdesc->groupsize[0] )
			x = pseqdesc->groupsize[0] - 1;

		if( y >= pseqdesc->groupsize[1] )
			y = pseqdesc->groupsize[1] - 1;

		return (x + pseqdesc->groupsize[0] * y); // animations[blend]
	}

	bool IsBoneUsed( mstudiobone_t *pbone )
	{
		if( m_iBoneMask )
			return (FBitSet( pbone->flags, m_iBoneMask )) ? true : false;
		return true;
	}

	bool IsBoneUsed( int iBone )
	{
		mstudiobone_t *pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

		if( iBone != -1 && m_iBoneMask != 0 )
			return (FBitSet( pbone[iBone].flags, m_iBoneMask )) ? true : false;
		return true;
	}
private:
	void InitBoneWeights( void ) { for( int i = 0; i < MAXSTUDIOBONES; i++ ) m_flDefaultBoneWeight[i] = 1.0f; }
	void ExtractAnimValue( int frame, const mstudioanimvalue_t *panimvalue, float scale, float &v1, float &v2 );
	void ExtractAnimValue( int frame, const mstudioanimvalue_t *panimvalue, float scale, float &v1 );
	Vector4D CalcBoneQuaternion( int frame, float s, int flags, mstudiobone_t *pbone, mstudioboneinfo_t *pboneinfo, mstudioanim_t *panim );
	Vector CalcBonePosition( int frame, float s, int flags, mstudiobone_t *pbone, mstudioanim_t *panim );
	void AdjustBoneAngles( mstudiobone_t *pbone, Radian &angles1, Radian &angles2 );
	void AdjustBoneOrigin( mstudiobone_t *pbone, Vector &origin );
	void CalcIKError( const mstudioikerror_t *pIKError, int frame, float s, Vector &pos, Vector4D &q );
	mstudioanim_t *FetchAnimation( mstudioseqdesc_t *pseqdesc, int animation );
	mstudioanimdesc_t *FetchAnimDesc( mstudioseqdesc_t *pseqdesc, int animation );
	void CalcAnimation( Vector pos[], Vector4D q[], mstudioseqdesc_t *seqdesc, int animation, float cycle );
	void BlendBones( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s );
	void SlerpBones( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s );
	void BuildBoneChain( const matrix3x4 &root, const Vector pos[], const Vector4D q[], int iBone, matrix3x4 *pBoneToWorld, byte *pSet = NULL );
	void WorldSpaceSlerp( Vector4D q1[], Vector pos1[], mstudioseqdesc_t *pseqdesc, const Vector4D q2[], const Vector pos2[], float s );
	void Calc9WayBlendIndices( int i0, int i1, float s0, float s1, const mstudioseqdesc_t *pseqdesc, int *pAnimIndices, float *pWeight );
	void AddSequenceLayers( CIKContext *pContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight );
	void AddLocalLayers( CIKContext *pContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight );
	void SolveBone( int iBone, matrix3x4 *pBoneToWorld, Vector pos[], Vector4D q[] );
	bool SolveIK( const mstudioikchain_t *pikchain, Vector &targetFoot, matrix3x4 *pBoneToWorld );
	bool SolveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, matrix3x4 *pBoneToWorld );
	bool SolveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, Vector &targetKneePos, Vector &targetKneeDir, matrix3x4 *pBoneToWorld );
	void CalcPoseSingle( Vector pos[], Vector4D q[], int sequence, float cycle );
	void AlignIKMatrix( matrix3x4 &mMat, const Vector &vAlignTo );

	// private routines
	void LocalPoseParameter( mstudioseqdesc_t *pseqdesc, int iLocalIndex, float &flSetting, int &index );
	bool IKAnimError( const mstudioikrule_t *pRule, mstudioanimdesc_t *panim, float flCycle, Vector &pos, Vector4D &q, float &flWeight );
	float IKTail( ikcontextikrule_t *ikRule, float flCycle );
	bool IKSequenceError( int iSequence, float flCycle, int iRule, mstudioanimdesc_t *panim[4], float weight[4], ikcontextikrule_t *ikRule );
	float IKRuleWeight( const mstudioikrule_t *ikRule, const mstudioanimdesc_t *panim, float flCycle, int &iFrame, float &fraq );
	float IKRuleWeight( ikcontextikrule_t *ikRule, float flCycle );
	bool IKShouldLatch( ikcontextikrule_t *ikRule, float flCycle );

	float		m_flDefaultBoneWeight[MAXSTUDIOBONES];	// compatibility issues
	float		*m_flCustomBoneWeight;		// user boneweights
	const float	*m_flBoneControllers;
	const float	*m_flPoseParams;
	int		m_iBoneMask;
	float		m_flTime;	// realtime

	// intermediate matrices
	matrix3x4		srcBoneToWorld[MAXSTUDIOBONES];
	matrix3x4		dstBoneToWorld[MAXSTUDIOBONES];
	matrix3x4		targetBoneToWorld[MAXSTUDIOBONES];
public:
	// import table
	virtual void debugMsg( char *szFmt, ... ) {}
	virtual mstudioanim_t *GetAnimSourceData( mstudioseqdesc_t *pseqdesc );
	virtual void debugLine( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest = false, float duration = 0.0f ) {}

	// export table
	void InitPose( Vector pos[], Vector4D q[] );
	void AccumulatePose( CIKContext *pContext, Vector pos[], Vector4D q[], int sequence, float cycle, float flWeight );
	void CalcBoneAdj( Vector pos[], Vector4D q[], const byte controllers[], byte mouthopen );
	void CalcBoneAdj( float adj[], const byte controllers[], byte mouthopen );
	void CalcAutoplaySequences( CIKContext *pContext, Vector pos[], Vector4D q[] );
	void SetStudioPointers( studiohdr_t *pStudioHdr, const float *pPoseParams ) { m_pStudioHeader = pStudioHdr; m_flPoseParams = pPoseParams; }
	void SetBoneControllers( float *pNewList ) { m_flBoneControllers = pNewList; }
	void SetBoneWeights( float *pNewList ) { m_flCustomBoneWeight = pNewList; }
	void SetBoneMask( int iBoneMask ) { m_iBoneMask = iBoneMask; }
	void UpdateRealTime( float flTime ) { m_flTime = flTime; }
	void CalcDefaultPoseParameters( float flPoseParams[] );

	// shared routines
	float GetController( int iController, float ctlValue );
	float SetController( int iController, float flValue, float &ctlValue );
	float GetPoseParameter( int iParameter, float ctlValue );
	float SetPoseParameter( int iParameter, float flValue, float &ctlValue );
	void LocalSeqAnims( int sequence, mstudioanimdesc_t *panim[4], float *weight );
	int LocalMaxFrame( int iSequence );
	float LocalDuration( int sequence );
	float LocalFPS( int sequence );
	float LocalCPS( int sequence );

	// movement routines
	bool AnimPosition( mstudioanimdesc_t *panim, float flCycle, Vector &vecPos, Vector &vecAngle );
	bool AnimVelocity( mstudioanimdesc_t *panim, float flCycle, Vector &vecVelocity );
	bool AnimMovement( mstudioanimdesc_t *panim, float flCycleFrom, float flCycleTo, Vector &deltaPos, Vector &deltaAngle );
	float FindAnimDistance( mstudioanimdesc_t *panim, float flDist );
	bool SeqMovement( int sequence, float flCycleFrom, float flCycleTo, Vector &deltaPos, Vector &deltaAngles );
	bool SeqVelocity( int iSequence, float flCycle, Vector &vecVelocity );
	float FindSeqDistance( int sequence, float flDist );

	studiohdr_t	*m_pStudioHeader;
	friend class	CIKContext;
};

#endif//BS_DEFS_H
