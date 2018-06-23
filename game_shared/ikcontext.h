/*
ikcontext.h - Inverse Kinematic implementation
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef IKCONTEXT_H
#define IKCONTEXT_H

#include <studio.h>
#include <utlarray.h>

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CIKContext;

// ik info
class CIKTarget
{
public:
	void SetOwner( int entindex, const Vector &pos, const Vector &angles );
	void ClearOwner( void );
	int GetOwner( void );
	void UpdateOwner( int entindex, const Vector &pos, const Vector &angles );
	void SetPos( const Vector &pos );
	void SetAngles( const Vector &angles );
	void SetQuaternion( const Vector4D &q );
	void SetNormal( const Vector &normal );
	void SetPosWithNormalOffset( const Vector &pos, const Vector &normal );
	void SetOnWorld( bool bOnWorld = true );

	bool IsActive( void );
	void IKFailed( void );
	int chain;
	int type;
	void MoveReferenceFrame( Vector &deltaPos, Vector &deltaAngles );
	// accumulated offset from ideal footplant location
public:
	struct x2
	{
		int	attachmentIndex;
		Vector	pos;
		Vector4D	q;
	} offset;
private:
	struct x3
	{
		Vector	pos;
		Vector4D	q;
	} ideal;
public:
	struct x4
	{
		float	latched;
		float	release;
		float	height;
		float	floor;
		float	radius;
		float	flWeight;
		Vector	pos;
		Vector4D	q;
		bool	onWorld;
	} est; // estimate contact position

	struct x5
	{
		float	hipToFoot;	// distance from hip
		float	hipToKnee;	// distance from hip to knee
		float	kneeToFoot;	// distance from knee to foot
		Vector	hip;		// location of hip
		Vector	closest;	// closest valid location from hip to foot that the foot can move to
		Vector	knee;		// pre-ik location of knee
		Vector	farthest;	// farthest valid location from hip to foot that the foot can move to
		Vector	lowest;		// lowest position directly below hip that the foot can drop to
	} trace;
private:
	// internally latched footset, position
	struct x1
	{
		bool	bNeedsLatch;
		bool	bHasLatch;
		float	influence;
		int	iFramecounter;
		int	owner;
		Vector	absOrigin;
		Vector	absAngles;
		Vector	pos;
		Vector4D	q;
		Vector	deltaPos;	// acculated error
		Vector4D	deltaQ;
		Vector	debouncePos;
		Vector4D	debounceQ;
	} latched;

	struct x6
	{
		float	flTime; // time last error was detected
		float	flErrorTime;
		float	ramp;
		bool	bInError;
	} error;

	friend class CIKContext;
};

typedef struct 
{
	// accumulated offset from ideal footplant location
	int		target;
	Vector		pos;
	Vector4D		q;
	float		flWeight;
} ikchainresult_t;

struct ikcontextikrule_t
{
	int		index;

	int		type;
	int		chain;

	int		bone;

	int		slot;	// iktarget slot.  Usually same as chain.
	float		height;
	float		radius;
	float		floor;
	Vector		pos;
	Vector4D	q;

	float		start;	// beginning of influence
	float		peak;	// start of full influence
	float		tail;	// end of full influence
	float		end;	// end of all influence

	float		top;
	float		drop;

	float		commit;		// frame footstep target should be committed
	float		release;	// frame ankle should end rotation from latched orientation

	float		flWeight;		// processed version of start-end cycle
	float		flRuleWeight;	// blending weight
	float		latched;		// does the IK rule use a latched value?
	int		iAttachment;

	Vector		kneeDir;
	Vector		kneePos;

	ikcontextikrule_t() {}

private:
	// No copy constructors allowed
	ikcontextikrule_t(const ikcontextikrule_t& vOther);
};

class CIKContext 
{
public:
	CIKContext( );
	void Init( const CStudioBoneSetup *pBoneSetup, const Vector &angles, const Vector &pos, float flTime, int iFramecounter );
	void AddDependencies( mstudioseqdesc_t *pseqdesc, int iSequence, float flCycle, float flWeight = 1.0f );

	void ClearTargets( void );
	void UpdateTargets( Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet = NULL );
	void AutoIKRelease( void );
	void SolveDependencies( Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet = NULL );

	void AddAutoplayLocks( Vector pos[], Vector4D q[] );
	void SolveAutoplayLocks( Vector pos[], Vector4D q[] );

	void AddSequenceLocks( mstudioseqdesc_t *pseqdesc, Vector pos[], Vector4D q[] );
	void SolveSequenceLocks( mstudioseqdesc_t *pseqdesc, Vector pos[], Vector4D q[] );
	
	void AddAllLocks( Vector pos[], Vector4D q[] );
	void SolveAllLocks( Vector pos[], Vector4D q[] );

	void SolveLock( const mstudioiklock_t *plock, int i, Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet = NULL );

	CUtlArrayFixed< CIKTarget, 12 >	m_target;

private:
	CStudioBoneSetup *m_pBoneSetup;

	bool Estimate( int iSequence, float flCycle, int iTarget, const float poseParameter[], float flWeight = 1.0f ); 
	void BuildBoneChain( const Vector pos[], const Vector4D q[], int iBone, matrix3x4 *pBoneToWorld, byte *pBoneSet = NULL );

	// virtual IK rules, filtered and combined from each sequence
	CUtlArray< CUtlArray< ikcontextikrule_t > > m_ikChainRule;
	CUtlArray< ikcontextikrule_t > m_ikLock;

	static matrix3x4	m_boneToWorld[MAXSTUDIOBONES];
	matrix3x4		m_rootxform;

	int m_iFramecounter;
	float m_flTime;
};

#endif//IKCONTEXT
