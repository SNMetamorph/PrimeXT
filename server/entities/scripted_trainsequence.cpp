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

#include "scripted_trainsequence.h"
#include "func_train.h"

LINK_ENTITY_TO_CLASS( scripted_trainsequence, CTrainSequence );

BEGIN_DATADESC( CTrainSequence )
	DEFINE_KEYFIELD( m_iszEntity, FIELD_STRING, "m_iszEntity" ),
	DEFINE_KEYFIELD( m_iszDestination, FIELD_STRING, "m_iszDestination" ),
	DEFINE_KEYFIELD( m_iszTerminate, FIELD_STRING, "m_iszTerminate" ),
	DEFINE_KEYFIELD( m_iDirection, FIELD_INTEGER, "m_iDirection" ),
	DEFINE_FIELD( m_pDestination, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pTrain, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( EndThink ),
END_DATADESC()

void CTrainSequence :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iDirection" ))
	{
		m_iDirection = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszEntity" ))
	{
		m_iszEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszDestination" ))
	{
		m_iszDestination = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszTerminate" ))
	{
		m_iszTerminate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CTrainSequence :: EndThink( void )
{
	// the sequence has expired. Release control.
	StopSequence();
	UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
}

void CTrainSequence :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType ))
		return;

	if( GetState() == STATE_OFF )
	{
		// start the sequence, take control of the train

		CBaseEntity *pEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_iszEntity ), pActivator );

		if( pEnt )
		{
			m_pDestination = UTIL_FindEntityByTargetname( NULL, STRING( m_iszDestination ), pActivator );

			if( FBitSet( pev->spawnflags, SF_TRAINSEQ_DEBUG ))
			{
				ALERT( at_console, "trainsequence \"%s\" found train \"%s\"", GetTargetname(), pEnt->GetTargetname());

				if( m_pDestination )
					ALERT( at_console, "found destination %s\n", m_pDestination->GetTargetname());
				else
					ALERT( at_console, "missing destination\n" );
			}

			if( FStrEq( pEnt->GetClassname(), "func_train" ))
			{
				CFuncTrain *pTrain = (CFuncTrain*)pEnt;

				// check whether it's being controlled by another sequence
				if( pTrain->m_pSequence )
					return;

				// ok, we can now take control of it.
				pTrain->StartSequence( this );
				m_pTrain = pTrain;

				if( FBitSet( pev->spawnflags, SF_TRAINSEQ_DIRECT ))
				{
					pTrain->pev->target = m_pDestination->pev->targetname;
					pTrain->Next();
				}
				else
				{
					int iDir = DIRECTION_NONE;

					switch( m_iDirection )
					{
					case DIRECTION_DESTINATION:
						if( m_pDestination )
						{
							Vector vecFTemp, vecBTemp;
							CBaseEntity *pTrainDest = UTIL_FindEntityByTargetname( NULL, pTrain->GetMessage( ));
							float fForward;

							if( !pTrainDest )
							{
								// this shouldn't happen.
								ALERT(at_error, "Found no path to reach destination! (train has t %s, m %s; dest is %s)\n",
								pTrain->GetTarget(), pTrain->GetMessage(), m_pDestination->GetTargetname( ));
								return;
							}

							if( FBitSet( pTrain->pev->spawnflags, SF_TRAIN_SETORIGIN ))
								fForward = (pTrainDest->GetLocalOrigin() - pTrain->GetLocalOrigin()).Length();
							else fForward = (pTrainDest->GetLocalOrigin() - (pTrain->GetLocalOrigin() + (pTrain->pev->maxs + pTrain->pev->mins) * 0.5f )).Length();
							float fBackward = -fForward; // the further back from the TrainDest entity we are, the shorter the backward distance.

							CBaseEntity *pCurForward = pTrainDest;
							CBaseEntity *pCurBackward = m_pDestination;
							vecFTemp = pCurForward->GetLocalOrigin();
							vecBTemp = pCurBackward->GetLocalOrigin();
							int loopbreaker = 10;

							while( iDir == DIRECTION_NONE )
							{
								if (pCurForward)
								{
									fForward += (pCurForward->GetLocalOrigin() - vecFTemp).Length();
									vecFTemp = pCurForward->GetLocalOrigin();

									// if we've finished tracing the forward line
									if( pCurForward == m_pDestination )
									{
										// if the backward line is longest
										if( fBackward >= fForward || pCurBackward == NULL )
											iDir = DIRECTION_FORWARDS;
									}
									else
									{
										pCurForward = pCurForward->GetNextTarget();
									}
								}

								if( pCurBackward )
								{
									fBackward += (pCurBackward->GetLocalOrigin() - vecBTemp).Length();
									vecBTemp = pCurBackward->GetLocalOrigin();

									// if we've finished tracng the backward line
									if( pCurBackward == pTrainDest )
									{
										// if the forward line is shorter
										if( fBackward < fForward || pCurForward == NULL )
											iDir = DIRECTION_BACKWARDS;
									}
									else
									{
										pCurBackward = pCurBackward->GetNextTarget();
									}
								}

								if( --loopbreaker <= 0 )
									iDir = DIRECTION_STOP;
							}
						}
						else
						{
							iDir = DIRECTION_STOP;
						}
						break;
					case DIRECTION_FORWARDS:
						iDir = DIRECTION_FORWARDS;
						break;
					case DIRECTION_BACKWARDS:
						iDir = DIRECTION_BACKWARDS;
						break;
					case DIRECTION_STOP:
						iDir = DIRECTION_STOP;
						break;
					}
					
					if( iDir == DIRECTION_BACKWARDS && !FBitSet( pTrain->pev->spawnflags, SF_TRAIN_REVERSE ))
					{
						// change direction
						SetBits( pTrain->pev->spawnflags, SF_TRAIN_REVERSE );

						CBaseEntity *pSearch = m_pDestination;

						while( pSearch )
						{
							if( FStrEq( pSearch->GetTarget(), pTrain->GetMessage()))
							{
								CBaseEntity *pTrainTarg = pSearch->GetNextTarget();
								if( pTrainTarg )
									pTrain->pev->enemy = pTrainTarg->edict();
								else pTrain->pev->enemy = NULL;

								pTrain->pev->target = pSearch->pev->targetname;
								break;
							}

							pSearch = pSearch->GetNextTarget();
						}

						if( !pSearch )
						{
							// this shouldn't happen.
							ALERT(at_error, "Found no path to reach destination! (train has t %s, m %s; dest is %s)\n",
							pTrain->GetTarget(), pTrain->GetMessage(), m_pDestination->GetTargetname( ));
							return;
						}

						// we haven't reached the corner, so don't use its settings
						pTrain->m_hCurrentTarget = NULL;
						pTrain->Next();
					}
					else if( iDir == DIRECTION_FORWARDS )
					{
						pTrain->pev->target = pTrain->pev->message;
						pTrain->Next();
					}
					else if( iDir == DIRECTION_STOP )
					{
						SetThink( &CTrainSequence::EndThink );
						SetNextThink( 0.1f );
						return;
					}
				}
			}
			else
			{
				ALERT( at_error, "scripted_trainsequence %s can't affect %s \"%s\": not a train!\n", GetTargetname(), pEnt->GetClassname(), pEnt->GetTargetname( ));
				return;
			}
		}
		else // no entity with that name
		{
			ALERT(at_error, "Missing train \"%s\" for scripted_trainsequence %s!\n", STRING( m_iszEntity ), GetTargetname( ));
			return;
		}
	}
	else // prematurely end the sequence
	{
		// disable the other end conditions
		DontThink();

		// release control of the train
		StopSequence();
	}
}

void CTrainSequence :: StopSequence( void )
{
	if( m_pTrain )
	{
		m_pTrain->StopSequence();
		m_pTrain = NULL;

		if( FBitSet( pev->spawnflags, SF_TRAINSEQ_REMOVE ))
			UTIL_Remove( this );
	}
	else
	{
		ALERT( at_error, "scripted_trainsequence: StopSequence without a train!?\n" );
		return; // this shouldn't happen.
	}

	UTIL_FireTargets( STRING( m_iszTerminate ), this, this, USE_TOGGLE, 0 );
}

void CTrainSequence :: ArrivalNotify( void )
{
	// check whether the current path is our destination,
	// and end the sequence if it is.

	if( m_pTrain )
	{
		if( m_pTrain->m_hCurrentTarget == m_pDestination )
		{
			// we've reached the destination. Stop now.
			EndThink();
		}
	}
	else
	{
		ALERT( at_error, "scripted_trainsequence: ArrivalNotify without a train!?\n" );
		return; // this shouldn't happen.
	}
}
