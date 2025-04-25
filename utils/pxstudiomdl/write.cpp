#pragma warning( disable : 4244 )
#pragma warning( disable : 4237 )
#pragma warning( disable : 4305 )

#include "port.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "studio.h"
#include "studiomdl.h"
#include "file_system.h"
#include "app_info.h"
#include "build_info.h"

int totalframes = 0;
float totalseconds = 0;
extern int numcommandnodes;

// bounds checking
byte *pData;
byte *pStart;
studiohdr_t *phdr = NULL;
studiohdr2_t *phdr2 = NULL;
studioseqhdr_t *pseqhdr;

#define ALIGN( a )		a = (byte *)((ptrdiff_t)((byte *)a + 3) & ~ 3)
#define FILEBUFFER		(48 * 1024 * 1024)	// 48 Mb for now
#define TRUNCNAME		"ValveBiped."

mstudioboneinfo_t		g_boneinfo[MAXSTUDIOBONES];
	
void SetupBoneInfo( int bone, mstudioboneinfo_t *out )
{
	matrix3x4	m = g_bonetable[bone].boneToPose.Invert();

	memset( out, 0, sizeof( *out ));

	out->poseToBone[0][0] = m[0][0];
	out->poseToBone[1][0] = m[0][1];
	out->poseToBone[2][0] = m[0][2];

	out->poseToBone[0][1] = m[1][0];
	out->poseToBone[1][1] = m[1][1];
	out->poseToBone[2][1] = m[1][2];

	out->poseToBone[0][2] = m[2][0];
	out->poseToBone[1][2] = m[2][1];
	out->poseToBone[2][2] = m[2][2];

	out->poseToBone[0][3] = m[3][0];
	out->poseToBone[1][3] = m[3][1];
	out->poseToBone[2][3] = m[3][2];

	out->qAlignment = g_bonetable[bone].qAlignment;

	AngleQuaternion( g_bonetable[bone].rot, out->quat );
	QuaternionAlign( out->qAlignment, out->quat, out->quat );
}

void WriteKeyValues( studiohdr2_t *phdr, CUtlArray< char > *pKeyValue )
{
	phdr->keyvaluesize = KeyValueTextSize( pKeyValue );

	if( phdr->keyvaluesize )
	{
		phdr->keyvalueindex = (pData - pStart);
		memcpy( pData, KeyValueText( pKeyValue ), phdr->keyvaluesize );

		// Add space for a null terminator
		pData[phdr->keyvaluesize] = 0;
		++phdr->keyvaluesize;
		pData += phdr->keyvaluesize * sizeof( char );
	}
	ALIGN( pData );
}

void WriteSeqKeyValues( mstudioseqdesc_t *pseqdesc, CUtlArray< char > *pKeyValue )
{
	// FIXME: no variables in mstudioseqdesc_t to store keyvaluesize without break binary compatibility :-(
	int keyvaluesize = KeyValueTextSize( pKeyValue );

	if( keyvaluesize )
	{
		pseqdesc->keyvalueindex = (pData - pStart);
		memcpy( pData, KeyValueText( pKeyValue ), keyvaluesize );

		// add space for a null terminator
		pData[keyvaluesize] = 0;
		keyvaluesize++;
		pData += keyvaluesize * sizeof( char );
	}

	ALIGN( pData );
}

void CopyStringTruncate( char *out, const char *in, size_t outsize )
{
	size_t insize = Q_strlen( in );
	size_t truncsize = Q_strlen( TRUNCNAME );
	const char *trunc;
	size_t start;

	*out = '\0';

	if(( trunc = Q_strstr( in, TRUNCNAME )) != NULL )
	{
		start = trunc - in;
		Q_strncpy( out, in, start );	// may be 0
		Q_strncat( out, trunc + truncsize, outsize );
		MsgDev( D_REPORT, "%s renamed to %s\n", in, out );
		return; // best case to truncate
	}
	else if( insize > ( outsize - 1 ))
	{
		MsgDev( D_WARN, "%s is too long (limit is %u symbols). string will be truncated\n", in, outsize );
	}

	Q_strncpy( out, in, outsize ); // this may cause problems...
}

