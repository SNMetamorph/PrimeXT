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
#ifndef GAME_SERVER_ENTITIES_ROPE_CROPE_H
#define GAME_SERVER_ENTITIES_ROPE_CROPE_H

class CRopeSegment;
class CRopeSample;

#define MAX_SEGMENTS	64
#define MAX_LIST_SEGMENTS	5

struct RopeSampleData;

/**
*	A rope with a number of segments.
*	Uses an RK4 integrator with dampened springs to simulate rope physics.
*/
class CRope : public CBaseDelay
{
public:
	DECLARE_CLASS( CRope, CBaseDelay );
	DECLARE_DATADESC();

	CRope();

	void KeyValue( KeyValueData* pkvd );

	void Precache();

	void Spawn();

	void Think();

	void ComputeForces( RopeSampleData* pSystem );
	void ComputeForces( CRopeSegment** ppSystem );
	void ComputeSampleForce( RopeSampleData& data );
	void ComputeSpringForce( RopeSampleData& first, RopeSampleData& second );
	void RK4Integrate( const float flDeltaTime );
	void TraceModels( void );
	bool MoveUp( const float flDeltaTime );
	bool MoveDown( const float flDeltaTime );
	Vector GetAttachedObjectsVelocity() const;
	void ApplyForceFromPlayer( const Vector& vecForce );
	void ApplyForceToSegment( const Vector& vecForce, const int iSegment );
	void AttachObjectToSegment( CRopeSegment* pSegment );
	void DetachObject();
	bool IsObjectAttached() const { return m_bObjectAttached; }
	bool IsAcceptingAttachment() const;
	int GetNumSegments() const { return m_iSegments; }
	CRopeSegment** GetSegments() { return m_pSegments; }
	bool IsSimulateBones() { return m_bSimulateBones; }
	bool IsSoundAllowed() const { return m_bMakeSound; }
	void SetSoundAllowed( const bool bAllowed )
	{
		m_bMakeSound = bAllowed;
	}

	bool ShouldCreak() const;
	string_t GetBodyModel() const { return (pev->modelindex) ? NULL_STRING : m_iszBodyModel; }
	string_t GetEndingModel() const { return (pev->modelindex) ? NULL_STRING : m_iszEndingModel; }
	void GetAlignmentAngles( const Vector& vecTop, const Vector& vecBottom, Vector& vecOut );
	float GetSegmentLength( int iSegmentIndex ) const;
	float GetRopeLength() const;
	Vector GetRopeOrigin() const;
	bool IsValidSegmentIndex( const int iSegment ) const;
	Vector GetSegmentOrigin( const int iSegment ) const;
	Vector GetSegmentAttachmentPoint( const int iSegment ) const;
	void SetAttachedObjectsSegment( CRopeSegment* pSegment );
	Vector GetSegmentDirFromOrigin( const int iSegmentIndex ) const;
	Vector GetAttachedObjectsPosition() const;
	void SetSegmentOrigin( CRopeSegment *pCurr, CRopeSegment *pNext );
	void SetSegmentAngles( CRopeSegment *pCurr, CRopeSegment *pNext );
	void SendUpdateBones( void );
private:
	int m_iSegments;
	int m_iNumSamples;

	Vector m_vecLastEndPos;
	Vector m_vecGravity;

	CRopeSegment* m_pSegments[MAX_SEGMENTS];
	int m_iAttachedObjectsSegment;
	bool m_bDisallowPlayerAttachment;
	float m_flAttachedObjectsOffset;
	bool m_bObjectAttached;
	float m_flDetachTime;

	string_t m_iszBodyModel;
	string_t m_iszEndingModel;
	bool m_bSimulateBones;
	bool m_bMakeSound;
};

#endif //GAME_SERVER_ENTITIES_ROPE_CROPE_H
