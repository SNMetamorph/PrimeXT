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

#include "extdll.h"
#include "util.h"

#include "studio.h"
#include "bs_defs.h"
#include "r_studioint.h"

#ifndef ACTIVITY_H
#include "activity.h"
#endif

#include "activitymap.h"

#ifndef ANIMATION_H
#include "animation.h"
#endif

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#include "cbase.h"

// Global engine <-> studio model rendering code interface
float m_poseparameter[MAXSTUDIOPOSEPARAM]; // stub
server_studio_api_t IEngineStudio;

class CBaseBoneSetup : public CStudioBoneSetup
{
	model_t *m_pSubModel;
public:
	virtual void debugMsg( char *szFmt, ... )
	{
		char	buffer[2048];	// must support > 1k messages
		va_list	args;

		va_start( args, szFmt );
		Q_vsnprintf( buffer, 2048, szFmt, args );
		va_end( args );

		ALERT( at_console, "%s", buffer );
	}

	virtual mstudioanim_t *GetAnimSourceData( mstudioseqdesc_t *pseqdesc )
	{
		mstudioseqgroup_t *pseqgroup;
		cache_user_t *paSequences;

		pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

		if( pseqdesc->seqgroup == 0 )
			return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqdesc->animindex);

		assert( m_pSubModel ); // assume model is set

		paSequences = (cache_user_t *)m_pSubModel->submodels;

		if( paSequences == NULL )
		{
			paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc( MAXSTUDIOGROUPS, sizeof( cache_user_t ));
			m_pSubModel->submodels = (dmodel_t *)paSequences;
		}

		// check for already loaded
		if( !IEngineStudio.Cache_Check(( struct cache_user_s *)&(paSequences[pseqdesc->seqgroup] )))
		{
			char filepath[128], modelpath[128], modelname[64];

			COM_FileBase( m_pSubModel->name, modelname );
			COM_ExtractFilePath( m_pSubModel->name, modelpath );

			// NOTE: here we build real sub-animation filename because stupid user may rename model without recompile
			Q_snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

			ALERT( at_console, "loading: %s\n", filepath );
			IEngineStudio.LoadCacheFile( filepath, (struct cache_user_s *)&paSequences[pseqdesc->seqgroup] );			
		}

		return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
	}

	void SetBaseModel( model_t *mod ) { m_pSubModel = mod; }
};

static CBaseBoneSetup g_boneSetup;

//================================================================================================
//			HUD_GetStudioModelInterface
//	Export this function for the engine to use the studio renderer class to render objects.
//================================================================================================
int Server_GetBlendingInterface( int version, sv_blending_interface_t **ppinterface, server_studio_api_t *pstudio, float (*transform)[3][4], float (*bones)[MAXSTUDIOBONES][3][4] )
{
	if( version != SV_BLENDING_INTERFACE_VERSION  )
		return 0;

	ALERT( at_aiconsole, "Server_GetBlendingInterface()\n" );

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ));

	// Success
	return 1;
}

void SetupModelBones( studiohdr_t *header )
{
	g_boneSetup.SetStudioPointers( header, m_poseparameter );
}

void CalcDefaultPoseParameters( void *pmodel, float *poseparams )
{
	studiohdr_t *pstudiohdr;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return;

	g_boneSetup.SetStudioPointers( pstudiohdr, poseparams );
	g_boneSetup.CalcDefaultPoseParameters( poseparams );
}

CStudioBoneSetup *GetBaseBoneSetup( int modelindex, float *poseparams )
{
	studiohdr_t *pstudiohdr;

	model_t *mod = (model_t *)MODEL_HANDLE( modelindex );

	if( !mod || mod->type != mod_studio )
		return NULL;

	if( !( pstudiohdr = (studiohdr_t *)mod->cache.data ))
		return NULL;

	g_boneSetup.SetBaseModel( mod );
	g_boneSetup.SetStudioPointers( pstudiohdr, poseparams );

	return &g_boneSetup;
}