void WriteBoneInfo( void )
{
	int i, j, k;
	mstudiobonecontroller_t	*pbonecontroller;
	mstudioattachment_t		*pattachment;
	mstudioboneinfo_t		*pboneinfo;
	mstudiobone_t		*pbone;
	mstudiobbox_t		*pbbox;

	// save bone info
	pbone = (mstudiobone_t *)pData;
	phdr->numbones = g_numbones;
	phdr->boneindex = (pData - pStart);

	for( i = 0; i < g_numbones; i++ ) 
	{
		CopyStringTruncate( pbone[i].name, g_bonetable[i].name, sizeof( pbone[0].name ));
		pbone[i].parent	= g_bonetable[i].parent;
		pbone[i].flags	= g_bonetable[i].flags;
		pbone[i].value[0]	= g_bonetable[i].pos[0];
		pbone[i].value[1]	= g_bonetable[i].pos[1];
		pbone[i].value[2]	= g_bonetable[i].pos[2];
		pbone[i].value[3]	= g_bonetable[i].rot[0];
		pbone[i].value[4]	= g_bonetable[i].rot[1];
		pbone[i].value[5]	= g_bonetable[i].rot[2];
		pbone[i].scale[0]	= g_bonetable[i].posscale[0];
		pbone[i].scale[1]	= g_bonetable[i].posscale[1];
		pbone[i].scale[2]	= g_bonetable[i].posscale[2];
		pbone[i].scale[3]	= g_bonetable[i].rotscale[0];
		pbone[i].scale[4]	= g_bonetable[i].rotscale[1];
		pbone[i].scale[5]	= g_bonetable[i].rotscale[2];

		SetupBoneInfo( i, &g_boneinfo[i] );
	}
	pData += g_numbones * sizeof( mstudiobone_t );
	ALIGN( pData );

	if( FBitSet( gflags, STUDIO_HAS_BONEINFO ))
	{
		pboneinfo = (mstudioboneinfo_t *)pData;
		memcpy( pboneinfo, g_boneinfo, g_numbones * sizeof( mstudioboneinfo_t ));
		pData += g_numbones * sizeof( mstudioboneinfo_t );
		ALIGN( pData );

		// save procedural bone info
		if( g_numaxisinterpbones )
		{
			mstudioaxisinterpbone_t *pProc = (mstudioaxisinterpbone_t *)pData;
			pData += g_numaxisinterpbones * sizeof( mstudioaxisinterpbone_t );
			ALIGN( pData );

			for( i = 0; i < g_numaxisinterpbones; i++ )
			{
				j = g_axisinterpbonemap[i];
				k = g_axisinterpbones[j].bone;
				pboneinfo[k].procindex = (byte *)&pProc[i] - pStart;
				pboneinfo[k].proctype = STUDIO_PROC_AXISINTERP;
				// Msg( "bone %d %d\n", j, pbone[k].procindex );
				pProc[i].control = g_axisinterpbones[j].control;
				pProc[i].axis = g_axisinterpbones[j].axis;
				for( k = 0; k < 6; k++ )
				{
					pProc[i].pos[k] = g_axisinterpbones[j].pos[k];
					pProc[i].quat[k] = g_axisinterpbones[j].quat[k];
				}
			}
		}

		if( g_numquatinterpbones )
		{
			mstudioquatinterpbone_t *pProc = (mstudioquatinterpbone_t *)pData;
			pData += g_numquatinterpbones * sizeof( mstudioquatinterpbone_t );
			ALIGN( pData );

			for( i = 0; i < g_numquatinterpbones; i++ )
			{
				j = g_quatinterpbonemap[i];
				k = g_quatinterpbones[j].bone;
				pboneinfo[k].procindex = (byte *)&pProc[i] - pStart;
				pboneinfo[k].proctype = STUDIO_PROC_QUATINTERP;
				// Msg( "bone %d %d\n", j, pbone[k].procindex );
				pProc[i].control = g_quatinterpbones[j].control;

				mstudioquatinterpinfo_t *pTrigger = (mstudioquatinterpinfo_t *)pData;
				pProc[i].numtriggers = g_quatinterpbones[j].numtriggers;
				pProc[i].triggerindex = (byte *)pTrigger - pStart;
				pData += pProc[i].numtriggers * sizeof( mstudioquatinterpinfo_t );

				for( k = 0; k < pProc[i].numtriggers; k++ )
				{
					pTrigger[k].inv_tolerance = 1.0 / g_quatinterpbones[j].tolerance[k];
					pTrigger[k].trigger	= g_quatinterpbones[j].trigger[k];
					pTrigger[k].pos = g_quatinterpbones[j].pos[k];
					pTrigger[k].quat = g_quatinterpbones[j].quat[k];
				}
			}
		}

		if( g_numjigglebones )
		{
			mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)pData;
			pData += g_numjigglebones * sizeof( mstudiojigglebone_t );
			ALIGN( pData );		

			for( i = 0; i < g_numjigglebones; i++ )
			{
				j = g_jigglebonemap[i];
				k = g_jigglebones[j].bone;
				pboneinfo[k].procindex = (byte *)&jiggleInfo[i] - pStart;
				pboneinfo[k].proctype = STUDIO_PROC_JIGGLE;
			
				jiggleInfo[i] = g_jigglebones[j].data;
			}

		}

		// write aim at bones
		if( g_numaimatbones )
		{
			mstudioaimatbone_t *pProc = (mstudioaimatbone_t *)pData;
			pData += g_numaimatbones * sizeof( mstudioaimatbone_t );
			ALIGN( pData );

			for( i = 0; i < g_numaimatbones; i++ )
			{
				j = g_aimatbonemap[i];
				k = g_aimatbones[j].bone;
				pboneinfo[k].procindex = (byte *)&pProc[i] - pStart;
				pboneinfo[k].proctype = g_aimatbones[j].aimAttach == -1 ? STUDIO_PROC_AIMATBONE : STUDIO_PROC_AIMATATTACH;
				pProc[i].parent = g_aimatbones[j].parent;
				pProc[i].aim = g_aimatbones[j].aimAttach == -1 ? g_aimatbones[j].aimBone : g_aimatbones[j].aimAttach;
				pProc[i].aimvector = g_aimatbones[j].aimvector;
				pProc[i].upvector = g_aimatbones[j].upvector;
				pProc[i].basepos = g_aimatbones[j].basepos;
			}
		}
	}

	// map bonecontroller to bones
	for( i = 0; i < g_numbones; i++ )
	{
		for( j = 0; j < 6; j++ )
			pbone[i].bonecontroller[j] = -1;
	}
	
	for( i = 0; i < g_numbonecontrollers; i++ )
	{
		j = g_bonecontroller[i].bone;

		switch( g_bonecontroller[i].type & STUDIO_TYPES )
		{
		case STUDIO_X:
			pbone[j].bonecontroller[0] = i;
			break;
		case STUDIO_Y:
			pbone[j].bonecontroller[1] = i;
			break;
		case STUDIO_Z:
			pbone[j].bonecontroller[2] = i;
			break;
		case STUDIO_XR:
			pbone[j].bonecontroller[3] = i;
			break;
		case STUDIO_YR:
			pbone[j].bonecontroller[4] = i;
			break;
		case STUDIO_ZR:
			pbone[j].bonecontroller[5] = i;
			break;
		default:
			COM_FatalError( "unknown bonecontroller type %i\n", g_bonecontroller[i].type & STUDIO_TYPES );
		}

		if( g_mergebonecontrollers )
		{
			for( j = 0; j < i; j++ )
			{
				if( g_bonecontroller[i].start == g_bonecontroller[j].start
				&& g_bonecontroller[i].end == g_bonecontroller[j].end
				&& g_bonecontroller[i].index == g_bonecontroller[j].index
				&& (( g_bonecontroller[i].type & (STUDIO_X | STUDIO_Y | STUDIO_Z )) != 0 ) == (( g_bonecontroller[j].type & ( STUDIO_X|STUDIO_Y|STUDIO_Z )) != 0 ))
				{
					switch( g_bonecontroller[i].type & STUDIO_TYPES )
					{
					case STUDIO_X:
						pbone[g_bonecontroller[i].bone].bonecontroller[0] = j;
						break;
					case STUDIO_Y:
						pbone[g_bonecontroller[i].bone].bonecontroller[1] = j;
						break;
					case STUDIO_Z:
						pbone[g_bonecontroller[i].bone].bonecontroller[2] = j;
						break;
					case STUDIO_XR:
						pbone[g_bonecontroller[i].bone].bonecontroller[3] = j;
						break;
					case STUDIO_YR:
						pbone[g_bonecontroller[i].bone].bonecontroller[4] = j;
						break;
					case STUDIO_ZR:
						pbone[g_bonecontroller[i].bone].bonecontroller[5] = j;
						break;
					}

					memcpy( &g_bonecontroller[i], &g_bonecontroller[i + 1], ( g_numbonecontrollers - ( i + 1 )) * sizeof( g_bonecontroller[0] ));
					g_numbonecontrollers--;
					i--;
					break;
				}
			}
		}
	}

	// save bonecontroller info
	pbonecontroller = (mstudiobonecontroller_t *)pData;
	phdr->numbonecontrollers = g_numbonecontrollers;
	phdr->bonecontrollerindex = (pData - pStart);

	for( i = 0; i < g_numbonecontrollers; i++ )
	{
		pbonecontroller[i].bone	= g_bonecontroller[i].bone;
		pbonecontroller[i].index	= g_bonecontroller[i].index;
		pbonecontroller[i].type	= g_bonecontroller[i].type;
		pbonecontroller[i].start	= g_bonecontroller[i].start;
		pbonecontroller[i].end	= g_bonecontroller[i].end;
	}

	pData += g_numbonecontrollers * sizeof( mstudiobonecontroller_t );
	ALIGN( pData );

	// save attachment info
	pattachment = (mstudioattachment_t *)pData;
	phdr->numattachments = g_numattachments;
	phdr->attachmentindex = (pData - pStart);

	for( i = 0; i < g_numattachments; i++ )
	{
		CopyStringTruncate( pattachment[i].name, g_attachment[i].name, sizeof( pattachment[0].name ));
		pattachment[i].bone = g_attachment[i].bone;
		pattachment[i].org = g_attachment[i].local.GetOrigin();
		pattachment[i].flags = g_attachment[i].flags;

		// evil FP jitter
		for( j = 0; j < 3; j++ )
		{
			if( fabs( pattachment[i].org[j] ) < 0.00001f )
				pattachment[i].org[j] = 0.0f;
		}

		// save rotation matrix for local attachments
		if( FBitSet( pattachment[i].flags, STUDIO_ATTACHMENT_LOCAL ))
		{
			pattachment[i].vectors[0] = g_attachment[i].local.GetForward();
			pattachment[i].vectors[1] = g_attachment[i].local.GetRight();
			pattachment[i].vectors[2] = g_attachment[i].local.GetUp();
		}
	}

	pData += g_numattachments * sizeof( mstudioattachment_t );
	ALIGN( pData );

	// save bbox info
	s_hitboxset_t *set = &g_hitboxsets[0];	// just use 0 group
	pbbox = (mstudiobbox_t *)pData;
	phdr->numhitboxes = set->numhitboxes;
	phdr->hitboxindex = (pData - pStart);

	for( i = 0; i < set->numhitboxes; i++ )
	{
		pbbox[i].bone = set->hitbox[i].bone;
		pbbox[i].group = set->hitbox[i].group;
		pbbox[i].bbmin = set->hitbox[i].bmin;
		pbbox[i].bbmax = set->hitbox[i].bmax;
	}
	pData += set->numhitboxes * sizeof( mstudiobbox_t );
	ALIGN( pData );

	// save hitbox sets
	phdr2->numhitboxsets = g_hitboxsets.Size();

	// remember start spot
	mstudiohitboxset_t *hitboxset = (mstudiohitboxset_t *)pData;
	phdr2->hitboxsetindex = pData - pStart;
	pData += phdr2->numhitboxsets * sizeof( mstudiohitboxset_t );
	ALIGN( pData );

	for ( int s = 0; s < g_hitboxsets.Size(); s++, hitboxset++ )
	{
		set = &g_hitboxsets[s];

		CopyStringTruncate( hitboxset->name, set->hitboxsetname, sizeof( hitboxset->name ));
		hitboxset->numhitboxes = set->numhitboxes;
		hitboxset->hitboxindex = ( pData - pStart );

		// save bbox info
		pbbox = (mstudiobbox_t *)pData;
		for( i = 0; i < hitboxset->numhitboxes; i++ ) 
		{
			pbbox[i].bone = set->hitbox[i].bone;
			pbbox[i].group = set->hitbox[i].group;
			pbbox[i].bbmin = set->hitbox[i].bmin;
			pbbox[i].bbmax = set->hitbox[i].bmax;
		}
		pData += hitboxset->numhitboxes * sizeof( mstudiobbox_t );
		ALIGN( pData );
	}
}

