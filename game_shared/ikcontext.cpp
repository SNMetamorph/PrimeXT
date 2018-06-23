/*
ikcontext.cpp - Inverse Kinematic implementation
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

matrix3x4 CIKContext :: m_boneToWorld[MAXSTUDIOBONES];
	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CIKContext :: CIKContext()
{
	m_target.EnsureCapacity( 12 ); // FIXME: this sucks, shouldn't it be grown?
	m_iFramecounter = -1;
	m_pBoneSetup = NULL;
	m_flTime = -1.0f;
	m_target.SetSize( 0 );
}

void CIKContext :: Init( const CStudioBoneSetup *pBoneSetup, const Vector &angles, const Vector &pos, float flTime, int iFramecounter )
{
	m_ikChainRule.RemoveAll(); // m_numikrules = 0;
	m_pBoneSetup = (CStudioBoneSetup *)pBoneSetup;

	if( m_pBoneSetup->GetNumIKChains( ))
	{
		m_ikChainRule.SetSize( m_pBoneSetup->GetNumIKChains() );

		// FIXME: Brutal hackery to prevent a crash
		if( m_target.Count() == 0 )
		{
			m_target.SetSize( 12 );
			memset( m_target.Base(), 0, sizeof( m_target[0] ) * m_target.Count() );
			ClearTargets();
		}
	}
	else
	{
		m_target.SetSize( 0 );
	}

	m_rootxform = matrix3x4( pos, angles );
	m_iFramecounter = iFramecounter;
	m_flTime = flTime;
}

void CIKContext :: AddDependencies( mstudioseqdesc_t *pseqdesc, int iSequence, float flCycle, float flWeight )
{
	int i;

	if( m_pBoneSetup->GetNumIKChains() == 0 )
		return;

	if( !FBitSet( pseqdesc->flags, STUDIO_IKRULES ))
		return;

	ikcontextikrule_t ikrule;

	// this shouldn't be necessary, but the Assert should help us catch whoever is screwing this up
	flWeight = bound( 0.0f, flWeight, 1.0f );

	// unify this
	if( FBitSet( pseqdesc->flags, STUDIO_REALTIME ))
	{
		float cps = m_pBoneSetup->LocalCPS( iSequence );
		flCycle = m_flTime * cps;
		flCycle = flCycle - (int)flCycle;
	}
	else if( flCycle < 0 || flCycle >= 1 )
	{
		if( FBitSet( pseqdesc->flags, STUDIO_LOOPING ))
		{
			flCycle = flCycle - (int)flCycle;
			if( flCycle < 0.0f ) flCycle += 1.0f;
		}
		else
		{
			flCycle = bound( 0.0f, flCycle, 0.9999f );
		}
	}

	mstudioanimdesc_t	*panim[4];
	float		weight[4];

	m_pBoneSetup->LocalSeqAnims( iSequence, panim, weight );

	// g-cont. all the animations of current blend has equal set of ikrules and chains. see studiomdl->simplify.cpp for details
	for( i = 0; i < panim[0]->numikrules; i++ )
	{
		if( !m_pBoneSetup->IKSequenceError( iSequence, flCycle, i, panim, weight, &ikrule ))
			continue;

		// don't add rule if the bone isn't going to be calculated
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( ikrule.chain );
		int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

		if( !m_pBoneSetup->IsBoneUsed( bone ))
			continue;

		// or if its relative bone isn't going to be calculated
		if( ikrule.bone >= 0 && !m_pBoneSetup->IsBoneUsed( ikrule.bone ))
			continue;

		// FIXME: Brutal hackery to prevent a crash
		if( m_target.Count() == 0 )
		{
			m_target.SetSize( 12 );
			memset( m_target.Base(), 0, sizeof( m_target[0] ) * m_target.Count());
			ClearTargets();
		}

		ikrule.flRuleWeight = flWeight;

		if( ikrule.flRuleWeight * ikrule.flWeight > 0.999f )
		{
			if( ikrule.type != IK_UNLATCH )
			{
				// clear out chain if rule is 100%
				m_ikChainRule.Element( ikrule.chain ).RemoveAll( );
				if( ikrule.type == IK_RELEASE )
					continue;
			}
		}

 		int nIndex = m_ikChainRule.Element( ikrule.chain ).AddToTail( );
  		m_ikChainRule.Element( ikrule.chain ).Element( nIndex ) = ikrule;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: AddAutoplayLocks( Vector pos[], Vector4D q[] )
{
	// skip all array access if no autoplay locks.
	if( m_pBoneSetup->GetNumIKAutoplayLocks() == 0 )
	{
		return;
	}

	int ikOffset = m_ikLock.AddMultipleToTail( m_pBoneSetup->GetNumIKAutoplayLocks() );
	memset( &m_ikLock[ikOffset], 0, sizeof( ikcontextikrule_t ) * m_pBoneSetup->GetNumIKAutoplayLocks() );

	for( int i = 0; i < m_pBoneSetup->GetNumIKAutoplayLocks(); i++ )
	{
		const mstudioiklock_t *lock = m_pBoneSetup->pIKAutoplayLock( i );
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( lock->chain );
		int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

		// don't bother with iklock if the bone isn't going to be calculated
		if( !m_pBoneSetup->IsBoneUsed( bone ))
			continue;

		// eval current ik'd bone
		BuildBoneChain( pos, q, bone, m_boneToWorld );

		ikcontextikrule_t *ikrule = &m_ikLock[ikOffset + i];

		ikrule->chain = lock->chain;
		ikrule->type = IK_WORLD;
		ikrule->slot = i;

		ikrule->q = m_boneToWorld[bone].GetQuaternion();
		ikrule->pos = m_boneToWorld[bone].GetOrigin();

		// save off current knee direction
		if( m_pBoneSetup->pIKLink( pchain, 0 )->kneeDir.LengthSqr() > 0.0f )
		{
			const mstudioiklink_t *link0 = m_pBoneSetup->pIKLink( pchain, 0 );
			const mstudioiklink_t *link1 = m_pBoneSetup->pIKLink( pchain, 1 );

			ikrule->kneeDir = m_boneToWorld[link0->bone].VectorRotate( link0->kneeDir );
			ikrule->kneePos = m_boneToWorld[link1->bone].GetOrigin(); 
		}
		else
		{
			ikrule->kneeDir.Init( );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: AddSequenceLocks( mstudioseqdesc_t *pseqdesc, Vector pos[], Vector4D q[] )
{
	if( m_pBoneSetup->GetNumIKChains() == 0 )
	{
		return;
	}

	if( pseqdesc->numiklocks == 0 )
	{
		return;
	}

	int ikOffset = m_ikLock.AddMultipleToTail( pseqdesc->numiklocks );
	memset( &m_ikLock[ikOffset], 0, sizeof( ikcontextikrule_t ) * pseqdesc->numiklocks );

	for( int i = 0; i < pseqdesc->numiklocks; i++ )
	{
		const mstudioiklock_t *plock = m_pBoneSetup->pIKLock( pseqdesc, i );
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( plock->chain );
		int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

		// don't bother with iklock if the bone isn't going to be calculated
		if( !m_pBoneSetup->IsBoneUsed( bone ))
			continue;

		// eval current ik'd bone
		BuildBoneChain( pos, q, bone, m_boneToWorld );

		ikcontextikrule_t *ikrule = &m_ikLock[ikOffset+i];
		ikrule->chain = i;
		ikrule->slot = i;
		ikrule->type = IK_WORLD;

		ikrule->q = m_boneToWorld[bone].GetQuaternion();
		ikrule->pos = m_boneToWorld[bone].GetOrigin();

		// save off current knee direction
		if( m_pBoneSetup->pIKLink( pchain, 0 )->kneeDir.LengthSqr() > 0.0f )
		{
			const mstudioiklink_t *link0 = m_pBoneSetup->pIKLink( pchain, 0 );
			ikrule->kneeDir = m_boneToWorld[link0->bone].VectorRotate( link0->kneeDir );
		}
		else
		{
			ikrule->kneeDir.Init( );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: build boneToWorld transforms for a specific bone
//-----------------------------------------------------------------------------
void CIKContext :: BuildBoneChain( const Vector pos[], const Vector4D q[], int iBone, matrix3x4 *pBoneToWorld, byte *pBoneSet )
{
	m_pBoneSetup->BuildBoneChain( m_rootxform, pos, q, iBone, pBoneToWorld, pBoneSet );
}

//-----------------------------------------------------------------------------
// Purpose: Invalidate any IK locks.
//-----------------------------------------------------------------------------
void CIKContext :: ClearTargets( void )
{
	for( int i = 0; i < m_target.Count(); i++ )
	{
		m_target[i].latched.iFramecounter = -9999;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Run through the rules that survived and turn a specific bones boneToWorld 
//			transform into a pos and q in parents bonespace
//-----------------------------------------------------------------------------
void CIKContext :: UpdateTargets( Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet )
{
	int i, j;

	for( i = 0; i < m_target.Count(); i++ )
	{
		m_target[i].est.flWeight = 0.0f;
		m_target[i].est.latched = 1.0f;
		m_target[i].est.release = 1.0f;
		m_target[i].est.height = 0.0f;
		m_target[i].est.floor = 0.0f;
		m_target[i].est.radius = 0.0f;
		m_target[i].offset.pos.Init();
		m_target[i].offset.q.Init();
	}

	AutoIKRelease( );

	for( j = 0; j < m_ikChainRule.Count(); j++ )
	{
		for( i = 0; i < m_ikChainRule.Element( j ).Count(); i++ )
		{
			ikcontextikrule_t *pRule = &m_ikChainRule.Element( j ).Element( i );

			switch( pRule->type )
			{
			case IK_ATTACHMENT:
			case IK_GROUND:
				{
					matrix3x4	footTarget;
					CIKTarget *pTarget = &m_target[pRule->slot];
					pTarget->chain = pRule->chain;
					pTarget->type = pRule->type;

					if( pRule->type == IK_ATTACHMENT )
						pTarget->offset.attachmentIndex = pRule->iAttachment;
					else pTarget->offset.attachmentIndex = 0;

					if( pRule->flRuleWeight == 1.0f || pTarget->est.flWeight == 0.0f )
					{
						pTarget->offset.q = pRule->q;
						pTarget->offset.pos = pRule->pos;
						pTarget->est.height = pRule->height;
						pTarget->est.floor = pRule->floor;
						pTarget->est.radius = pRule->radius;
						pTarget->est.latched = pRule->latched * pRule->flRuleWeight;
						pTarget->est.release = pRule->release;
						pTarget->est.flWeight = pRule->flWeight * pRule->flRuleWeight;
					}
					else
					{
						QuaternionSlerp( pTarget->offset.q, pRule->q, pRule->flRuleWeight, pTarget->offset.q );
						pTarget->offset.pos = Lerp( pRule->flRuleWeight, pTarget->offset.pos, pRule->pos );
						pTarget->est.height = Lerp( pRule->flRuleWeight, pTarget->est.height, pRule->height );
						pTarget->est.floor = Lerp( pRule->flRuleWeight, pTarget->est.floor, pRule->floor );
						pTarget->est.radius = Lerp( pRule->flRuleWeight, pTarget->est.radius, pRule->radius );
						//pTarget->est.latched = Lerp( pRule->flRuleWeight, pTarget->est.latched, pRule->latched );
						pTarget->est.latched = Q_min( pTarget->est.latched, pRule->latched );
						pTarget->est.release = Lerp( pRule->flRuleWeight, pTarget->est.release, pRule->release );
						pTarget->est.flWeight = Lerp( pRule->flRuleWeight, pTarget->est.flWeight, pRule->flWeight );
					}

					if( pRule->type == IK_GROUND )
					{
						pTarget->latched.deltaPos.z = 0.0f;
						pTarget->est.pos.z = pTarget->est.floor + m_rootxform[3][2];
					}
				}
			break;
			case IK_UNLATCH:
				{
					CIKTarget *pTarget = &m_target[pRule->slot];
					if( pRule->latched > 0.0 ) pTarget->est.latched = 0.0f;
					else pTarget->est.latched = Q_min( pTarget->est.latched, 1.0f - pRule->flWeight );
				}
				break;
			case IK_RELEASE:
				{
					CIKTarget *pTarget = &m_target[pRule->slot];
					if( pRule->latched > 0.0f ) pTarget->est.latched = 0.0f;
					else pTarget->est.latched = Q_min( pTarget->est.latched, 1.0f - pRule->flWeight );
					pTarget->est.flWeight = (pTarget->est.flWeight) * (1.0f - pRule->flWeight * pRule->flRuleWeight);
				}
				break;
			}
		}
	}

	for( i = 0; i < m_target.Count(); i++ )
	{
		CIKTarget *pTarget = &m_target[i];

		if( pTarget->est.flWeight > 0.0 )
		{
			const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( pTarget->chain );
			int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

			// eval current ik'd bone
			BuildBoneChain( pos, q, bone, boneToWorld, pBoneSet );

			// xform IK target error into world space
			matrix3x4	local = matrix3x4( pTarget->offset.pos, pTarget->offset.q ).Invert();
			matrix3x4	worldFootpad = boneToWorld[bone].ConcatTransforms( local );

			if( pTarget->est.latched == 1.0f )
				pTarget->latched.bNeedsLatch = true;
			else pTarget->latched.bNeedsLatch = false;

			// disable latched position if it looks invalid
			if( m_iFramecounter < 0 || pTarget->latched.iFramecounter < m_iFramecounter - 1
			|| pTarget->latched.iFramecounter > m_iFramecounter )
			{
				pTarget->latched.bHasLatch = false;
				pTarget->latched.influence = 0.0;
			}

			pTarget->latched.iFramecounter = m_iFramecounter;

			// find ideal contact position
			pTarget->est.q = pTarget->ideal.q = worldFootpad.GetQuaternion();
			pTarget->est.pos = pTarget->ideal.pos = worldFootpad.GetOrigin();

			float latched = pTarget->est.latched;

			if( pTarget->latched.bHasLatch )
			{
				if( pTarget->est.latched == 1.0f )
				{
					// keep track of latch position error from ideal contact position
					pTarget->latched.deltaPos = pTarget->latched.pos - pTarget->est.pos;
					QuaternionSM( -1.0f, pTarget->est.q, pTarget->latched.q, pTarget->latched.deltaQ );
					pTarget->est.q = pTarget->latched.q;
					pTarget->est.pos = pTarget->latched.pos;
				}
				else if( pTarget->est.latched > 0.0f )
				{
					// ramp out latch differences during decay phase of rule
					if( latched > 0 && latched < pTarget->latched.influence )
					{
						// latching has decreased
						float dt = pTarget->latched.influence - latched;
						if( pTarget->latched.influence > 0.0 )
							dt = dt / pTarget->latched.influence;

						QuaternionScale( pTarget->latched.deltaQ, (1.0f - dt), pTarget->latched.deltaQ );
						pTarget->latched.deltaPos *= (1.0f - dt);
					}

					// move ideal contact position by latched error factor
					pTarget->est.pos = pTarget->est.pos + pTarget->latched.deltaPos;
					QuaternionMA( pTarget->est.q, 1, pTarget->latched.deltaQ, pTarget->est.q );
					pTarget->latched.q = pTarget->est.q;
					pTarget->latched.pos = pTarget->est.pos;
				}
				else
				{
					pTarget->latched.bHasLatch = false;
					pTarget->latched.q = pTarget->est.q;
					pTarget->latched.pos = pTarget->est.pos;
					pTarget->latched.deltaPos.Init();
					pTarget->latched.deltaQ.Init();
				}
				pTarget->latched.influence = latched;
			}

			// check for illegal requests
			Vector p1, p2, p3;

			p1 = boneToWorld[m_pBoneSetup->pIKLink( pchain, 0 )->bone].GetOrigin(); // hip
			p2 = boneToWorld[m_pBoneSetup->pIKLink( pchain, 1 )->bone].GetOrigin(); // knee
			p3 = boneToWorld[m_pBoneSetup->pIKLink( pchain, 2 )->bone].GetOrigin(); // foot

			float d1 = (p2 - p1).Length();
			float d2 = (p3 - p2).Length();

			if( pTarget->latched.bHasLatch )
			{
				float d4 = (p3 + pTarget->latched.deltaPos - p1).Length();

				// unstick feet when distance is too great
				if(( d4 < fabs( d1 - d2 ) || d4 * 0.95f > d1 + d2 ) && pTarget->est.latched > 0.2f )
				{
					pTarget->error.flTime = m_flTime;
				}

				// unstick feet when angle is too great
				if( pTarget->est.latched > 0.2f )
				{
					float d = fabs( pTarget->latched.deltaQ.w ) * 2.0f - 1.0f;

					// FIXME: cos(45), make property of chain
					if( d < 0.707f )
					{
						pTarget->error.flTime = m_flTime;
					}
				}
			}

			Vector dt = pTarget->est.pos - p1;
			pTarget->trace.hipToFoot = dt.Length();
			pTarget->trace.hipToKnee = d1;
			pTarget->trace.kneeToFoot = d2;
			pTarget->trace.hip = p1;
			pTarget->trace.knee = p2;
			dt = dt.Normalize();
			pTarget->trace.closest = p1 + dt * ( fabs( d1 - d2 ) * 1.01f);
			pTarget->trace.farthest = p1 + dt * (d1 + d2) * 0.99;
			pTarget->trace.lowest = p1 + Vector( 0, 0, -1.0f ) * (d1 + d2) * 0.99f;
			// pTarget->trace.endpos = pTarget->est.pos;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: insert release rules if the ik rules were in error
//-----------------------------------------------------------------------------
void CIKContext :: AutoIKRelease( void )
{
	int i;

	for( i = 0; i < m_target.Count(); i++ )
	{
		CIKTarget *pTarget = &m_target[i];

		float dt = m_flTime - pTarget->error.flTime;

		if( pTarget->error.bInError || dt < 0.5f )
		{
			if( !pTarget->error.bInError )
			{
				pTarget->error.ramp = 0.0; 
				pTarget->error.flErrorTime = pTarget->error.flTime;
				pTarget->error.bInError = true;
			}

			float ft = m_flTime - pTarget->error.flErrorTime;

			if( dt < 0.25f )
			{
				pTarget->error.ramp = Q_min( pTarget->error.ramp + ft * 4.0f, 1.0f );
			}
			else
			{
				pTarget->error.ramp = Q_max( pTarget->error.ramp - ft * 4.0f, 0.0f );
			}

			if( pTarget->error.ramp > 0.0f )
			{
				ikcontextikrule_t ikrule;

				ikrule.chain = pTarget->chain;
				ikrule.bone = 0;
				ikrule.type = IK_RELEASE;
				ikrule.slot = i;
				ikrule.flWeight = SimpleSpline( pTarget->error.ramp );
				ikrule.flRuleWeight = 1.0;
				ikrule.latched = dt < 0.25 ? 0.0 : ikrule.flWeight;

				// don't bother with AutoIKRelease if the bone isn't going to be calculated
				// this code is crashing for some unknown reason.
				if( pTarget->chain >= 0 && pTarget->chain < m_pBoneSetup->GetNumIKChains( ))
				{
					const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( pTarget->chain );

					if( pchain != NULL )
					{
						int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;
						if( bone >= 0 && bone < m_pBoneSetup->m_pStudioHeader->numbones )
						{
							if( !m_pBoneSetup->IsBoneUsed( bone ))
							{
								pTarget->error.bInError = false;
								continue;
							}

							int nIndex = m_ikChainRule.Element( ikrule.chain ).AddToTail( );
							m_ikChainRule.Element( ikrule.chain ).Element( nIndex ) = ikrule;
						}
						else
						{
							m_pBoneSetup->debugMsg( "^2Warning:^7 AutoIKRelease (%s) out of range bone %d (%d)\n",
							m_pBoneSetup->m_pStudioHeader->name, bone, m_pBoneSetup->m_pStudioHeader->numbones );
						}
					}
					else
					{
						m_pBoneSetup->debugMsg( "^2Warning:^7 AutoIKRelease (%s) got a NULL pchain %d\n",
						m_pBoneSetup->m_pStudioHeader->name, pTarget->chain );
					}
				}
				else
				{
					m_pBoneSetup->debugMsg( "^2Warning:^7 AutoIKRelease (%s) got an out of range chain %d (%d)\n",
					m_pBoneSetup->m_pStudioHeader->name, pTarget->chain, m_pBoneSetup->GetNumIKChains( ));
				}
			}
			else
			{
				pTarget->error.bInError = false;
			}

			pTarget->error.flErrorTime = m_flTime;
		}
	}
}

void CIKContext :: SolveDependencies( Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet )
{
	matrix3x4	worldTarget;
	int i, j;

	ikchainresult_t chainResult[32]; // allocate!!!

	// init chain rules
	for( i = 0; i < m_pBoneSetup->GetNumIKChains(); i++ )
	{
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( i );
		ikchainresult_t *pChainResult = &chainResult[i];
		int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

		pChainResult->target = -1;
		pChainResult->flWeight = 0.0;

		// don't bother with chain if the bone isn't going to be calculated
		if( !m_pBoneSetup->IsBoneUsed( bone ))
			continue;

		// eval current ik'd bone
		BuildBoneChain( pos, q, bone, boneToWorld, pBoneSet );

		pChainResult->q = boneToWorld[bone].GetQuaternion();
		pChainResult->pos = boneToWorld[bone].GetOrigin();
	}

	for( j = 0; j < m_ikChainRule.Count(); j++ )
	{
		for( i = 0; i < m_ikChainRule.Element( j ).Count(); i++ )
		{
			ikcontextikrule_t *pRule = &m_ikChainRule.Element( j ).Element( i );
			ikchainresult_t *pChainResult = &chainResult[pRule->chain];
			pChainResult->target = -1;

			switch( pRule->type )
			{
			case IK_SELF:
				{
					// xform IK target error into world space
					matrix3x4	local = matrix3x4( pRule->pos, pRule->q );

					// eval target bone space
					if( pRule->bone != -1 )
					{
						BuildBoneChain( pos, q, pRule->bone, boneToWorld, pBoneSet );
						worldTarget = boneToWorld[pRule->bone].ConcatTransforms( local );
					}
					else
					{
						worldTarget = m_rootxform.ConcatTransforms( local );
					}
			
					float flWeight = pRule->flWeight * pRule->flRuleWeight;
					pChainResult->flWeight = pChainResult->flWeight * (1 - flWeight) + flWeight;

					Vector	p2;
					Vector4D	q2;
					
					// target p and q
					q2 = worldTarget.GetQuaternion();
					p2 = worldTarget.GetOrigin();

					// m_pBoneSetup->debugLine( pChainResult->pos, p2, 0, 0, 255, true, 0.1 );

					// blend in position and angles
					pChainResult->pos = pChainResult->pos * (1.0f - flWeight) + p2 * flWeight;
					QuaternionSlerp( pChainResult->q, q2, flWeight, pChainResult->q );
				}
				break;
			case IK_WORLD:
				break;
			case IK_ATTACHMENT:
				break;
			case IK_GROUND:
				break;
			case IK_RELEASE:
				{
					// move target back towards original location
					float flWeight = pRule->flWeight * pRule->flRuleWeight;
					const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( pRule->chain );
					int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

					Vector	p2;
					Vector4D	q2;
					
					BuildBoneChain( pos, q, bone, boneToWorld, pBoneSet );
					q2 = boneToWorld[bone].GetQuaternion();
					p2 = boneToWorld[bone].GetOrigin();

					// blend in position and angles
					pChainResult->pos = pChainResult->pos * (1.0 - flWeight) + p2 * flWeight;
					QuaternionSlerp( pChainResult->q, q2, flWeight, pChainResult->q );
				}
				break;
			case IK_UNLATCH:
				{
					/*
					pChainResult->flWeight = pChainResult->flWeight * (1 - pRule->flWeight) + pRule->flWeight;

					pChainResult->pos = pChainResult->pos * (1.0 - pRule->flWeight ) + pChainResult->local.pos * pRule->flWeight;
					QuaternionSlerp( pChainResult->q, pChainResult->local.q, pRule->flWeight, pChainResult->q );
					*/
				}
				break;
			}
		}
	}

	for (i = 0; i < m_target.Count(); i++)
	{
		CIKTarget *pTarget = &m_target[i];

		if( m_target[i].est.flWeight > 0.0f )
		{
			ikchainresult_t *pChainResult = &chainResult[ pTarget->chain ];
			matrix3x4 local = matrix3x4( pTarget->offset.pos, pTarget->offset.q );
			matrix3x4	worldFootpad = matrix3x4( pTarget->est.pos, pTarget->est.q );
			worldTarget = worldFootpad.ConcatTransforms( local );

			Vector	p2;
			Vector4D	q2;

			// target p and q
			q2 = worldTarget.GetQuaternion();
			p2 = worldTarget.GetOrigin();

			// blend in position and angles
			pChainResult->flWeight = pTarget->est.flWeight;
			pChainResult->pos = pChainResult->pos * (1.0 - pChainResult->flWeight ) + p2 * pChainResult->flWeight;
			QuaternionSlerp( pChainResult->q, q2, pChainResult->flWeight, pChainResult->q );
		}

		if( pTarget->latched.bNeedsLatch )
		{
			// keep track of latch position
			pTarget->latched.bHasLatch = true;
			pTarget->latched.q = pTarget->est.q;
			pTarget->latched.pos = pTarget->est.pos;
		}
	}

	for( i = 0; i < m_pBoneSetup->GetNumIKChains(); i++ )
	{
		ikchainresult_t *pChainResult = &chainResult[ i ];
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( i );

		if( pChainResult->flWeight > 0.0f )
		{
			int bone0 = m_pBoneSetup->pIKLink( pchain, 0 )->bone;
			int bone1 = m_pBoneSetup->pIKLink( pchain, 1 )->bone;
			int bone2 = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

			Vector tmp = boneToWorld[bone2].GetOrigin();
			// m_pBoneSetup->debugLine( pChainResult->pos, tmp, 255, 255, 255, true, 0.1 );

			// do exact IK solution
			// FIXME: once per link!
			if( m_pBoneSetup->SolveIK( pchain, pChainResult->pos, boneToWorld ))
			{
				Vector p3 = boneToWorld[bone2].GetOrigin();
				// replace rotational component with IK result
				boneToWorld[bone2] = matrix3x4( p3, pChainResult->q );

				// rebuild chain
				// FIXME: is this needed if everyone past this uses the boneToWorld array?
				m_pBoneSetup->SolveBone( bone2, boneToWorld, pos, q );
				m_pBoneSetup->SolveBone( bone1, boneToWorld, pos, q );
				m_pBoneSetup->SolveBone( bone0, boneToWorld, pos, q );
			}
			else
			{
				// FIXME: need to invalidate the targets that forced this...
				if( pChainResult->target != -1 )
				{
					CIKTarget *pTarget = &m_target[pChainResult->target];
					QuaternionScale( pTarget->latched.deltaQ, 0.8f, pTarget->latched.deltaQ );
					pTarget->latched.deltaPos *= 0.8f;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: SolveAutoplayLocks( Vector pos[], Vector4D q[] )
{
	for( int i = 0; i < m_ikLock.Count(); i++ )
	{
		const mstudioiklock_t *lock = m_pBoneSetup->pIKAutoplayLock( i );
		SolveLock( lock, i, pos, q, m_boneToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: SolveSequenceLocks( mstudioseqdesc_t *pseqdesc, Vector pos[], Vector4D q[] )
{
	for( int i = 0; i < m_ikLock.Count(); i++ )
	{
		const mstudioiklock_t *plock = m_pBoneSetup->pIKLock( pseqdesc, i );
		SolveLock( plock, i, pos, q, m_boneToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: AddAllLocks( Vector pos[], Vector4D q[] )
{
	// skip all array access if no autoplay locks.
	if( m_pBoneSetup->GetNumIKChains() == 0 )
	{
		return;
	}

	int ikOffset = m_ikLock.AddMultipleToTail( m_pBoneSetup->GetNumIKChains() );
	memset( &m_ikLock[ikOffset], 0, sizeof( ikcontextikrule_t ) * m_pBoneSetup->GetNumIKChains() );

	for( int i = 0; i < m_pBoneSetup->GetNumIKChains(); i++ )
	{
		const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( i );
		int bone = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

		// don't bother with iklock if the bone isn't going to be calculated
		if( !m_pBoneSetup->IsBoneUsed( bone ))
			continue;

		// eval current ik'd bone
		BuildBoneChain( pos, q, bone, m_boneToWorld );

		ikcontextikrule_t *ikrule = &m_ikLock[ikOffset + i];

		ikrule->type = IK_WORLD;
		ikrule->chain = i;
		ikrule->slot = i;

		ikrule->q = m_boneToWorld[bone].GetQuaternion();
		ikrule->pos = m_boneToWorld[bone].GetOrigin();

		// save off current knee direction
		if( m_pBoneSetup->pIKLink( pchain, 0 )->kneeDir.LengthSqr() > 0.0f )
		{
			const mstudioiklink_t *link0 = m_pBoneSetup->pIKLink( pchain, 0 );
			ikrule->kneeDir = m_boneToWorld[link0->bone].VectorRotate( link0->kneeDir );
			ikrule->kneePos = m_boneToWorld[link0->bone].GetOrigin();

		}
		else
		{
			ikrule->kneeDir.Init( );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: SolveAllLocks( Vector pos[], Vector4D q[] )
{
	mstudioiklock_t	lock;

	for( int i = 0; i < m_ikLock.Count(); i++ )
	{
		lock.chain = i;
		lock.flPosWeight = 1.0f;
		lock.flLocalQWeight = 0.0f;
		lock.flags = 0;

		SolveLock( &lock, i, pos, q, m_boneToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext :: SolveLock( const mstudioiklock_t *plock, int i, Vector pos[], Vector4D q[], matrix3x4 boneToWorld[], byte *pBoneSet )
{
	const mstudioikchain_t *pchain = m_pBoneSetup->pIKChain( plock->chain );
	int bone0 = m_pBoneSetup->pIKLink( pchain, 0 )->bone;
	int bone1 = m_pBoneSetup->pIKLink( pchain, 1 )->bone;
	int bone2 = m_pBoneSetup->pIKLink( pchain, 2 )->bone;

	// don't bother with iklock if the bone isn't going to be calculated
	if( !m_pBoneSetup->IsBoneUsed( bone2 ))
		return;

	// eval current ik'd bone
	BuildBoneChain( pos, q, bone2, boneToWorld, pBoneSet );

	Vector	p1, p2, p3;
	Vector4D	q2, q3;

	// current p and q
	p1 = boneToWorld[bone2].GetOrigin();

	// blend in position
	p3 = p1 * (1.0 - plock->flPosWeight ) + m_ikLock[i].pos * plock->flPosWeight;

	// do exact IK solution
	if( m_ikLock[i].kneeDir.LengthSqr() > 0.0f )
		m_pBoneSetup->SolveIK( bone0, bone1, bone2, p3, m_ikLock[i].kneePos, m_ikLock[i].kneeDir, boneToWorld );
	else m_pBoneSetup->SolveIK(pchain, p3, boneToWorld );

	// slam orientation
	p3 = boneToWorld[bone2].GetOrigin();
	boneToWorld[bone2] = matrix3x4( p3, m_ikLock[i].q );

	// rebuild chain
	q2 = q[bone2];
	m_pBoneSetup->SolveBone( bone2, boneToWorld, pos, q );
	QuaternionSlerp( q[bone2], q2, plock->flLocalQWeight, q[bone2] );
	m_pBoneSetup->SolveBone( bone1, boneToWorld, pos, q );
	m_pBoneSetup->SolveBone( bone0, boneToWorld, pos, q );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CIKTarget :: SetOwner( int entindex, const Vector &pos, const Vector &angles )
{
	latched.owner = entindex;
	latched.absOrigin = pos;
	latched.absAngles = angles;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CIKTarget :: ClearOwner( void )
{
	latched.owner = -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIKTarget :: GetOwner( void )
{
	return latched.owner;
}

//-----------------------------------------------------------------------------
// Purpose: update the latched IK values that are in a moving frame of reference
//-----------------------------------------------------------------------------
void CIKTarget :: UpdateOwner( int entindex, const Vector &pos, const Vector &angles )
{
	if( pos == latched.absOrigin && angles == latched.absAngles )
		return;

	matrix3x4	in = matrix3x4( pos, angles );
	matrix3x4	out = matrix3x4( latched.absOrigin, latched.absAngles ).Invert();

	matrix3x4	tmp1 = matrix3x4( latched.pos, latched.q );
	matrix3x4 tmp2 = out.ConcatTransforms( tmp1 );
	tmp1 = in.ConcatTransforms( tmp2 );

	latched.q = tmp1.GetQuaternion(); 
	latched.pos = tmp1.GetOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: sets the ground position of an ik target
//-----------------------------------------------------------------------------
void CIKTarget :: SetPos( const Vector &pos )
{
	est.pos = pos;
}

//-----------------------------------------------------------------------------
// Purpose: sets the ground "identity" orientation of an ik target
//-----------------------------------------------------------------------------
void CIKTarget :: SetAngles( const Vector &angles )
{
	AngleQuaternion( angles, est.q );
}

//-----------------------------------------------------------------------------
// Purpose: sets the ground "identity" orientation of an ik target
//-----------------------------------------------------------------------------
void CIKTarget :: SetQuaternion( const Vector4D &q )
{
	est.q = q;
}

//-----------------------------------------------------------------------------
// Purpose: calculates a ground "identity" orientation based on the surface
//			normal of the ground and the desired ground identity orientation
//-----------------------------------------------------------------------------
void CIKTarget :: SetNormal( const Vector &normal )
{
	// recalculate foot angle based on slope of surface
	matrix3x4 m1 = matrix3x4( g_vecZero, est.q );
	Vector forward, right;

	right = m1.GetRight();
	forward = CrossProduct( right, normal );
	right = CrossProduct( normal, forward );

	m1.SetForward( forward );
	m1.SetRight( right );
	m1.SetUp( normal );

	est.q = m1.GetQuaternion();
}

//-----------------------------------------------------------------------------
// Purpose: estimates the ground impact at the center location assuming a the edge of 
//			an Z axis aligned disc collided with it the surface.
//-----------------------------------------------------------------------------
void CIKTarget :: SetPosWithNormalOffset( const Vector &pos, const Vector &normal )
{
	// assume it's a disc edge intersecting with the floor, so try to estimate the z location of the center
	est.pos = pos;

	if( normal.z > 0.9999f )
	{
		return;
	}
	else if( normal.z > 0.707 )
	{
		// clamp at 45 degrees
		// tan == sin / cos
		float tan = sqrt( 1.0f - normal.z * normal.z ) / normal.z;
		est.pos.z = est.pos.z - est.radius * tan;
	}
	else
	{
		est.pos.z = est.pos.z - est.radius;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKTarget :: SetOnWorld( bool bOnWorld )
{
	est.onWorld = bOnWorld;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CIKTarget :: IsActive( void )
{ 
	return (est.flWeight > 0.0f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKTarget :: IKFailed( void )
{
	latched.deltaPos.Init();
	latched.deltaQ.Init();
	latched.pos = ideal.pos;
	latched.q = ideal.q;
	est.latched = 0.0;
	est.flWeight = 0.0;
	est.onWorld = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKTarget :: MoveReferenceFrame( Vector &deltaPos, Vector &deltaAngles )
{
	est.pos -= deltaPos;
	latched.pos -= deltaPos;
	offset.pos -= deltaPos;
	ideal.pos -= deltaPos;
}
