//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#ifndef JIGGLEBONES_H
#define JIGGLEBONES_H

#include <assert.h>
#include "mathlib.h"
#include "studio.h"
#include "utllinkedlist.h"

//-----------------------------------------------------------------------------
/**
 * JiggleData is the instance-specific data for a jiggle bone
 */
struct JiggleData
{
	void Init( int initBone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
	{
		bone = initBone;

		lastUpdate = currenttime;

		basePos = initBasePos;
		baseLastPos = basePos;
		baseVel.Init();
		baseAccel.Init();

		tipPos = initTipPos;
		tipVel.Init();
		tipAccel.Init();

		lastLeft = Vector( 0, 0, 0 );

		lastBoingPos = initBasePos;
		boingDir = Vector( 0.0f, 0.0f, 1.0f );
		boingVelDir.Init();
		boingSpeed = 0.0f;
		boingTime = 0.0f;
	}

	int bone;

	float lastUpdate;	// based on gpGlobals->realtime

	Vector basePos;		// position of the base of the jiggle bone
	Vector baseLastPos;
	Vector baseVel;
	Vector baseAccel;

	Vector tipPos;		// position of the tip of the jiggle bone
	Vector tipVel;
	Vector tipAccel;
	Vector lastLeft;		// previous up vector

	Vector lastBoingPos;	// position of base of jiggle bone last update for tracking velocity
	Vector boingDir;		// current direction along which the boing effect is occurring
	Vector boingVelDir;		// current estimation of jiggle bone unit velocity vector for boing effect
	float boingSpeed;		// current estimation of jiggle bone speed for boing effect
	float boingTime;
};

class CJiggleBones
{
public:
	CJiggleBones() {}
	~CJiggleBones() { m_jiggleBoneState.RemoveAll(); }

	JiggleData *GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos );
	void BuildJiggleTransformations( int boneIndex, float currentime, const mstudiojigglebone_t *jiggleParams, const matrix3x4 &goalMX, matrix3x4 &boneMX );
private:
	CUtlLinkedList< JiggleData >	m_jiggleBoneState;
};

extern bool CalcProceduralBone( const studiohdr_t *pStudioHdr, int iBone, matrix3x4 *bonetransform );

#endif // C_JIGGLEBONES_H