void WriteSequenceInfo( void )
{
	int i, j, k;

	mstudioseqgroup_t	*pseqgroup;
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;
	byte		*ptransition;

	// save sequence info
	pseqdesc = (mstudioseqdesc_t *)pData;
	phdr->numseq = g_numseq;
	phdr->seqindex = (pData - pStart);
	pData += g_numseq * sizeof( mstudioseqdesc_t );

	for( i = 0; i < g_numseq; i++, pseqdesc++ ) 
	{
		CopyStringTruncate( pseqdesc->label, g_sequence[i].name, sizeof( pseqdesc->label ));
		pseqdesc->numframes		= g_sequence[i].numframes;
		pseqdesc->fps		= g_sequence[i].fps;
		pseqdesc->flags		= g_sequence[i].flags;

		pseqdesc->numblends		= g_sequence[i].numblends;

		pseqdesc->groupsize[0]	= g_sequence[i].groupsize[0];
		pseqdesc->groupsize[1]	= g_sequence[i].groupsize[1];
		pseqdesc->blendtype[0]	= g_sequence[i].paramindex[0];
		pseqdesc->blendtype[1]	= g_sequence[i].paramindex[1];
		pseqdesc->blendstart[0]	= g_sequence[i].paramstart[0];
		pseqdesc->blendend[0]	= g_sequence[i].paramend[0];
		pseqdesc->blendstart[1]	= g_sequence[i].paramstart[1];
		pseqdesc->blendend[1]	= g_sequence[i].paramend[1];

		pseqdesc->motiontype	= g_sequence[i].motiontype;
		pseqdesc->linearmovement	= g_sequence[i].linearmovement;

		pseqdesc->seqgroup		= g_sequence[i].seqgroup;

		pseqdesc->animindex		= g_sequence[i].animindex;		// anim data[numblends]
		pseqdesc->animdescindex	= g_sequence[i].animdescindex;	// anim desc[numblends]

		if(( g_sequence[i].groupsize[0] > 1 || g_sequence[i].groupsize[1] > 1 ))
		{
			// save posekey values
			float *pposekey = (float *)pData;
			pseqdesc->posekeyindex = (pData - pStart);
			pData += (pseqdesc->groupsize[0] + pseqdesc->groupsize[1]) * sizeof( float );

			for( j = 0; j < pseqdesc->groupsize[0]; j++ )
				*(pposekey++) = g_sequence[i].param0[j];

			for( j = 0; j < pseqdesc->groupsize[1]; j++ )
				*(pposekey++) = g_sequence[i].param1[j];
		}

		pseqdesc->activity		= g_sequence[i].activity;
		pseqdesc->actweight		= g_sequence[i].actweight;

		pseqdesc->bbmin		= g_sequence[i].bmin;
		pseqdesc->bbmax		= g_sequence[i].bmax;

		pseqdesc->fadeintime	= bound( 0, g_sequence[i].fadeintime * 100, 255 );
		pseqdesc->fadeouttime	= bound( 0, g_sequence[i].fadeouttime * 100, 255 );

		pseqdesc->entrynode		= g_sequence[i].entrynode;
		pseqdesc->exitnode		= g_sequence[i].exitnode;
		pseqdesc->nodeflags		= g_sequence[i].nodeflags;
		pseqdesc->cycleposeindex	= g_sequence[i].cycleposeindex;

//		totalframes		+= g_sequence[i].numframes;
//		totalseconds		+= g_sequence[i].numframes / g_sequence[i].fps;

		// save events
		pevent			= (mstudioevent_t *)pData;
		pseqdesc->numevents		= g_sequence[i].numevents;
		pseqdesc->eventindex	= (pData - pStart);
		pData += pseqdesc->numevents * sizeof( mstudioevent_t );
		ALIGN( pData );

		for( j = 0; j < g_sequence[i].numevents; j++ )
		{
			pevent[j].frame	= g_sequence[i].event[j].frame - g_sequence[i].frameoffset;
			pevent[j].event	= g_sequence[i].event[j].event;
			memcpy( pevent[j].options, g_sequence[i].event[j].options, sizeof( pevent[j].options ) );
		}

		// save ikrules
		if( g_sequence[i].numikrules )
			SetBits( pseqdesc->flags, STUDIO_IKRULES ); // just a bit to tell what animations contain ik rules

		// save autolayers
		mstudioautolayer_t *pautolayer = (mstudioautolayer_t *)pData;
		pseqdesc->numautolayers = g_sequence[i].numautolayers;
		pseqdesc->autolayerindex = (pData - pStart);
		pData += pseqdesc->numautolayers * sizeof( mstudioautolayer_t );

		for( j = 0; j < g_sequence[i].numautolayers; j++ )
		{
			pautolayer[j].iSequence = g_sequence[i].autolayer[j].sequence;
			pautolayer[j].iPose		= g_sequence[i].autolayer[j].pose;
			pautolayer[j].flags		= g_sequence[i].autolayer[j].flags;

			if( !FBitSet( pautolayer[j].flags, STUDIO_AL_POSE ))
			{
				pautolayer[j].start	= g_sequence[i].autolayer[j].start / (g_sequence[i].numframes - 1);
				pautolayer[j].peak	= g_sequence[i].autolayer[j].peak / (g_sequence[i].numframes - 1);
				pautolayer[j].tail	= g_sequence[i].autolayer[j].tail / (g_sequence[i].numframes - 1);
				pautolayer[j].end	= g_sequence[i].autolayer[j].end / (g_sequence[i].numframes - 1);
			}
			else
			{
				pautolayer[j].start	= g_sequence[i].autolayer[j].start;
				pautolayer[j].peak	= g_sequence[i].autolayer[j].peak;
				pautolayer[j].tail	= g_sequence[i].autolayer[j].tail;
				pautolayer[j].end	= g_sequence[i].autolayer[j].end;
			}
		}


		// save boneweights
		if (g_numweightlist > 0) 
		{
			int oldweightlistindex = 0;
			j = 0;

			// look up previous sequence weights and try to find a match
			for (k = 0; k < i; k++)
			{
				j = 0;
				// only check newer boneweights than the last one
				if (pseqdesc[k - i].weightlistindex > oldweightlistindex)
				{
					oldweightlistindex = pseqdesc[k - i].weightlistindex;
					for (j = 0; j < g_numbones; j++)
					{
						if (g_sequence[i].weight[j] != g_sequence[k].weight[j])
							break;
					}
					if (j == g_numbones)
						break;
				}
			}

			// check to see if all the bones matched
			if (j < g_numbones)
			{
				// allocate new block
				float *pweight = (float *)pData;
				pseqdesc->weightlistindex = (pData - pStart);
				pData += g_numbones * sizeof(float);

				for (j = 0; j < g_numbones; j++)
					pweight[j] = g_sequence[i].weight[j];
			}
			else
			{
				// use previous boneweight
				pseqdesc->weightlistindex = oldweightlistindex;
			}
		}
		else {
			pseqdesc->weightlistindex = 0; // or may be we should use -1?
		}

		// save iklocks
		mstudioiklock_t *piklock	= (mstudioiklock_t *)pData;
		pseqdesc->numiklocks	= g_sequence[i].numiklocks;
		pseqdesc->iklockindex	= (pData - pStart);
		pData += pseqdesc->numiklocks * sizeof( mstudioiklock_t );
		ALIGN( pData );

		for( j = 0; j < pseqdesc->numiklocks; j++ )
		{
			piklock->chain		= g_sequence[i].iklock[j].chain;
			piklock->flPosWeight	= g_sequence[i].iklock[j].flPosWeight;
			piklock->flLocalQWeight	= g_sequence[i].iklock[j].flLocalQWeight;
			piklock++;
		}

		WriteSeqKeyValues( pseqdesc, &g_sequence[i].KeyValue );
	}

	// save sequence group info
	pseqgroup = (mstudioseqgroup_t *)pData;
	phdr->numseqgroups = g_numseqgroups;
	phdr->seqgroupindex = (pData - pStart);
	pData += phdr->numseqgroups * sizeof( mstudioseqgroup_t );
	ALIGN( pData );

	for( i = 0; i < g_numseqgroups; i++ ) 
	{
		CopyStringTruncate( pseqgroup[i].label, g_sequencegroup[i].label, sizeof( pseqgroup[0].label ));
		CopyStringTruncate( pseqgroup[i].name, g_sequencegroup[i].name, sizeof( pseqgroup[0].name ));
	}

	// save transition graph
	ptransition = (byte *)pData;
	phdr->numtransitions = g_numxnodes;
	phdr->transitionindex = (pData - pStart);
	pData += g_numxnodes * g_numxnodes * sizeof( byte );
	ALIGN( pData );

	for( i = 0; i < g_numxnodes; i++ )
	{
		for( j = 0; j < g_numxnodes; j++ )
		{
			*ptransition++ = g_xnode[i][j];
		}
	}
}