//=========================================================
//=========================================================
int LookupPoseParameter( void *pmodel, const char *szName, float *poseparams )
{
	studiohdr_t *pstudiohdr;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return -1;

	g_boneSetup.SetStudioPointers( pstudiohdr, poseparams );

	for( int i = 0; i < g_boneSetup.CountPoseParameters(); i++ )
	{
		if( !Q_stricmp( g_boneSetup.pPoseParameter( i )->name, szName ))
			return i;
	}

	return -1; // Error
}

void SetPoseParameter( void *pmodel, int iParameter, float flValue, float *poseparams )
{
	studiohdr_t *pstudiohdr;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return;

	g_boneSetup.SetStudioPointers( pstudiohdr, poseparams );
	g_boneSetup.SetPoseParameter( iParameter, flValue, poseparams[iParameter] );
}

float GetPoseParameter( void *pmodel, int iParameter, float *poseparams )
{
	studiohdr_t *pstudiohdr;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return 0.0f;

	g_boneSetup.SetStudioPointers( pstudiohdr, poseparams );
	return g_boneSetup.GetPoseParameter( iParameter, poseparams[iParameter] );
}

int FindHitboxSetByName( void *pmodel, const char *name )
{
	studiohdr_t *pstudiohdr;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return -1;

	SetupModelBones( pstudiohdr );

	for( int i = 0; i < g_boneSetup.GetNumHitboxSets(); i++ )
	{
		mstudiohitboxset_t *set = g_boneSetup.pHitboxSet( i );

		if( !set ) continue;

		if( !Q_stricmp( set->name, name ))
			return i;
	}

	return -1;
}

int ExtractBbox( void *pmodel, int sequence, Vector &mins, Vector &maxs )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return 0;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	
	mins = pseqdesc[sequence].bbmin;
	maxs = pseqdesc[sequence].bbmax;

	return 1;
}

int LookupActivity( void *pmodel, int activity )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return ACTIVITY_NOT_AVAILABLE;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weighttotal = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity )
		{
			weighttotal += pseqdesc[i].actweight;

			if( !weighttotal || RANDOM_LONG( 0, weighttotal - 1 ) < pseqdesc[i].actweight )
				seq = i;
		}
	}

	return seq;
}

int LookupActivityHeaviest( void *pmodel, int activity )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return ACTIVITY_NOT_AVAILABLE;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity )
		{
			if( pseqdesc[i].actweight > weight )
			{
				weight = pseqdesc[i].actweight;
				seq = i;
			}
		}
	}

	return seq;
}

int GetEyePosition( void *pmodel, Vector &vecEyePosition )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return 0;

	vecEyePosition = pstudiohdr->eyeposition;

	return 1;
}

int LookupSequence( void *pmodel, const char *label )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return -1;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( !Q_stricmp( pseqdesc[i].label, label ))
			return i;
	}

	return -1;
}

int IsSoundEvent( int eventNumber )
{
	if( eventNumber == SCRIPT_EVENT_SOUND || eventNumber == SCRIPT_EVENT_SOUND_VOICE )
		return 1;
	return 0;
}

void SequencePrecache( void *pmodel, const char *pSequenceName )
{
	int index = LookupSequence( pmodel, pSequenceName );

	if( index >= 0 )
	{
		studiohdr_t *pstudiohdr;
	
		pstudiohdr = (studiohdr_t *)pmodel;

		if( !pstudiohdr || index >= pstudiohdr->numseq )
			return;

		mstudioseqdesc_t *pseqdesc;
		mstudioevent_t *pevent;

		pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + index;
		pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

		for( int i = 0; i < pseqdesc->numevents; i++ )
		{
			// don't send client-side events to the server AI
			if( pevent[i].event >= EVENT_CLIENT )
				continue;

			if( IsSoundEvent( pevent[i].event ))
			{
				if( !Q_strlen(pevent[i].options ))
				{
					ALERT( at_error, "Bad sound event %d in sequence %s :: %s (sound is \"%s\")\n",
					pevent[i].event, pstudiohdr->name, pSequenceName, pevent[i].options );
				}

				PRECACHE_SOUND( pevent[i].options );
			}
		}
	}
}

