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
#ifndef GAME_SERVER_ENTITIES_ROPE_CROPESEGMENT_H
#define GAME_SERVER_ENTITIES_ROPE_CROPESEGMENT_H

class CRope;

struct RopeSampleData
{
	Vector	mPosition;
	Vector	mVelocity;
	Vector	mForce;
	Vector	mExternalForce;

	bool	mApplyExternalForce;
	float	mMassReciprocal;
	float	restLength;
};

/**
*	Represents a visible rope segment.
*	Segments require models to define an attachment 0 that, when the segment origin is subtracted from it, represents the length of the segment.
*/
class CRopeSegment : public CBaseAnimating
{
public:
	DECLARE_CLASS( CRopeSegment, CBaseAnimating );
	DECLARE_DATADESC();

	CRopeSegment();

	void Precache();

	void Spawn();

	void Touch( CBaseEntity* pOther );

	static CRopeSegment* CreateSegment( string_t iszModelName, float flRecipMass );
	
	RopeSampleData* GetSample() { return &m_Data; }

	void ApplyExternalForce( const Vector& vecForce );
	void SetMass( const float flMass );
	void SetCauseDamageOnTouch( const bool bCauseDamage );
	void SetCanBeGrabbed( const bool bCanBeGrabbed );
	CRope* GetMasterRope() { return m_pMasterRope; }
	void SetMasterRope( CRope* pRope );
	RopeSampleData m_Data;
private:
	CRope*		m_pMasterRope;
	string_t		m_iszModelName;
	bool		m_bCauseDamage;
	bool		m_bCanBeGrabbed;
};

#endif //GAME_SERVER_ENTITIES_ROPE_CROPESEGMENT_H