byte *WriteIkErrors( s_animation_t *srcanim, byte *pData )
{
	int j, k, n;

	// NOTE: GoldSrc models saves animations per-sequence and duplicate them
	// but we can to avoid wrote identical data block again.
	if( !srcanim->numikrules || srcanim->ikruleindex )
		return pData; // already stored?

	srcanim->ikruleindex = pData - pStart;

	// write IK error keys
	mstudioikrule_t *pikruledata = (mstudioikrule_t *)pData;
	pData += srcanim->numikrules * sizeof( mstudioikrule_t );
	ALIGN( pData );

	for( j = 0; j < srcanim->numikrules; j++ )
	{
		mstudioikrule_t *pikrule = pikruledata + j;

		pikrule->index	= srcanim->ikrule[j].index;
		pikrule->chain	= srcanim->ikrule[j].chain;
		pikrule->bone	= srcanim->ikrule[j].bone;
		pikrule->type	= srcanim->ikrule[j].type;
		pikrule->slot	= srcanim->ikrule[j].slot;
		pikrule->pos	= srcanim->ikrule[j].pos;
		pikrule->quat	= srcanim->ikrule[j].q;
		pikrule->height	= srcanim->ikrule[j].height;
		pikrule->floor	= srcanim->ikrule[j].floor;
		pikrule->radius	= srcanim->ikrule[j].radius;
		pikrule->attachment = LookupAttachment( srcanim->ikrule[j].attachment );

		if( srcanim->numframes > 1.0 )
		{
			pikrule->start	= srcanim->ikrule[j].start / (srcanim->numframes - 1.0f);
			pikrule->peak	= srcanim->ikrule[j].peak / (srcanim->numframes - 1.0f);
			pikrule->tail	= srcanim->ikrule[j].tail / (srcanim->numframes - 1.0f);
			pikrule->end	= srcanim->ikrule[j].end / (srcanim->numframes - 1.0f);
			pikrule->contact	= srcanim->ikrule[j].contact / (srcanim->numframes - 1.0f);
		}
		else
		{
			pikrule->start	= 0.0f;
			pikrule->peak	= 0.0f;
			pikrule->tail	= 1.0f;
			pikrule->end	= 1.0f;
			pikrule->contact	= 0.0f;
		}

		pikrule->iStart = srcanim->ikrule[j].start;

		// skip writting the header if there's no IK data
		for( k = 0; k < 6; k++ )
		{
			if( srcanim->ikrule[j].errorData.numanim[k] )
				break;
		}

		if( k == 6 ) continue;

		// compressed
		pikrule->ikerrorindex = (pData - pStart);
		mstudioikerror_t *pCompressed = (mstudioikerror_t *)pData;
		pData += sizeof( *pCompressed );
		mstudioanimvalue_t *panimvalue = (mstudioanimvalue_t *)pData;

		for( k = 0; k < 6; k++ )
		{
			pCompressed->scale[k] = srcanim->ikrule[j].errorData.scale[k];

			if( srcanim->ikrule[j].errorData.numanim[k] == 0 )
			{
				pCompressed->offset[k] = 0;
			}
			else
			{
				pCompressed->offset[k] = ((byte *)panimvalue - (byte *)pCompressed);
				for( n = 0; n < srcanim->ikrule[j].errorData.numanim[k]; n++ )
				{
					panimvalue->value = srcanim->ikrule[j].errorData.anim[k][n].value;
					panimvalue++;
				}
			}
		}

		pData = (byte *)panimvalue;
		ALIGN( pData );
	}

	return pData;
}