void CalcGaitFrame( void *pmodel, int &gaitsequence, float &flGaitFrame, float flGaitMovement, int numframes )
{
	studiohdr_t *pstudiohdr;
	mstudioseqdesc_t* pseqdesc;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return;	

	if( gaitsequence < 0 || gaitsequence >= pstudiohdr->numseq ) 
		gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0.0f )
		flGaitFrame += (flGaitMovement / pseqdesc->linearmovement[0]) * numframes;
	else flGaitFrame += pseqdesc->fps * gpGlobals->frametime;

	// do modulo
	flGaitFrame = fmod( flGaitFrame, (float)numframes);
	while( flGaitFrame < 0.0 ) flGaitFrame += numframes;
}

void GetSequenceInfo( void *pmodel, int sequence, float *pflFrameRate, float *pflGroundSpeed )
{
	studiohdr_t *pstudiohdr;
	Vector vecMove, vecAngle;

	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return;	

	SetupModelBones( pstudiohdr );

	mstudioseqdesc_t *pseqdesc;

	if( sequence >= pstudiohdr->numseq )
	{
		*pflFrameRate = 0.0f;
		*pflGroundSpeed = 0.0f;
		return;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;
	g_boneSetup.SeqMovement( sequence, 0.0f, 1.0f, vecMove, vecAngle );

	if( pseqdesc->numframes > 1 )
	{
		*pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
		*pflGroundSpeed = vecMove.Length();
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 256.0f;
		*pflGroundSpeed = 0.0f;
	}
}

int GetSequenceFlags( void *pmodel, int sequence )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;

	if( !pstudiohdr || sequence >= pstudiohdr->numseq )
		return 0;

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;

	return pseqdesc->flags;
}

int GetAnimationEvent( void *pmodel, int sequence, MonsterEvent_t *pMonsterEvent, float flStart, float flEnd, int index )
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if( !pstudiohdr || sequence >= pstudiohdr->numseq || !pMonsterEvent )
		return 0;

	int events = 0;
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;
	mstudioevent_t *pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

	if( pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if( pseqdesc->numframes > 1 )
	{
		flStart *= (pseqdesc->numframes - 1) / 255.0f;
		flEnd *= (pseqdesc->numframes - 1) / 255.0f;
	}
	else
	{
		flStart = 0.0f;
		flEnd = 1.0f;
	}

	for( ; index < pseqdesc->numevents; index++ )
	{
		// Don't send client-side events to the server AI
		if( pevent[index].event >= EVENT_CLIENT )
			continue;

		if(( pevent[index].frame >= flStart && pevent[index].frame < flEnd ) || ((pseqdesc->flags & STUDIO_LOOPING) && flEnd >= pseqdesc->numframes - 1 && pevent[index].frame < flEnd - pseqdesc->numframes + 1) )
		{
			pMonsterEvent->event = pevent[index].event;
			pMonsterEvent->options = pevent[index].options;
			return index + 1;
		}
	}

	return 0;
}

float SetController( void *pmodel, byte *controller, int iController, float flValue )
{
	int i;
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return flValue;

	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)pstudiohdr + pstudiohdr->bonecontrollerindex);

	// find first controller that matches the index
	for( i = 0; i < pstudiohdr->numbonecontrollers; i++, pbonecontroller++ )
	{
		if( pbonecontroller->index == iController )
			break;
	}

	if( i >= pstudiohdr->numbonecontrollers )
		return flValue;

	// wrap 0..360 if it's a rotational controller
	if( pbonecontroller->type & ( STUDIO_XR|STUDIO_YR|STUDIO_ZR ))
	{
		// ugly hack, invert value if end < start
		if( pbonecontroller->end < pbonecontroller->start )
			flValue = -flValue;

		// does the controller not wrap?
		if( pbonecontroller->start + 359.0f >= pbonecontroller->end )
		{
			if( flValue > (( pbonecontroller->start + pbonecontroller->end ) / 2.0f ) + 180 )
				flValue = flValue - 360;
			if( flValue < (( pbonecontroller->start + pbonecontroller->end ) / 2.0f ) - 180 )
				flValue = flValue + 360;
		}
		else
		{
			if( flValue > 360.0f )
				flValue = flValue - (int)(flValue / 360.0f) * 360.0f;
			else if( flValue < 0.0f )
				flValue = flValue + (int)((flValue / -360.0f) + 1.0f) * 360.0f;
		}
	}

	int setting = 255 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);

	setting = bound( 0, setting, 255 );
	controller[iController] = setting;

	return setting * (1.0f / 255.0f) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