byte *WriteMovements( s_animation_t *srcanim, byte *pData )
{
	mstudiomovement_t *pmovement = (mstudiomovement_t *)pData;

	// NOTE: GoldSrc models saves animations per-sequence and duplicate them
	// but we can to avoid wrote identical data block again.
	if( srcanim->numpiecewisekeys && !srcanim->movementindex )
	{
		srcanim->movementindex = (pData - pStart);
		for( int j = 0; j < srcanim->numpiecewisekeys; j++ )
		{
			pmovement[j].endframe = srcanim->piecewisemove[j].endframe;
			pmovement[j].motionflags = srcanim->piecewisemove[j].flags;
			pmovement[j].v0 = srcanim->piecewisemove[j].v0;
			pmovement[j].v1 = srcanim->piecewisemove[j].v1;
			pmovement[j].angle = RAD2DEG( srcanim->piecewisemove[j].rot[2] );
			pmovement[j].vector = srcanim->piecewisemove[j].vector;
			pmovement[j].position = srcanim->piecewisemove[j].pos;
		}

		pData += srcanim->numpiecewisekeys * sizeof( mstudiomovement_t );
		ALIGN( pData );
	}

	return pData;
}

byte *WriteAnimations( byte *pData, byte *pStart, int group )
{
	int i, j, k;
	int q, n;

	mstudioanim_t *panim;
	mstudioanimvalue_t *panimvalue;

	// hack for seqgroup 0
	// pseqgroup->data = (pData - pStart);

	for( i = 0; i < g_numseq; i++ ) 
	{
		if( g_sequence[i].seqgroup == group )
		{
			// save animations
			panim = (mstudioanim_t *)pData;
			g_sequence[i].animindex = (pData - pStart);
			pData += g_sequence[i].numblends * g_numbones * sizeof( mstudioanim_t );
			ALIGN( pData );
			
			panimvalue = (mstudioanimvalue_t *)pData;

			for( q = 0; q < g_sequence[i].numblends; q++ )
			{
				// save animation value info
				for( j = 0; j < g_numbones; j++ )
				{
					for( k = 0; k < 6; k++ )
					{
						if( g_sequence[i].panim[q]->numanim[j][k] == 0 )
						{
							panim->offset[k] = 0;
						}
						else
						{
							panim->offset[k] = ((byte *)panimvalue - (byte *)panim);
							for( n = 0; n < g_sequence[i].panim[q]->numanim[j][k]; n++ )
							{
								panimvalue->value = g_sequence[i].panim[q]->anim[j][k][n].value;
								panimvalue++;
							}
						}
					}

					// a main limitation of GoldSrc studio format ;-(
					if(((byte *)panimvalue - (byte *)panim) > 65535 )
						COM_FatalError( "sequence \"%s\" is greater than 64K ( %s )\n",
						g_sequence[i].name, Q_memprint( ((byte *)panimvalue - (byte *)panim)));
					panim++;
				}
			}

			pData = (byte *)panimvalue;
			ALIGN( pData );
		}
	}

	return pData;
}

byte *WriteAnimDescriptions( byte *pData, byte *pStart, int group )
{
	int i, j, q;

	mstudioanimdesc_t *panimdesc;

	// hack for seqgroup 0
	// pseqgroup->data = (pData - pStart);

	for( i = 0; i < g_numseq; i++ ) 
	{
		if( g_sequence[i].seqgroup == group )
		{
			// write piecewise movement
			for( q = 0; q < g_sequence[i].numblends; q++ )
			{
				s_animation_t *srcanim = g_sequence[i].panim[q];
				pData = WriteMovements( srcanim, pData );
			}

			// write IK errors
			for( q = 0; q < g_sequence[i].numblends; q++ )
			{
				s_animation_t *srcanim = g_sequence[i].panim[q];
				pData = WriteIkErrors( srcanim, pData );
			}

			// write animation descriptions
			panimdesc = (mstudioanimdesc_t *)pData;
			g_sequence[i].animdescindex = (pData - pStart);
			pData += g_sequence[i].numblends * sizeof( mstudioanimdesc_t );
			ALIGN( pData );

			// write anim descriptions
			for( q = 0; q < g_sequence[i].numblends; q++ )
			{
				s_animation_t *srcanim = g_sequence[i].panim[q];
				mstudioanimdesc_t *destanim = &panimdesc[q];

				CopyStringTruncate( destanim->label, srcanim->name, sizeof( destanim->label ));
				destanim->fps	= srcanim->fps;
				destanim->flags	= srcanim->flags;

				totalframes	+= srcanim->numframes;
				totalseconds	+= srcanim->numframes / srcanim->fps;

				destanim->numframes	= srcanim->numframes;
				destanim->nummovements = srcanim->numpiecewisekeys;
				destanim->movementindex = srcanim->movementindex;
				destanim->numikrules = srcanim->numikrules;
				destanim->ikruleindex = srcanim->ikruleindex;

				j = srcanim->numpiecewisekeys - 1;
				if( srcanim->piecewisemove[j].pos[0] != 0 || srcanim->piecewisemove[j].pos[1] != 0 ) 
				{
					float t = (srcanim->numframes - 1) / srcanim->fps;
					float r = 1.0f / t;
					float a = RAD2DEG( atan2( srcanim->piecewisemove[j].pos[1], srcanim->piecewisemove[j].pos[0] ));
					float d = sqrt( DotProduct( srcanim->piecewisemove[j].pos, srcanim->piecewisemove[j].pos ) );
					MsgDev( D_REPORT, "%12s %7.2f %7.2f : %7.2f (%7.2f) %.1f\n",
					srcanim->name, srcanim->piecewisemove[j].pos[0], srcanim->piecewisemove[j].pos[1], d * r, a, t );
				}
			}
		}
	}

	return pData;
}
	
void WriteTextures( void )
{
	int i, j;
	mstudiotexture_t *ptexture;
	short	*pref;

	// save bone info
	ptexture = (mstudiotexture_t *)pData;
	phdr->numtextures = g_numtextures;
	phdr->textureindex = (pData - pStart);
	pData += g_numtextures * sizeof( mstudiotexture_t );
	ALIGN( pData );

	phdr->skinindex = (pData - pStart);
	phdr->numskinref = g_numskinref;
	phdr->numskinfamilies = g_numskinfamilies;
	pref = (short *)pData;

	for( i = 0; i < phdr->numskinfamilies; i++ ) 
	{
		for( j = 0; j < phdr->numskinref; j++ ) 
		{
			*pref = g_skinref[i][j];
			pref++;
		}
	}
	pData = (byte *)pref;
	ALIGN( pData );

	phdr->texturedataindex = (pData - pStart); 	// must be the end of the file!

	for( i = 0; i < g_numtextures; i++ )
	{
		CopyStringTruncate( ptexture[i].name, g_texture[i].name, sizeof( ptexture[0].name ));
		ptexture[i].flags		= g_texture[i].flags;
		ptexture[i].width		= g_texture[i].skinwidth;
		ptexture[i].height		= g_texture[i].skinheight;
		ptexture[i].index		= (pData - pStart);
		memcpy( pData, g_texture[i].pdata, g_texture[i].size );
		pData += g_texture[i].size;
	}

	ALIGN( pData );
}