float SetBlending( void *pmodel, int sequence, byte *blending, int iBlender, float flValue )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return flValue;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;

	if( pseqdesc->blendtype[iBlender] == 0 )
		return flValue;

	if( pseqdesc->blendtype[iBlender] & ( STUDIO_XR|STUDIO_YR|STUDIO_ZR ))
	{
		// ugly hack, invert value if end < start
		if( pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender] )
			flValue = -flValue;

		// does the controller not wrap?
		if( pseqdesc->blendstart[iBlender] + 359.0f >= pseqdesc->blendend[iBlender] )
		{
			if( flValue > (( pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender] ) / 2.0f ) + 180.0f )
				flValue = flValue - 360.0f;
			if( flValue < (( pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender] ) / 2.0f ) - 180.0f )
				flValue = flValue + 360.0f;
		}
	}

	int setting = 255 * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]);

	setting = bound( 0, setting, 255 );
	blending[iBlender] = setting;

	return setting * (1.0f / 255.0f) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];
}

int FindTransition( void *pmodel, int iEndingAnim, int iGoalAnim, int *piDir )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return iGoalAnim;

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	// bail if we're going to or from a node 0
	if( pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0 )
	{
		return iGoalAnim;
	}

	int iEndNode;

	// ALERT( at_console, "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	if( *piDir > 0 )
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if( iEndNode == pseqdesc[iGoalAnim].entrynode )
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte *pTransition = ((byte *)pstudiohdr + pstudiohdr->transitionindex);

	int iInternNode = pTransition[(iEndNode-1)*pstudiohdr->numtransitions + (pseqdesc[iGoalAnim].entrynode-1)];

	if( iInternNode == 0 )
		return iGoalAnim;

	// look for someone going
	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode )
		{
			*piDir = 1;
			return i;
		}

		if( pseqdesc[i].nodeflags )
		{
			if( pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode )
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT( at_console, "error in transition graph" );

	return iGoalAnim;
}

void SetBodygroup( void *pmodel, int &iBody, int iGroup, int iValue )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return;

	if( iGroup > pstudiohdr->numbodyparts )
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if( iValue >= pbodypart->nummodels )
		return;

	int iCurrent = (iBody / pbodypart->base) % pbodypart->nummodels;
	iBody = (iBody - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}

int GetBodygroup( void *pmodel, int iBody, int iGroup )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return 0;

	if( iGroup > pstudiohdr->numbodyparts )
		return 0;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;
	if( pbodypart->nummodels <= 1 )
		return 0;

	int iCurrent = (iBody / pbodypart->base) % pbodypart->nummodels;

	return iCurrent;
}

int FindAttachmentByName( void *pmodel, const char *pName )
{
	studiohdr_t *pstudiohdr;
	
	if( !( pstudiohdr = (studiohdr_t *)pmodel ))
		return 0;

	for( int i = 0; i < pstudiohdr->numattachments; i++ )
	{
		mstudioattachment_t	*pattachment = (mstudioattachment_t *) ((byte *)pstudiohdr + pstudiohdr->attachmentindex);

		if( !Q_stricmp( pattachment[i].name, pName ))
			return i;

	}

	return -1;
}