void WriteModel( void )
{
	int i, j, k;

	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t	*pmodel;
	// Vector		*bbox;
	byte		*pbone;
	Vector		*pvert;
	Vector		*pnorm;
	mstudioboneweight_t	*pweight;
	mstudiomesh_t	*pmesh;
	s_trianglevert_t	*psrctri;
	ptrdiff_t	cur;
	int		total_tris = 0;
	int		total_strips = 0;

	pbodypart = (mstudiobodyparts_t *)pData;
	phdr->numbodyparts = g_bodypart.Count();
	phdr->bodypartindex = (pData - pStart);
	pData += g_bodypart.Count() * sizeof( mstudiobodyparts_t );

	pmodel = (mstudiomodel_t *)pData;
	pData += g_nummodels * sizeof( mstudiomodel_t );

	for (i = 0, j = 0; i < g_bodypart.Count(); i++)
	{
		CopyStringTruncate( pbodypart[i].name, g_bodypart[i].name, sizeof( pbodypart[0].name ));
		pbodypart[i].nummodels = g_bodypart[i].nummodels;
		pbodypart[i].base = g_bodypart[i].base;
		pbodypart[i].modelindex = ((byte *)&pmodel[j]) - pStart;
		j += g_bodypart[i].nummodels;
	}
	ALIGN( pData );

	cur = (ptrdiff_t)pData;
	for( i = 0; i < g_nummodels; i++ ) 
	{
		static CUtlArray<int> normmap;
		static CUtlArray<int> normimap;
		int n = 0;

		CopyStringTruncate( pmodel[i].name, g_model[i]->name, sizeof( pmodel[0].name ));
		normmap.SetSize(g_model[i]->norm.Count());
		normimap.SetSize(g_model[i]->norm.Count());

		// remap normals to be sorted by skin reference
		for (j = 0; j < g_model[i]->nummesh; j++)
		{
			for (k = 0; k < g_model[i]->norm.Count(); k++)
			{
				if (g_model[i]->norm[k].skinref == g_model[i]->pmesh[j]->skinref)
				{
					normmap[k] = n;
					normimap[n] = k;
					n++;
					g_model[i]->pmesh[j]->numnorms++;
				}
			}
		}
		
		// save vertice bones
		pbone = pData;
		pmodel[i].numverts	= g_model[i]->vert.Count();
		pmodel[i].vertinfoindex = (pData - pStart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = g_model[i]->vert[j].globalWeight.bone[0];
		}
		ALIGN( pbone );

		// save normal bones
		pmodel[i].numnorms	= g_model[i]->norm.Count();
		pmodel[i].norminfoindex = ((byte *)pbone - pStart);
		for (j = 0; j < pmodel[i].numnorms; j++)
		{
			*pbone++ = g_model[i]->norm[normimap[j]].globalWeight.bone[0];
		}
		ALIGN( pbone );

		pData = pbone;

		if( FBitSet( gflags, STUDIO_HAS_BONEWEIGHTS ))
		{
			// save blended vertice bones
			pweight = (mstudioboneweight_t *)pData;

			pmodel[i].blendvertinfoindex = (pData - pStart);
			for (j = 0; j < pmodel[i].numverts; j++)
			{
				for( k = 0; k < MAXSTUDIOBONEWEIGHTS; k++ )
				{
					if( k < g_model[i]->vert[j].globalWeight.numbones )
					{
						pweight->bone[k] = g_model[i]->vert[j].globalWeight.bone[k];
						pweight->weight[k] = static_cast<byte>(
							round(g_model[i]->vert[j].globalWeight.weight[k] * 255.0)
						);
					}
					else
					{
						pweight->bone[k] = -1;
						pweight->weight[k] = 0;
					}
				}
				pweight++;
			}

			pData = (byte *)pweight;
			ALIGN( pData );

			// save blended normal bones
			pmodel[i].blendnorminfoindex = (pData - pStart);
			for (j = 0; j < pmodel[i].numnorms; j++)
			{
				for( k = 0; k < MAXSTUDIOBONEWEIGHTS; k++ )
				{
					if( k < g_model[i]->norm[normimap[j]].globalWeight.numbones )
					{
						pweight->bone[k] = g_model[i]->norm[normimap[j]].globalWeight.bone[k];
						pweight->weight[k] = static_cast<byte>(
							round(g_model[i]->norm[normimap[j]].globalWeight.weight[k] * 255.0)
						);
					}
					else
					{
						pweight->bone[k] = -1;
						pweight->weight[k] = 0;
					}
				}
				pweight++;
			}

			pData = (byte *)pweight;
			ALIGN( pData );
		}
		else
		{
			pmodel[i].blendvertinfoindex = 0;
			pmodel[i].blendnorminfoindex = 0;
		}

		// save group info
		pvert = (Vector *)pData;
		pData += g_model[i]->vert.Count() * sizeof( Vector );
		pmodel[i].vertindex		= ((byte *)pvert - pStart); 
		ALIGN( pData );			

		pnorm = (Vector *)pData;
		pData += g_model[i]->norm.Count() * sizeof( Vector );
		pmodel[i].normindex		= ((byte *)pnorm - pStart); 
		ALIGN( pData );

		for (j = 0; j < g_model[i]->vert.Count(); j++)
		{
			VectorCopy( g_model[i]->vert[j].org, pvert[j] );
		}

		for (j = 0; j < g_model[i]->norm.Count(); j++)
		{
			VectorCopy( g_model[i]->norm[normimap[j]].org, pnorm[j] );
		}
		Msg( "vertices   %7d bytes (%d vertices, %d normals)\n", pData - cur, g_model[i]->vert.Count(), g_model[i]->norm.Count() );
		cur = (ptrdiff_t)pData;

		// save mesh info
		pmesh = (mstudiomesh_t *)pData;
		pmodel[i].nummesh		= g_model[i]->nummesh;
		pmodel[i].meshindex		= (pData - pStart);
		pData += pmodel[i].nummesh * sizeof( mstudiomesh_t );
		ALIGN( pData );

		total_tris = 0;
		total_strips = 0;
		for (j = 0; j < g_model[i]->nummesh; j++)
		{
			int numCmdBytes;
			byte *pCmdSrc;

			pmesh[j].numtris	= g_model[i]->pmesh[j]->numtris;
			pmesh[j].skinref	= g_model[i]->pmesh[j]->skinref;
			pmesh[j].numnorms	= g_model[i]->pmesh[j]->numnorms;

			psrctri = (s_trianglevert_t *)(g_model[i]->pmesh[j]->triangle);
			for (k = 0; k < pmesh[j].numtris * 3; k++) 
			{
				psrctri->normindex	= normmap[psrctri->normindex];
				psrctri++;
			}

			numCmdBytes = BuildTris( g_model[i]->pmesh[j]->triangle, g_model[i]->pmesh[j], &pCmdSrc );

			pmesh[j].triindex	= (pData - pStart);
			memcpy( pData, pCmdSrc, numCmdBytes );
			pData += numCmdBytes;
			ALIGN( pData );
			total_tris += pmesh[j].numtris;
			total_strips += numcommandnodes;
		}
		Msg( "mesh       %7d bytes (%d tris, %d strips)\n", pData - cur, total_tris, total_strips );
		cur = (ptrdiff_t)pData;
	}	
}

void WriteFile( void )
{
	int	i, j, total = 0;
	char model_name[64];

	pStart = (byte *)Mem_Alloc( FILEBUFFER );

	COM_StripExtension( outname );

	if( has_boneweights )
		SetBits( gflags, STUDIO_HAS_BONEWEIGHTS|STUDIO_HAS_BONEINFO );

	// procedural bones are invoke to write extended boneinfo
	if( g_numaxisinterpbones || g_numquatinterpbones || g_numjigglebones || g_numaimatbones )
		SetBits( gflags, STUDIO_HAS_BONEINFO );

	for( i = 1; i < g_numseqgroups; i++ )
	{
		// write the non-default sequence group data to separate files
		char groupname[128], localname[128];

		Q_snprintf( groupname, sizeof( groupname ), "%s%02d.mdl", outname, i );

		Msg( "writing %s:\n", groupname );

		pseqhdr = (studioseqhdr_t *)pStart;
		pseqhdr->id = IDSEQGRPHEADER;
		pseqhdr->version = STUDIO_VERSION;

		pData = pStart + sizeof( studioseqhdr_t ); 

		pData = WriteAnimations( pData, pStart, i );
		pData = WriteAnimDescriptions( pData, pStart, i );

		COM_FileBase( groupname, localname );
		Q_snprintf( g_sequencegroup[i].name, sizeof( g_sequencegroup[0].name ), "models\\%s.mdl", localname );
		CopyStringTruncate( pseqhdr->name, g_sequencegroup[i].name, sizeof( pseqhdr->name ));
		pseqhdr->length = pData - pStart;

		Msg( "total     %6d\n", pseqhdr->length );

		COM_SaveFile( groupname, pStart, pseqhdr->length, true );
		memset( pStart, 0, pseqhdr->length );
	}

//
// write the model output file
//
	COM_DefaultExtension( outname, ".mdl" );
	
	Msg( "---------------------\n" );
	phdr = (studiohdr_t *)pStart;

	phdr->ident = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;
	COM_FileBase(outname, model_name); // g-cont. don't show real path on artist machine! palevo!
	Q_snprintf(phdr->name, sizeof(phdr->name), "%s (pxstudiomdl / %s / %s)", model_name, BuildInfo::GetCommitHash(), BuildInfo::GetArchitecture());
	VectorCopy(eyeposition, phdr->eyeposition);
	VectorCopy(bbox[0], phdr->min);
	VectorCopy(bbox[1], phdr->max);
	VectorCopy(cbox[0], phdr->bbmin);
	VectorCopy(cbox[1], phdr->bbmax);

	phdr->flags = gflags;

	pData = (byte *)phdr + sizeof( studiohdr_t );
	phdr2 = (studiohdr2_t *)pData;
	phdr->studiohdr2index = pData - pStart;
	pData += sizeof( studiohdr2_t );
 
	WriteBoneInfo( );
	Msg( "bones      %7d bytes (%d)\n", pData - pStart - total, g_numbones );
	total = pData - pStart;

	// write ik chains
	mstudioikchain_t *pikchain = (mstudioikchain_t *)pData;
	phdr2->numikchains = g_numikchains;
	phdr2->ikchainindex = pData - pStart;
	pData += g_numikchains * sizeof( mstudioikchain_t );
	ALIGN( pData );

	for( j = 0; j < g_numikchains; j++ )
	{
		CopyStringTruncate( pikchain->name, g_ikchain[j].name, sizeof( pikchain->name ));
		pikchain->numlinks = g_ikchain[j].numlinks;

		mstudioiklink_t *piklink = (mstudioiklink_t *)pData;
		pikchain->linkindex	= (pData - pStart);
		pData += pikchain->numlinks * sizeof( mstudioiklink_t );

		for( i = 0; i < pikchain->numlinks; i++ )
		{
			piklink[i].bone = g_ikchain[j].link[i].bone;
			piklink[i].kneeDir = g_ikchain[j].link[i].kneeDir;
		}
		pikchain++;
	}

	// save autoplay locks
	mstudioiklock_t *piklock = (mstudioiklock_t *)pData;
	phdr2->numikautoplaylocks = g_numikautoplaylocks;
	phdr2->ikautoplaylockindex = pData - pStart;
	pData += g_numikautoplaylocks * sizeof( mstudioiklock_t );
	ALIGN( pData );

	for( j = 0; j < g_numikautoplaylocks; j++ )
	{
		piklock->chain = g_ikautoplaylock[j].chain;
		piklock->flPosWeight = g_ikautoplaylock[j].flPosWeight;
		piklock->flLocalQWeight = g_ikautoplaylock[j].flLocalQWeight;
		piklock++;
	}

	// save pose parameters
	mstudioposeparamdesc_t *ppose = (mstudioposeparamdesc_t *)pData;
	phdr2->numposeparameters = g_numposeparameters;
	phdr2->poseparamindex = pData - pStart;
	pData += g_numposeparameters * sizeof( mstudioposeparamdesc_t );
	ALIGN( pData );

	for( i = 0; i < g_numposeparameters; i++ )
	{
		CopyStringTruncate( ppose[i].name, g_pose[i].name, sizeof( ppose[0].name ));
		ppose[i].start = g_pose[i].min;
		ppose[i].end = g_pose[i].max;
		ppose[i].flags = g_pose[i].flags;
		ppose[i].loop = g_pose[i].loop;
	}

	Msg( "ik/pose    %7d bytes\n", pData - pStart - total );

	pData = WriteAnimations( pData, pStart, 0 );
	pData = WriteAnimDescriptions( pData, pStart, 0 );

	WriteSequenceInfo( );
	Msg( "sequences   %6d bytes (%d frames) [%d:%02d]\n", pData - pStart - total, totalframes, (int)totalseconds / 60, (int)totalseconds % 60 );
	total  = pData - pStart;

	WriteModel( );
	Msg( "models     %7d bytes\n", pData - pStart - total );
	total  = pData - pStart;

	WriteKeyValues( phdr2, &g_KeyValueText );
	Msg( "keyvalues  %7d bytes\n", pData - pStart - total );

	// NOTE: textures must be last!
	WriteTextures( );
	Msg( "textures   %7d bytes\n", pData - pStart - total );

	total = pData - pStart;

	phdr->length = pData - pStart;

	Msg( "total     %s\n", Q_memprint( phdr->length ));

	Msg( "writing: %s\n", outname );

	COM_SaveFile( outname, pStart, phdr->length, true );

	// allow to write model but warn the user
	if( pData > pStart + FILEBUFFER )
		COM_FatalError( "WriteModel: memory buffer is corrupted. Check the model!\n" );
	Mem_Free( pStart );
}
