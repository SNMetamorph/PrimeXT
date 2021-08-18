/*
simplify.cpp - studio model simplification
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

#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "studio.h"
#include "studiomdl.h"
#include "iksolver.h"

#define ANIM_COMPRESS_THRESHOLD	0	// more values compress animation but can skip frames (optimal range between 0-100)
#define cmp_animvalue( x, y )		( abs( value[x] - value[y] ) <= ANIM_COMPRESS_THRESHOLD )

void AccumulateSeqLayers( Vector pos[], Vector4D q[], int sequence, float frame, float flWeight );
int g_rootIndex = 0;

//----------------------------------------------------------------------
// underlay:
// studiomdl : delta = new_anim * ( -1 * base_anim )
// engine : result = (w * delta) * base_anim
// 
// overlay
// 
// studiomdl : delta = (-1 * base_anim ) * new_anim
// engine : result = base_anim * (w * delta)
//
//----------------------------------------------------------------------
void QuaternionSMAngles( float s, Vector4D const &p, Vector4D const &q, Radian &angles )
{
	Vector4D qt;
	QuaternionSM( s, p, q, qt );
	QuaternionAngle( qt, angles );
}

void QuaternionMAAngles( Vector4D const &p, float s, Vector4D const &q, Radian &angles )
{
	Vector4D qt;
	QuaternionMA( p, s, q, qt );
	QuaternionAngle( qt, angles );
}

//-----------------------------------------------------------------------------
// Purpose: convert pBoneToWorld back into rot/pos data
//-----------------------------------------------------------------------------
void solveBone( s_animation_t *panim, int iFrame, int iBone, matrix3x4 *pBoneToWorld )
{
	int iParent = g_bonetable[iBone].parent;

	if( iParent == -1 )
	{
		pBoneToWorld[iBone].GetStudioTransform( panim->sanim[iFrame][iBone].pos, panim->sanim[iFrame][iBone].rot );
		return;
	}

	matrix3x4	worldToBone = pBoneToWorld[iParent].Invert();
	matrix3x4	local = worldToBone.ConcatTransforms( pBoneToWorld[iBone] );

	iFrame = iFrame % panim->numframes;

	local.GetStudioTransform( panim->sanim[iFrame][iBone].pos, panim->sanim[iFrame][iBone].rot );
}

bool AnimationDifferent( const Vector& startPos, const Radian& startRot, const Vector& pos, const Radian& rot )
{
	if( !VectorCompareEpsilon( startPos, pos, 0.01 ))
		return true;

	if( !RadianCompareEpsilon( startRot, rot, 0.01 ))
		return true;
	return false;
}

bool BoneHasAnimation( const char *pName )
{
	bool	first = true;
	Vector	pos;
	Radian	rot;

	if( !g_numani ) return false;

	int globalIndex = findGlobalBone( pName );

	// don't check root bones for animation
	if( globalIndex >= 0 && g_bonetable[globalIndex].parent == -1 )
		return true;

	// find used bones per g_model
	for( int i = 0; i < g_numani; i++ )
	{
		s_animation_t *panim = g_panimation[i];
		int boneIndex = findLocalBone( panim, pName );
		if( boneIndex < 0 ) continue;	// not in this source?

		// this is not right, but enough of the bones are moved unintentionally between
		// animations that I put this in to catch them.
		int n = panim->startframe - panim->source.startframe;
		first = true;

		for( int j = 0; j < panim->numframes; j++ )
		{
			if ( first )
			{
				pos = panim->rawanim[j+n][boneIndex].pos;
				rot = panim->rawanim[j+n][boneIndex].rot;
				first = false;
			}
			else
			{
				if( AnimationDifferent( pos, rot, panim->rawanim[j+n][boneIndex].pos, panim->rawanim[j+n][boneIndex].rot ))
					return true;
			}
		}
	}

	return false;
}

bool BoneHasAttachments( char const *pname )
{
	for( int k = 0; k < g_numattachments; k++ )
	{
		if( !Q_stricmp( g_attachment[k].bonename, pname ))
			return true;
	}

	return false;
}

int BoneIsProcedural( char const *pname )
{
	int	k;

	for( k = 0; k < g_numaxisinterpbones; k++ )
	{
		if( !Q_stricmp( g_axisinterpbones[k].bonename, pname ))
			return true;
	}

	for( k = 0; k < g_numquatinterpbones; k++ )
	{
		if( IsGlobalBoneXSI( g_quatinterpbones[k].bonename, pname ))
			return true;
	}

	for( k = 0; k < g_numaimatbones; k++ )
	{
		if( IsGlobalBoneXSI( g_aimatbones[k].bonename, pname ))
			return true;
	}

	for( k = 0; k < g_numjigglebones; k++ )
	{
		if( !Q_stricmp( g_jigglebones[k].bonename, pname ))
			return true;
	}
	return false;
}

bool BoneIsIK( char const *pname )
{
	// tag bones used by ikchains 
	for( int k = 0; k < g_numikchains; k++ )
	{
		if( !Q_stricmp( g_ikchain[k].bonename, pname ))
			return true;
	}
	return false;
}

bool BoneShouldCollapse( char const *pname )
{
	for( int k = 0; k < g_collapse.Count(); k++ )
	{
		if( !Q_stricmp( g_collapse[k].bonename, pname ))
			return true;
	}

	return ( !BoneHasAnimation( pname ) && !BoneIsProcedural( pname ) && !BoneIsIK( pname ) /* && !BoneHasAttachments( pname ) */);
}

//-----------------------------------------------------------------------------
// Purpose: Collapse vertex assignments up to parent on bones that are not needed
//			This can optimize a model substantially if the animator is using
//			lots of helper bones with no animation.
//-----------------------------------------------------------------------------
void CollapseBones( void )
{
	int	count = 0;
	int	j, k;

	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].bDontCollapse )
			continue;

		if(( g_bonetable[k].flags != 0 || g_bonetable[k].bPreDefined ) && !BoneShouldCollapse( g_bonetable[k].name ))
			continue;
		count++;

		MsgDev( D_NOTE, "collapsing %s\n", g_bonetable[k].name );

		g_numbones--;
		int m = g_bonetable[k].parent;

		for( j = k; j < g_numbones; j++ )
		{
			g_bonetable[j] = g_bonetable[j+1];

			if( g_bonetable[j].parent == k )
				g_bonetable[j].parent = m;
			else if( g_bonetable[j].parent >= k )
				g_bonetable[j].parent = g_bonetable[j].parent - 1;
		}
		k--;
	}

	if( count ) MsgDev( D_REPORT, "Collapsed %d bones\n", count );
}

//-----------------------------------------------------------------------------
// Purpose: replace all animation, rotation and translation, etc. with a single bone
//-----------------------------------------------------------------------------
void MakeStaticProp( void )
{
	matrix3x4	rotated = matrix3x4( g_vecZero, g_defaultrotation );
	Vector	centerOffset;
	int	i, j, k;

	// FIXME: missing attachment point recalcs!
	// replace bone 0 with "static_prop" bone and attach everything to it.
	for( i = 0; i < g_nummodels; i++ )
	{
		s_model_t	*pmodel = g_model[i];

		Q_strncpy( pmodel->localBone[0].name, "kHED", sizeof( pmodel->localBone[0].name ));	// g-cont :-)
		pmodel->localBone[0].parent = -1;

		for( k = 1; k < pmodel->numbones; k++ )
		{
			pmodel->localBone[k].parent = -1;
		}

		rotated.SetOrigin( g_defaultadjust );

		Vector mins, maxs;
		ClearBounds( mins, maxs );

		for( j = 0; j < pmodel->numsrcverts; j++ )
		{
			for( k = 0; k < pmodel->srcvert[j].localWeight.numbones; k++ )
			{
				// attach everything to root
				pmodel->srcvert[j].localWeight.bone[k] = 0;
			}

			// **shift everything into identity space**
			pmodel->srcvert[j].vert = rotated.VectorTransform( pmodel->srcvert[j].vert );

			// normal
			pmodel->srcvert[j].norm = rotated.VectorRotate( pmodel->srcvert[j].norm );

			// incrementally compute identity space bbox
			AddPointToBounds( pmodel->srcvert[j].vert, mins, maxs );
		}

		if( g_centerstaticprop )
		{
			const char *pAttachmentName = "placementOrigin";
			bool bFound = false;

			for( k = 0; k < g_numattachments; k++ )
			{
				if( !Q_stricmp( g_attachment[k].name, pAttachmentName ))
				{
					bFound = true;
					break;
				}
			}

			if( !bFound )
			{
				centerOffset = -0.5f * (mins + maxs);
			}

			for( j = 0; j < pmodel->numsrcverts; j++ )
			{
				pmodel->srcvert[j].vert += centerOffset;
			}

			if ( !bFound )
			{
				// now add an attachment point to store this offset
				Q_strncpy( g_attachment[g_numattachments].name, pAttachmentName, sizeof( g_attachment[0].name ));
				Q_strncpy( g_attachment[g_numattachments].bonename, "kHED", sizeof( g_attachment[0].name ));
				g_attachment[g_numattachments].local = matrix3x4( centerOffset, g_vecZero );
				g_attachment[g_numattachments].bone = 0;
				g_attachment[g_numattachments].type = 0;
				g_numattachments++;
			}
		}

		// force the bone to be identity
		pmodel->skeleton[0].pos = Vector( 0, 0, 0 );
		pmodel->skeleton[0].rot = Radian( 0, 0, 0 );
	
		// make an identity boneToPose transform
		pmodel->boneToPose[0].Identity();
	}

	// throw all specified hitboxes
	g_numbonecontrollers = 0;
	allow_boneweights = 0;
	g_numimportbones = 0;
	has_boneweights = 0;
	g_hitboxsets.Purge();
	g_BoneMerge.Purge();

	// throw away all animations
	g_panimation[0]->numframes = 1;
	g_panimation[0]->startframe = 0;
	g_panimation[0]->endframe = 1;
	g_panimation[0]->source.numframes = 1;
	g_panimation[0]->source.startframe = 0;
	g_panimation[0]->source.endframe = 1;
	Q_strncpy( g_panimation[0]->name, "seq-name", sizeof( g_panimation[0]->name ));
	g_panimation[0]->rotation = Radian( 0, 0, 0 );
	g_panimation[0]->adjust = Vector( 0, 0, 0 );
	g_panimation[0]->fps = 30.0f;

	g_numani = 1;

	// recalc attachment points:
	for( i = 0; i < g_numattachments; i++ )
	{
		if( g_centerstaticprop && ( i == g_numattachments - 1 ))
			continue;
		
		Q_strncpy( g_attachment[i].bonename, "kHED", sizeof( g_attachment[i].name ));
		g_attachment[i].local = rotated.ConcatTransforms( g_attachment[i].local );
		g_attachment[i].bone = 0;
		g_attachment[i].type = 0;
	}

	// throw away all sequences
	Q_strncpy( g_sequence[0].name, "seq-name", sizeof( g_sequence[0].name ));
	g_sequence[0].panim[0] = g_panimation[0];
	g_sequence[0].fps = g_panimation[0]->fps;
	g_sequence[0].numautolayers = 0;
	g_sequence[0].activity = 0;
	g_sequence[0].numframes = 1;
	g_sequence[0].numblends = 1;
	g_sequence[0].numevents = 0;
	g_sequence[0].seqgroup = 0;
	g_sequence[0].paramindex[0] = -1;
	g_sequence[0].paramindex[1] = -1;
	g_sequence[0].groupsize[0] = 0;
	g_sequence[0].groupsize[1] = 0;
	g_sequence[0].fadeintime = 0.2f;
	g_sequence[0].fadeouttime = 0.2f;
	g_sequence[0].numiklocks = 0;
	g_sequence[0].numikrules = 0;
	g_sequence[0].flags = 0;
	g_numseq = 1;
}

//-----------------------------------------------------------------------------
// Purpose: set "boneref" for all the source bones used by vertices, attachments, etc.
//-----------------------------------------------------------------------------
void TagUsedBones( void )
{
	int	i, j, k, n;

	// find used bones
	for( i = 0; i < g_nummodels; i++ )
	{
		s_model_t	*pmodel = g_model[i];

		for( k = 0; k < MAXSTUDIOSRCBONES; k++ )
		{
			pmodel->boneflags[k] = 0;
			pmodel->boneref[k] = 0;
		}

		for( j = 0; j < pmodel->numsrcverts; j++ )
		{
			for( k = 0; k < pmodel->srcvert[j].localWeight.numbones; k++ )
			{
				SetBits( pmodel->boneflags[pmodel->srcvert[j].localWeight.bone[k]], BONE_USED_BY_VERTEX );
			}
		}

		for( k = 0; k < g_numattachments; k++ )
		{
			for( j = 0; j < pmodel->numbones; j++ )
			{
				if( !Q_stricmp( g_attachment[k].bonename, pmodel->localBone[j].name ))
				{
					// this bone is a keeper with or without associated vertices
					// because an attachment point depends on it.
					if( FBitSet( g_attachment[k].type, IS_RIGID ))
					{
						for( n = j; n != -1; n = pmodel->localBone[n].parent )
						{
							if( FBitSet( pmodel->boneflags[n], BONE_USED_BY_VERTEX ))
							{
								SetBits( pmodel->boneflags[n], BONE_USED_BY_ATTACHMENT );
								break;
							}
						}
					}
					else
					{
						SetBits( pmodel->boneflags[j], BONE_USED_BY_ATTACHMENT );
					}
				}
			}
		}

		for( k = 0; k < g_numikchains; k++ )
		{
			for( j = 0; j < pmodel->numbones; j++ )
			{
				if( !Q_stricmp( g_ikchain[k].bonename, pmodel->localBone[j].name ))
				{
					// this bone is a keeper with or without associated vertices
					// because a ikchain depends on it.
					SetBits( pmodel->boneflags[j], BONE_USED_BY_ATTACHMENT );
				}
			}
		}

		// Tag all bones marked as being used by bonemerge
		int nBoneMergeCount = g_BoneMerge.Count(); 
		for( k = 0; k < nBoneMergeCount; ++k )
		{
			for( j = 0; j < pmodel->numbones; j++ )
			{
				if( !Q_stricmp( g_BoneMerge[k].bonename, pmodel->localBone[j].name ))
					continue;

				SetBits( pmodel->boneflags[j], BONE_USED_BY_BONE_MERGE );
			}
		}

		// NOTE: This must come last; after all flags have been set!
		// tag bonerefs as being used the union of the boneflags all their children
		for( k = 0; k < MAXSTUDIOSRCBONES; k++ )
		{
			if( !pmodel->boneflags[k] )
				continue;

			// tag parent bones as used if child has been used
			pmodel->boneref[k] |= pmodel->boneflags[k];

			n = g_model[i]->localBone[k].parent;
			while( n != -1 )
			{
				pmodel->boneref[n] |= pmodel->boneref[k];
				n = g_model[i]->localBone[n].parent;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: change the names in the source files for bones that max auto-renamed on us
//-----------------------------------------------------------------------------
void RenameBones( void )
{
	int	i, j, k;

	// rename model bones if needed
	for( i = 0; i < g_nummodels; i++ )
	{
		for( j = 0; j < g_model[i]->numbones; j++ )
		{
			for( k = 0; k < g_numrenamedbones; k++ )
			{
				if( !Q_strcmp( g_model[i]->localBone[j].name, g_renamedbone[k].from ))
				{
					Q_strncpy( g_model[i]->localBone[j].name, g_renamedbone[k].to, MAXSRCSTUDIONAME );
					break;
				}
			}
		}
	}

	// rename sequence bones if needed
	for( i = 0; i < g_numani; i++ )
	{
		for( j = 0; j < g_panimation[i]->numbones; j++ )
		{
			for( k = 0; k < g_numrenamedbones; k++ )
			{
				if( !Q_strcmp( g_panimation[i]->localBone[j].name, g_renamedbone[k].from ))
				{
					Q_strncpy( g_panimation[i]->localBone[j].name, g_renamedbone[k].to, MAXSRCSTUDIONAME );
					break;
				}
			}
		}
	}
}

void BuildGlobalBoneToPose( void )
{
	// build reference pose
	for( int j = 0; j < g_numbones; j++ )
	{
		if( g_bonetable[j].parent == -1 ) g_bonetable[j].boneToPose = g_bonetable[j].rawLocal;
		else g_bonetable[j].boneToPose = g_bonetable[g_bonetable[j].parent].boneToPose.ConcatTransforms( g_bonetable[j].rawLocal );
	}
}

void EnforceHierarchy( void )
{
	bool	bSort = true;
	int	i, j, k, count = 0;

	// force changes to hierarchy
	for( i = 0; i < g_numforcedhierarchy; i++ )
	{
		j = findGlobalBone( g_forcedhierarchy[i].parentname );
		k = findGlobalBone( g_forcedhierarchy[i].childname );

		if( j == -1 && Q_strlen( g_forcedhierarchy[i].parentname ) > 0 )
		{
			COM_FatalError( "unknown bone: \"%s\" in forced hierarchy\n", g_forcedhierarchy[i].parentname );
		}

		if( k == -1 )
		{
			COM_FatalError( "unknown bone: \"%s\" in forced hierarchy\n", g_forcedhierarchy[i].childname );
		}

		g_bonetable[k].parent = j;
	}

	while( bSort ) 
	{
		count++;
		bSort = false;
		for( i = 0; i < g_numbones; i++ )
		{
			if( g_bonetable[i].parent > i )
			{
				// swap
				j = g_bonetable[i].parent;
				s_bonetable_t tmp;
				tmp = g_bonetable[i];
				g_bonetable[i] = g_bonetable[j];
				g_bonetable[j] = tmp;

				// relink parents
				for( k = i; k < g_numbones; k++ )
				{
					if( g_bonetable[k].parent == i )
						g_bonetable[k].parent = j;
					else if( g_bonetable[k].parent == j )
						g_bonetable[k].parent = i;
				}
				bSort = true;
			}
		}

		if( count > 1000 )
			COM_FatalError( "circular bone heirarchy\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: build transforms in source space, assuming source bones
//-----------------------------------------------------------------------------
void BuildRawTransforms( s_model_t const *pmodel, Vector const &shift, Radian const &rotate, matrix3x4 *boneToWorld )
{
	matrix3x4	rootxform = matrix3x4( g_vecZero, rotate );
	matrix3x4	bonematrix;

	// build source space local to world transforms
	for( int k = 0; k < pmodel->numbones; k++ )
	{
		Radian	rot = pmodel->skeleton[k].rot;
		Vector	pos = pmodel->skeleton[k].pos;

		if( pmodel->localBone[k].parent == -1 )
		{
			// translate
			Vector	tmp = pos - shift;

			// rotate
			pos = rootxform.VectorRotate( tmp );
			bonematrix = rootxform.ConcatTransforms( matrix3x4( g_vecZero, rot ));
			bonematrix.GetStudioTransform( tmp, rot );
			clip_rotations( rot );
		}

		bonematrix = matrix3x4( pos, rot );

		if( pmodel->localBone[k].parent == -1 ) boneToWorld[k] = bonematrix;
		else boneToWorld[k] = boneToWorld[pmodel->localBone[k].parent].ConcatTransforms( bonematrix );
	}
}

//-----------------------------------------------------------------------------
// Purpose: build transforms in source space, assuming source bones
//-----------------------------------------------------------------------------
void BuildRawTransforms( s_animation_t const *panim, int frame, Vector const &shift, Radian const &rotate, matrix3x4 *boneToWorld )
{
	matrix3x4	rootxform = matrix3x4( g_vecZero, rotate );
	matrix3x4	bonematrix;

	if( FBitSet( panim->flags, STUDIO_LOOPING ))
	{
		if( frame )
		{
			while( frame < 0)
				frame += panim->source.numframes;
			frame = frame % panim->source.numframes;
		}
	}
	else
	{
		frame = bound( 0, frame, panim->source.numframes - 1 );
	}

	// build source space local to world transforms
	for( int k = 0; k < panim->numbones; k++ )
	{
		Radian	rot = panim->rawanim[frame][k].rot;
		Vector	pos = panim->rawanim[frame][k].pos;

		if( panim->localBone[k].parent == -1)
		{
			// translate
			Vector	tmp = pos - shift;

			// rotate
			pos = rootxform.VectorRotate( tmp );
			bonematrix = rootxform.ConcatTransforms( matrix3x4( g_vecZero, rot ));
			bonematrix.GetStudioTransform( tmp, rot );
			clip_rotations( rot );
		}

		bonematrix = matrix3x4( pos, rot );

		if( panim->localBone[k].parent == -1 ) boneToWorld[k] = bonematrix;
		else boneToWorld[k] = boneToWorld[panim->localBone[k].parent].ConcatTransforms( bonematrix );
	}
}

void RebuildLocalPose( void )
{
	matrix3x4	boneToPose[MAXSTUDIOBONES];
	matrix3x4	poseToBone[MAXSTUDIOBONES];
	int	j;

	// build reference pose
	for( j = 0; j < g_numbones; j++ )
		boneToPose[j] = g_bonetable[j].boneToPose;

	// rebuild local pose
	for( j = 0; j < g_numbones; j++ )
	{
		if( g_bonetable[j].parent == -1 ) g_bonetable[j].rawLocal = boneToPose[j];
		else g_bonetable[j].rawLocal = poseToBone[g_bonetable[j].parent].ConcatTransforms( boneToPose[j] );

		g_bonetable[j].rawLocal.GetStudioTransform( g_bonetable[j].pos, g_bonetable[j].rot );
		g_bonetable[j].boneToPose = boneToPose[j];
		poseToBone[j] = boneToPose[j].Invert();
	}
}

//-----------------------------------------------------------------------------
// Tags bones in the global bone table
//-----------------------------------------------------------------------------
void TagUsedImportedBones( void )
{
	// NOTE: This has to happen because some bones referenced by bonemerge
	// can be set up using the importbones feature
	int	k, j;

	// Tag all bones marked as being used by bonemerge
	int nBoneMergeCount = g_BoneMerge.Count(); 
	for( k = 0; k < nBoneMergeCount; k++ )
	{
		for( j = 0; j < g_numbones; j++ )
		{
			if( Q_stricmp( g_BoneMerge[k].bonename, g_bonetable[j].name ))
				continue;
			g_bonetable[j].flags |= BONE_USED_BY_BONE_MERGE;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: look through all the sources and build a table of used bones
//-----------------------------------------------------------------------------
void BuildGlobalBonetable( void )
{
	int	i, j, k, n;

	g_numbones = 0;

	for( i = 0; i < MAXSTUDIOSRCBONES; i++ )
	{
		g_bonetable[i].srcRealign.Identity();
	}

	// insert predefined bones first
	for( i = 0; i < g_numimportbones; i++ )
	{
		k = findGlobalBone( g_importbone[i].name );

		if( k == -1 )
		{
			k = g_numbones;
			Q_strncpy( g_bonetable[k].name, g_importbone[i].name, sizeof( g_bonetable[0].name ));

			if( Q_strlen( g_importbone[i].parent ) == 0 )
			{
				g_bonetable[k].parent = -1;
			}
			else
			{
				// FIXME: This won't work if the imported bone refers to
				// another imported bone which is further along in the list
				g_bonetable[k].parent = findGlobalBone( g_importbone[i].parent );
				if( g_bonetable[k].parent == -1 )
				{
					MsgDev( D_WARN, "Imported bone %s tried to access parent bone %s and failed!\n",
						g_importbone[i].name, g_importbone[i].parent );
				}
			}

			g_bonetable[k].bPreDefined = true;
			g_bonetable[k].rawLocal = g_importbone[i].rawLocal;
			g_bonetable[k].rawLocalOriginal = g_bonetable[k].rawLocal;
			g_numbones++;
		}

		g_bonetable[k].bDontCollapse = true;
		g_bonetable[k].srcRealign = g_importbone[i].srcRealign;
		g_bonetable[k].bPreAligned = true;
	}

	TagUsedImportedBones();

	// union of all used bones
	for( i = 0; i < g_nummodels; i++ )
	{
		s_model_t	*pmodel = g_model[i];

		matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
		BuildRawTransforms( pmodel, g_vecZero, g_radZero, srcBoneToWorld );

		for( j = 0; j < pmodel->numbones; j++ )
		{
			if( g_collapse_bones_aggressive )
			{
				if( pmodel->boneflags[j] == 0 )
					continue;
			}
			else
			{
				if( pmodel->boneref[j] == 0 )
					continue;
			}

			k = findGlobalBone( pmodel->localBone[j].name );

			if( k == -1 )
			{
				// create new bone
				k = g_numbones;
				Q_strncpy( g_bonetable[k].name, pmodel->localBone[j].name, sizeof( g_bonetable[0].name ));

				if(( n = pmodel->localBone[j].parent ) != -1 )
					g_bonetable[k].parent = findGlobalBone( pmodel->localBone[n].name );
				else g_bonetable[k].parent = -1;

				g_bonetable[k].bonecontroller	= 0;
				g_bonetable[k].flags = pmodel->boneflags[j];

				if( g_bonetable[k].parent == -1 || !g_bonetable[g_bonetable[k].parent].bPreAligned )
				{
					g_bonetable[k].rawLocal = matrix3x4( pmodel->skeleton[j].pos, pmodel->skeleton[j].rot );
					g_bonetable[k].rawLocalOriginal = g_bonetable[k].rawLocal;
				}
				else
				{
					// convert the local relative position into a realigned relative position
					matrix3x4 srcParentBoneToWorld;
					srcParentBoneToWorld = srcBoneToWorld[n].ConcatTransforms( g_bonetable[g_bonetable[k].parent].srcRealign );
					matrix3x4 invSrcParentBoneToWorld = srcParentBoneToWorld.Invert();
					g_bonetable[k].rawLocal = invSrcParentBoneToWorld.ConcatTransforms( srcBoneToWorld[j] );
				}

				g_bonetable[k].rawLocal = matrix3x4( pmodel->skeleton[j].pos, pmodel->skeleton[j].rot );
				g_bonetable[k].rawLocalOriginal = g_bonetable[k].rawLocal;

				g_bonetable[k].boneToPose.Identity();	// in original code this was Invalidate (write NAN at each section)
				g_numbones++;
				continue;
			}

			if( g_overridebones && g_bonetable[k].bPreDefined )
			{
				g_bonetable[k].flags |= pmodel->boneflags[j];

				g_bonetable[k].boneToPose = srcBoneToWorld[j].ConcatTransforms( g_bonetable[k].srcRealign ); 

				if( g_bonetable[k].parent == -1 )
				{
					g_bonetable[k].rawLocal = g_bonetable[k].boneToPose;
				}
				else
				{
					matrix3x4 tmp = g_bonetable[g_bonetable[k].parent].boneToPose.Invert();
					g_bonetable[k].rawLocal = tmp.ConcatTransforms( g_bonetable[k].boneToPose ); 
				}
				continue;
			}

			// accumlate flags
			g_bonetable[k].flags |= pmodel->boneflags[j];
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: find procedural bones and tag for inclusion even if they don't animate
//-----------------------------------------------------------------------------
void TagProceduralBones( void )
{
	int	numaxisinterpbones = 0;
	int	numquatinterpbones = 0;
	int	numaimatbones = 0;
	int	numjigglebones = 0;
	int	j;
	
	// look for AxisInterp bone definitions
	for( j = 0; j < g_numaxisinterpbones; j++ )
	{
		g_axisinterpbones[j].bone = findGlobalBone( g_axisinterpbones[j].bonename );
		g_axisinterpbones[j].control = findGlobalBone( g_axisinterpbones[j].controlname );

		if( g_axisinterpbones[j].bone == -1 ) 
		{
			MsgDev( D_WARN, "axisinterpbone \"%s\" unused\n", g_axisinterpbones[j].bonename );
			continue; // optimized out, don't complain
		}

		if( g_axisinterpbones[j].control == -1 )
		{
			COM_FatalError( "Missing control bone \"%s\" for procedural bone \"%s\"\n",
			g_axisinterpbones[j].bonename, g_axisinterpbones[j].controlname );
		}

		g_bonetable[g_axisinterpbones[j].bone].flags |= BONE_ALWAYS_PROCEDURAL; // ??? what about physics rules
		g_axisinterpbonemap[numaxisinterpbones++] = j;
	}
	g_numaxisinterpbones = numaxisinterpbones;

	// look for QuatInterp bone definitions
	for( j = 0; j < g_numquatinterpbones; j++ )
	{
		g_quatinterpbones[j].bone = findGlobalBoneXSI( g_quatinterpbones[j].bonename );
		g_quatinterpbones[j].control = findGlobalBoneXSI( g_quatinterpbones[j].controlname );

		if( g_quatinterpbones[j].bone == -1 ) 
		{
			MsgDev( D_WARN, "quatinterpbone \"%s\" unused\n", g_quatinterpbones[j].bonename );
			continue; // optimized out, don't complain
		}

		if( g_quatinterpbones[j].control == -1 )
		{
			COM_FatalError( "Missing control bone \"%s\" for procedural bone \"%s\"\n",
			g_quatinterpbones[j].bonename, g_quatinterpbones[j].controlname );
		}

		g_bonetable[g_quatinterpbones[j].bone].flags |= BONE_ALWAYS_PROCEDURAL; // ??? what about physics rules
		g_quatinterpbonemap[numquatinterpbones++] = j;
	}
	g_numquatinterpbones = numquatinterpbones;

	// look for AimAt bone definitions
	for( j = 0; j < g_numaimatbones; j++ )
	{
		g_aimatbones[j].bone = findGlobalBoneXSI( g_aimatbones[j].bonename );

		if( g_aimatbones[j].bone == -1 ) 
		{
			MsgDev( D_WARN, "<aimconstraint> \"%s\" unused\n", g_aimatbones[j].bonename );
			continue; // optimized out, don't complain
		}

		g_aimatbones[j].parent = findGlobalBoneXSI( g_aimatbones[j].parentname );

		if( g_aimatbones[j].parent == -1 )
		{
			COM_FatalError( "Missing parent control bone \"%s\" for procedural bone \"%s\"\n",
			g_aimatbones[j].parentname, g_aimatbones[j].bonename );
		}

		// Look for the aim bone as an attachment first
		g_aimatbones[j].aimAttach = -1;

		for( int ai = 0; ai < g_numattachments; ai++ )
		{
			if( !Q_strcmp( g_attachment[ai].name, g_aimatbones[j].aimname ))
			{
				g_aimatbones[j].aimAttach = ai;
				break;
			}
		}

		if( g_aimatbones[j].aimAttach == -1 )
		{
			g_aimatbones[j].aimBone = findGlobalBoneXSI( g_aimatbones[j].aimname );

			if( g_aimatbones[j].aimBone == -1 )
			{
				COM_FatalError( "Missing aim control attachment or bone \"%s\" for procedural bone \"%s\"\n",
				g_aimatbones[j].aimname, g_aimatbones[j].bonename );
			}
		}

		g_bonetable[g_aimatbones[j].bone].flags |= BONE_ALWAYS_PROCEDURAL; // ??? what about physics rules
		g_aimatbonemap[numaimatbones++] = j;
	}
	g_numaimatbones = numaimatbones;

	// look for Jiggle bone definitions
	for( j = 0; j < g_numjigglebones; j++ )
	{
		g_jigglebones[j].bone = findGlobalBone( g_jigglebones[j].bonename );

		if( g_jigglebones[j].bone == -1 ) 
		{
			MsgDev( D_WARN, "jigglebone \"%s\" unused\n", g_jigglebones[j].bonename );
			continue; // optimized out, don't complain
		}

		g_bonetable[g_jigglebones[j].bone].flags |= BONE_JIGGLE_PROCEDURAL; // ??? what about physics rules
		g_jigglebonemap[numjigglebones++] = j;
	}
	g_numjigglebones = numjigglebones;
}

//-----------------------------------------------------------------------------
// Purpose: convert original procedural bone info into correct values for existing skeleton
//-----------------------------------------------------------------------------
void RemapProceduralBones( void )
{
	int	j;

	// look for QuatInterp bone definitions
	for( j = 0; j < g_numquatinterpbones; j++ )
	{
		s_quatinterpbone_t	*pInterp = &g_quatinterpbones[g_quatinterpbonemap[j]];
		int origParent = findGlobalBoneXSI( pInterp->parentname );
		int origControlParent = findGlobalBoneXSI( pInterp->controlparentname );

		if( origParent == -1 )
		{
			COM_FatalError( "procedural bone \"%s\", can't find orig parent \"%s\"\n\n", pInterp->bonename, pInterp->parentname );
		}

		if( origControlParent == -1 )
		{
			COM_FatalError( "procedural bone \"%s\", can't find control parent \"%s\n\n", pInterp->bonename, pInterp->controlparentname );
		}

		if( g_bonetable[pInterp->bone].parent != origParent )
		{
			COM_FatalError( "unknown procedural bone parent remapping\n" );
		}

		if( g_bonetable[pInterp->control].parent != origControlParent )
		{
			COM_FatalError( "procedural bone \"%s\", parent remapping error, control parent was \"%s\", is now \"%s\"\n",
				pInterp->bonename, pInterp->controlparentname, g_bonetable[g_bonetable[pInterp->control].parent].name );
		}

		// remap triggers and movements/rotations due to skeleton changes and realignment
		for( int k = 0; k < pInterp->numtriggers; k++ )
		{
			int	parent = g_bonetable[pInterp->control].parent;

			// triggers are the "control" bone relative to the control's parent bone
			if( parent != -1 )
			{
				matrix3x4 invControlParentRealign = g_bonetable[parent].srcRealign.Invert();
				matrix3x4 srcControlParentBoneToPose = g_bonetable[parent].boneToPose.ConcatTransforms( invControlParentRealign );
				matrix3x4 srcControlRelative = matrix3x4( g_vecZero, pInterp->trigger[k] );
				matrix3x4 srcControlBoneToPose = srcControlParentBoneToPose.ConcatTransforms( srcControlRelative );
				matrix3x4 destControlParentBoneToPose = srcControlParentBoneToPose.ConcatTransforms( g_bonetable[parent].srcRealign );
				matrix3x4 destControlBoneToPose = srcControlBoneToPose.ConcatTransforms( g_bonetable[pInterp->control].srcRealign );
				matrix3x4 invDestControlParentBoneToPose = destControlParentBoneToPose.Invert();
				matrix3x4 destControlRelative = invDestControlParentBoneToPose.ConcatTransforms( destControlBoneToPose );

				// FIXME: do revision
				pInterp->trigger[k] = destControlRelative.GetQuaternion();
#if 0
				Vector	pos = srcControlRelative.GetOrigin();
				Vector	rot = srcControlRelative.GetAngles();

				Msg( "srcControlRelative  : %7.2f %7.2f %7.2f\n", rot.x, rot.y, rot.z );

				pos = destControlRelative.GetOrigin();
				rot = destControlRelative.GetAngles();

				Msg( "destControlRelative : %7.2f %7.2f %7.2f\n", rot.x, rot.y, rot.z );

				Msg( "\n" );
#endif
			}

			// movements are relative to the bone's parent
			parent = g_bonetable[pInterp->bone].parent;

			if( parent != -1 )
			{
				// Msg( "procedural bone \"%s\"\n", pInterp->bonename );
				// Msg( "pre  : %7.2f %7.2f %7.2f\n", pInterp->pos[k].x, pInterp->pos[k].y, pInterp->pos[k].z );
				// get local transform
				matrix3x4 srcParentRelative = matrix3x4( pInterp->pos[k] + pInterp->basepos, pInterp->quat[k] );

				// get original boneToPose
				matrix3x4 invSrcRealign = g_bonetable[parent].srcRealign.Invert();
				matrix3x4 origParentBoneToPose = g_bonetable[parent].boneToPose.ConcatTransforms( invSrcRealign );

				// move bone adjustment into world position
				matrix3x4 srcBoneToWorld = origParentBoneToPose.ConcatTransforms( srcParentRelative );

				// calculate local transform
				matrix3x4 parentPoseToBone = g_bonetable[parent].boneToPose.Invert();
				matrix3x4 destBoneToWorld =parentPoseToBone.ConcatTransforms( srcBoneToWorld );

				// save out the local transform
				pInterp->quat[k] = destBoneToWorld.GetQuaternion();
				pInterp->pos[k] = destBoneToWorld.GetOrigin();

				pInterp->pos[k] += g_bonetable[pInterp->control].pos * pInterp->percentage;

				// Msg("post : %7.2f %7.2f %7.2f\n", pInterp->pos[k].x, pInterp->pos[k].y, pInterp->pos[k].z );
			}

		}
	}

	// look for aimatbones
	for( j = 0; j < g_numaimatbones; j++ )
	{
		s_aimatbone_t *pAimAtBone = &g_aimatbones[g_aimatbonemap[j]];

		int origParent = findGlobalBoneXSI( pAimAtBone->parentname );

		if( origParent == -1 )
		{
			COM_FatalError( "<aimconstraint> bone \"%s\", can't find parent bone \"%s\"\n\n", pAimAtBone->bonename, pAimAtBone->parentname );
		}

		int origAim = -1;

		for( int ai = 0; ai < g_numattachments; ++ai )
		{
			if( !Q_strcmp( g_attachment[ai].name, pAimAtBone->aimname ))
			{
				origAim = ai;
				break;
			}
		}

		if( origAim == -1 )
		{
			COM_FatalError( "<aimconstraint> bone \"%s\", can't find aim bone \"%s\n\n", pAimAtBone->bonename, pAimAtBone->aimname );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: propogate procedural bone usage up its chain
//-----------------------------------------------------------------------------
void MarkProceduralBoneChain( void )
{
	int	k, fBoneFlags;

	// look for QuatInterp bone definitions
	for( int j = 0; j < g_numquatinterpbones; j++ )
	{
		s_quatinterpbone_t *pInterp = &g_quatinterpbones[g_quatinterpbonemap[j]];
	
		fBoneFlags = g_bonetable[pInterp->bone].flags & BONE_USED_MASK;

		// propogate the procedural bone usage up its hierarchy
		k = pInterp->control;
		while( k != -1 )
		{
			g_bonetable[k].flags |= fBoneFlags;
			k = g_bonetable[k].parent;
		}

		// propogate the procedural bone usage up its hierarchy
		k = pInterp->bone;
		while( k != -1 )
		{
			g_bonetable[k].flags |= fBoneFlags;
			k = g_bonetable[k].parent;
		}
	}
}

void MapSourcesToGlobalBonetable( void )
{
	int	i, j, k;

	// map each source bone list to master list
	for( i = 0; i < g_nummodels; i++ )
	{
		for( k = 0; k < MAXSTUDIOSRCBONES; k++ )
		{
			g_model[i]->boneGlobalToLocal[k] = -1;
			g_model[i]->boneLocalToGlobal[k] = -1;
		}

		for( j = 0; j < g_model[i]->numbones; j++ )
		{
			k = findGlobalBone( g_model[i]->localBone[j].name );
			
			if( k == -1 )
			{
				int m = g_model[i]->localBone[j].parent;
				while( m != -1 && ( k = findGlobalBone( g_model[i]->localBone[m].name )) == -1 )
				{
					m = g_model[i]->localBone[m].parent;
				}

				if( k == -1 ) k = 0;

				g_model[i]->boneLocalToGlobal[j] = k;
			}
			else
			{
				g_model[i]->boneLocalToGlobal[j] = k;
				g_model[i]->boneGlobalToLocal[k] = j;
			}
		}
	}

	// map each sequences bone list to master list
	for( i = 0; i < g_numani; i++ )
	{
		s_animation_t *panim = g_panimation[i];

		for( k = 0; k < MAXSTUDIOSRCBONES; k++ )
		{
			panim->boneGlobalToLocal[k] = -1;
			panim->boneLocalToGlobal[k] = -1;
		}

		for( j = 0; j < panim->numbones; j++ )
		{
			k = findGlobalBone( panim->localBone[j].name );
			
			if( k == -1 )
			{
				int m = panim->localBone[j].parent;
				while( m != -1 && ( k = findGlobalBone( panim->localBone[m].name )) == -1 )
				{
					m = panim->localBone[m].parent;
				}

				if( k == -1 ) k = 0;

				panim->boneLocalToGlobal[j] = k;
			}
			else
			{
				panim->boneLocalToGlobal[j] = k;
				panim->boneGlobalToLocal[k] = j;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: go through bone and find any that arent aligned on the X axis
//-----------------------------------------------------------------------------
void RealignBones( void )
{
	matrix3x4	boneToPose[MAXSTUDIOBONES];
	int	childbone[MAXSTUDIOBONES];
	matrix3x4	poseToBone, bonematrix;
	int	i, j, k;

	for( j = 0; j < g_numbones; j++ )
		childbone[j] = -1;

	// force bones with IK rules to realign themselves
	for( i = 0; i < g_numikchains; i++ )
	{
		k = g_ikchain[i].link[0].bone;
		if( childbone[k] == -1 || childbone[k] == g_ikchain[i].link[1].bone )
		{
			childbone[k] = g_ikchain[i].link[1].bone;
		}
		else
		{
			COM_FatalError( "Trying to realign bone \"%s\" with two children \"%s\", \"%s\"\n", 
			g_bonetable[k].name, g_bonetable[childbone[k]].name, g_bonetable[g_ikchain[i].link[1].bone].name );
		}

		k = g_ikchain[i].link[1].bone;
		if( childbone[k] == -1 || childbone[k] == g_ikchain[i].link[2].bone )
		{
			childbone[k] = g_ikchain[i].link[2].bone;
		}
		else
		{
			COM_FatalError( "Trying to realign bone \"%s\" with two children \"%s\", \"%s\"\n", 
			g_bonetable[k].name, g_bonetable[childbone[k]].name, g_bonetable[g_ikchain[i].link[2].bone].name );
		}
	}

	if( g_realignbones )
	{
		int	children[MAXSTUDIOBONES];

		// count children
		for( k = 0; k < g_numbones; k++ )
		{
			children[k] = 0;
		}

		for( k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].parent != -1 )
				children[g_bonetable[k].parent]++;
		}

		// if my parent bone only has one child, then tell it to align to me
		for( k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].parent != -1 && children[g_bonetable[k].parent] == 1 )
				childbone[g_bonetable[k].parent] = k;
		}
	}

	for( j = 0; j < g_numbones; j++ )
		boneToPose[j] = g_bonetable[j].boneToPose;

	// look for bones that aren't on a primary X axis
	for( k = 0; k < g_numbones; k++ )
	{
		if( !g_bonetable[k].bPreAligned && childbone[k] != -1 )
		{
			float d = g_bonetable[childbone[k]].pos.Length();

			// check to see that it's on positive X
			if( d - g_bonetable[childbone[k]].pos.x > 0.01 )
			{
				Vector forward, left, up;
				Vector v2, v3;

				// calc X axis
				g_bonetable[childbone[k]].boneToPose.GetOrigin( v2 );
				g_bonetable[k].boneToPose.GetOrigin( v3 );
				forward = (v2 - v3).Normalize();

				// try to align to existing bone/boundingbox by finding most perpendicular 
				// existing axis and aligning the new Z axis to it.
				Vector forward2 = boneToPose[k].GetForward();
				Vector left2 = boneToPose[k].GetRight(); // FIXME: wrong name
				Vector up2 = boneToPose[k].GetUp();

				float d1 = fabs( DotProduct( forward, forward2 ));
				float d2 = fabs( DotProduct( forward, left2 ));
				float d3 = fabs( DotProduct( forward, up2 ));

				if( d1 <= d2 && d1 <= d3 )
				{
					up = CrossProduct( forward, forward2 ).Normalize();
				}
				else if( d2 <= d1 && d2 <= d3 )
				{
					up = CrossProduct( forward, left2 ).Normalize();
				}
				else
				{
					up = CrossProduct( forward, up2 ).Normalize();
				}

				left = CrossProduct( up, forward );

				// setup matrix
				boneToPose[k].SetForward( forward );
				boneToPose[k].SetRight( left );
				boneToPose[k].SetUp( up );

				// check orthonormality of matrix
				d = fabs( DotProduct( forward, left )) 
				+ fabs( DotProduct( left, up )) 
				+ fabs( DotProduct( up, forward ))
				+ fabs( DotProduct( boneToPose[k].GetForward(), boneToPose[k].GetRight() ))
				+ fabs( DotProduct( boneToPose[k].GetRight(), boneToPose[k].GetUp() ))
				+ fabs( DotProduct( boneToPose[k].GetUp(), boneToPose[k].GetForward() ));

				if( d > 0.0001 )
					COM_FatalError( "error with realigning bone %s\n", g_bonetable[k].name );
				boneToPose[k].SetOrigin( v3 );
			}
		}
	}

	for( i = 0; i < g_numforcedrealign; i++ )
	{
		k = findGlobalBone( g_forcedrealign[i].name );
		if( k == -1 )
		{
			COM_FatalError( "unknown bone %s in $forcedrealign\n", g_forcedrealign[i].name );
		}

		matrix3x4 local = matrix3x4( g_vecZero, g_forcedrealign[i].rot );
		boneToPose[k] = boneToPose[k].ConcatTransforms( local );
	}

	// build realignment transforms
	for( j = 0; j < g_numbones; j++ )
	{
		if( g_bonetable[j].bPreAligned )
			continue;

		poseToBone = g_bonetable[j].boneToPose.Invert();
		g_bonetable[j].srcRealign = poseToBone.ConcatTransforms( boneToPose[j] );
		g_bonetable[j].boneToPose = boneToPose[j];
	}

	// rebuild default angles, position, etc.
	for( j = 0; j < g_numbones; j++ )
	{
		if( g_bonetable[j].bPreAligned )
			continue;

		if( g_bonetable[j].parent == -1 )
		{
			bonematrix = g_bonetable[j].boneToPose;
		}
		else
		{
			// convert my transform into parent relative space
			poseToBone = g_bonetable[g_bonetable[j].parent].boneToPose.Invert();
			bonematrix = poseToBone.ConcatTransforms( g_bonetable[j].boneToPose );
		}

		bonematrix.GetStudioTransform( g_bonetable[j].pos, g_bonetable[j].rot );
	}

	// build reference pose
	for( j = 0; j < g_numbones; j++ )
	{
		bonematrix = matrix3x4( g_bonetable[j].pos, g_bonetable[j].rot );

		if( g_bonetable[j].parent == -1 ) g_bonetable[j].boneToPose = bonematrix;
		else g_bonetable[j].boneToPose = g_bonetable[g_bonetable[j].parent].boneToPose.ConcatTransforms( bonematrix );
	}
}

//-----------------------------------------------------------------------------
// Purpose: find all the different bones used in all the source files and map everything
//			to a common bonetable.
//-----------------------------------------------------------------------------
void RemapBones( void )
{
	g_real_numbones = g_numbones;
	g_real_numseq = g_numseq;
	g_real_numani = g_numani;

	if( g_staticprop )
	{
		MakeStaticProp( );
	}
	else if ( g_centerstaticprop )
	{
		MsgDev( D_WARN, "Ignoring option $autocenter. Only supported on $staticprop models!\n" );
	}

	TagUsedBones( );

	RenameBones( );

	BuildGlobalBonetable( );

	BuildGlobalBoneToPose( );

	EnforceHierarchy( );

	// tag parent bones as being in the same way as their children
	for( int k = 0; k < g_numbones; k++ )
	{
		int n = g_bonetable[k].parent;
		while( n != -1 )
		{
			g_bonetable[n].flags |= g_bonetable[k].flags;
			n = g_bonetable[n].parent;
		}
	}

	if( g_collapse_bones || g_numimportbones )
	{
		CollapseBones( );
	}

	if( g_numbones >= MAXSTUDIOBONES )
		COM_FatalError( "Too many bones used in model, used %d, max %d\n", g_numbones, MAXSTUDIOBONES );

	RebuildLocalPose( );

	TagProceduralBones( );

	MapSourcesToGlobalBonetable( );
}

//-----------------------------------------------------------------------------
// Purpose: convert source bone animation into global bone animation
//-----------------------------------------------------------------------------
void TranslateAnimations( const int boneGlobalToLocal[MAXSTUDIOSRCBONES], const matrix3x4 *srcBoneToWorld, matrix3x4 *destBoneToWorld )
{
	for( int k = 0; k < g_numbones; k++ )
	{
		int	q = boneGlobalToLocal[k];
		matrix3x4	bonematrix;

		if( q == -1 )
		{
			// unknown bone, copy over defaults
			if( g_bonetable[k].parent >= 0 )
			{
				bonematrix = matrix3x4( g_bonetable[k].pos, g_bonetable[k].rot );
				destBoneToWorld[k] = destBoneToWorld[g_bonetable[k].parent].ConcatTransforms( bonematrix );
			}
			else
			{
				destBoneToWorld[k] = matrix3x4( g_bonetable[k].pos, g_bonetable[k].rot );
			}
		}
		else
		{
			destBoneToWorld[k] = srcBoneToWorld[q].ConcatTransforms( g_bonetable[k].srcRealign );
		}
	}
}

void RemapVertices( void )
{
	int	i, j;

	for( i = 0; i < g_nummodels; i++ )
	{
		matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
		matrix3x4	destBoneToWorld[MAXSTUDIOSRCBONES];
		Vector	tmp1, tmp2, vdest, ndest;
		s_model_t	*pmodel = g_model[i];

		BuildRawTransforms( pmodel, g_vecZero, g_radZero, srcBoneToWorld );
		TranslateAnimations( pmodel->boneGlobalToLocal, srcBoneToWorld, destBoneToWorld );

		for( j = 0; j < pmodel->numsrcverts; j++ )
		{
			vdest.Init();
			ndest.Init();

			for( int n = 0; n < pmodel->srcvert[j].localWeight.numbones; n++ )
			{

				int	q = pmodel->srcvert[j].localWeight.bone[n];	// src bone
				int	k = pmodel->boneLocalToGlobal[q];		// mapping to global bone

				if( k == -1 )
				{
					vdest = pmodel->srcvert[j].vert;
					ndest = pmodel->srcvert[j].norm;
					break; // staticprop
				}

				for( int m = 0; m < pmodel->srcvert[j].globalWeight.numbones; m++ )
				{
					if( k == pmodel->srcvert[j].globalWeight.bone[m] )
					{
						// bone got collapsed out
						pmodel->srcvert[j].globalWeight.weight[m] += pmodel->srcvert[j].localWeight.weight[n];
						break;
					}
				}

				if( m == pmodel->srcvert[j].globalWeight.numbones )
				{
					// add new bone
					pmodel->srcvert[j].globalWeight.bone[m] = k;
					pmodel->srcvert[j].globalWeight.weight[m] = pmodel->srcvert[j].localWeight.weight[n];
					pmodel->srcvert[j].globalWeight.numbones++;
				}

				if( has_boneweights )
				{
					// convert vertex into original models' bone local space
					tmp1 = destBoneToWorld[k].VectorITransform( pmodel->srcvert[j].vert );
					// convert that into global world space using stardard pose
					tmp2 = g_bonetable[k].boneToPose.VectorTransform( tmp1 );
					// accumulate
					vdest += tmp2 * pmodel->srcvert[j].localWeight.weight[n];

					// convert vertex into original models' bone local space
					tmp1 = destBoneToWorld[k].VectorIRotate( pmodel->srcvert[j].norm );
					// convert that into global world space using stardard pose
					tmp2 = g_bonetable[k].boneToPose.VectorRotate( tmp1 );
					// accumulate
					ndest += tmp2 * pmodel->srcvert[j].localWeight.weight[n];
				}
				else
				{
					vdest = g_bonetable[k].boneToPose.VectorITransform( pmodel->srcvert[j].vert );
					ndest = g_bonetable[k].boneToPose.VectorIRotate( pmodel->srcvert[j].norm );
				}
			}

			pmodel->srcvert[j].vert = vdest;
			pmodel->srcvert[j].norm = ndest.Normalize();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: make indexed vertex and normal arrays
//-----------------------------------------------------------------------------
void BuildVertexArrays( void )
{
	for( int i = 0; i < g_nummodels; i++ )
	{
		s_model_t	*pmodel = g_model[i];

		for( int j = 0; j < pmodel->nummesh; j++ )
		{
			s_mesh_t	*pmesh = pmodel->pmesh[j];

			for( int k = 0; k < pmesh->numtris; k++ )
			{
				s_trianglevert_t	*ptriv = pmesh->triangle[k];

				for( int q = 0; q < 3; q++ )
				{
					ptriv[q].vertindex = LookupVertex( pmodel, &pmodel->srcvert[ptriv[q].vertindex] );
					ptriv[q].normindex = LookupNormal( pmodel, &pmodel->srcvert[ptriv[q].normindex] );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: convert source bone animation into global bone animation
//-----------------------------------------------------------------------------
void ConvertAnimation( s_animation_t const *panim, int frame, s_bone_t *dest )
{
	matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
	matrix3x4	destBoneToWorld[MAXSTUDIOSRCBONES];
	matrix3x4	destWorldToBone[MAXSTUDIOSRCBONES];
	matrix3x4	bonematrix;
	int	k;

	BuildRawTransforms( panim, frame, panim->adjust, panim->rotation, srcBoneToWorld );
	TranslateAnimations( panim->boneGlobalToLocal, srcBoneToWorld, destBoneToWorld );

	for( k = 0; k < g_numbones; k++ )
	{
		destWorldToBone[k] = destBoneToWorld[k].Invert();
	}

	// convert source_space_local_to_world transforms to shared_space_local_to_world transforms
	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].parent == -1 )
		{
			bonematrix = destBoneToWorld[k];
		}
		else
		{
			// convert my transform into parent relative space
			bonematrix = destWorldToBone[g_bonetable[k].parent].ConcatTransforms( destBoneToWorld[k] );
		}

		bonematrix.GetStudioTransform( dest[k].pos, dest[k].rot );
		clip_rotations( dest[k].rot );
	}
}

void RemapAnimations( void )
{
	int		size = g_numbones * sizeof( s_bone_t );
	s_animation_t	*panim;
	int		i, j, n;

	for( i = 0; i < g_numani; i++ )
	{
		panim = g_panimation[i];
		n = panim->startframe - panim->source.startframe;
		for( j = 0; j < panim->numframes; j++ )
		{
			panim->sanim[j] = (s_bone_t *)Mem_Alloc( size );
			ConvertAnimation( panim, n + j, panim->sanim[j] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: calculate the bone to world transforms for a processed animation
//-----------------------------------------------------------------------------
void CalcBoneTransforms( s_animation_t *panimation, s_animation_t *pbaseanimation, int frame, matrix3x4 *pBoneToWorld )
{
	if( FBitSet( panimation->flags, STUDIO_LOOPING ) && panimation->numframes > 1 )
	{
		while( frame >= ( panimation->numframes - 1 ))
			frame = frame - (panimation->numframes - 1);
	}

	if( frame < 0 || frame >= panimation->numframes )
	{
		COM_FatalError( "requested out of range frame on animation \"%s\" : %d (%d)\n", panimation->name, frame, panimation->numframes );
	}

	for( int k = 0; k < g_numbones; k++ )
	{
		matrix3x4	bonematrix;
		Vector	angle;

		if( !FBitSet( panimation->flags, STUDIO_DELTA ))
		{
			bonematrix = matrix3x4( panimation->sanim[frame][k].pos, panimation->sanim[frame][k].rot );
		}
		else if( pbaseanimation )
		{
			Vector4D	q1, q2, q3;
			Vector	p3;

			AngleQuaternion( pbaseanimation->sanim[0][k].rot, q1 );
			AngleQuaternion( panimation->sanim[frame][k].rot, q2 );

			float s = panimation->weight[k];

			QuaternionMA( q1, s, q2, q3 );
			p3 = pbaseanimation->sanim[0][k].pos + s * panimation->sanim[frame][k].pos;
			bonematrix = matrix3x4( p3, q3 );
		}
		else
		{
			Vector4D	q1, q2, q3;
			Vector	p3;

			AngleQuaternion( g_bonetable[k].rot, q1 );
			AngleQuaternion( panimation->sanim[frame][k].rot, q2 );

			float s = panimation->weight[k];

			QuaternionMA( q1, s, q2, q3 );
			p3 = pbaseanimation->sanim[0][k].pos + s * g_bonetable[k].pos;

			bonematrix = matrix3x4( p3, q3 );
		}

		if( g_bonetable[k].parent == -1 ) pBoneToWorld[k] = bonematrix;
		else pBoneToWorld[k] = pBoneToWorld[g_bonetable[k].parent].ConcatTransforms( bonematrix );
	}
}

void CalcBoneTransforms( s_animation_t *panimation, int frame, matrix3x4 *pBoneToWorld )
{
	CalcBoneTransforms( panimation, g_panimation[0], frame, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: calculate the bone to world transforms for a processed animation
//-----------------------------------------------------------------------------
void CalcBoneTransformsCycle( s_animation_t *panimation, s_animation_t *pbaseanimation, float flCycle, matrix3x4 *pBoneToWorld )
{
	float fFrame = flCycle * (panimation->numframes - 1);
	int iFrame = (int)fFrame;
	float s = (fFrame - iFrame);

	int iFrame1 = iFrame % (panimation->numframes - 1);
	int iFrame2 = (iFrame + 1) % (panimation->numframes - 1);

	for( int k = 0; k < g_numbones; k++ )
	{
		Vector4D	q1, q2, q3;
		matrix3x4	bonematrix;
		Vector	p3;

		AngleQuaternion( panimation->sanim[iFrame1][k].rot, q1 );
		AngleQuaternion( panimation->sanim[iFrame2][k].rot, q2 );
		QuaternionSlerp( q1, q2, s, q3 );

		VectorLerp( panimation->sanim[iFrame1][k].pos, s, panimation->sanim[iFrame2][k].pos, p3 );

		bonematrix = matrix3x4( p3, q3 );

		if( g_bonetable[k].parent == -1 ) pBoneToWorld[k] = bonematrix;
		else pBoneToWorld[k] = pBoneToWorld[g_bonetable[k].parent].ConcatTransforms( bonematrix );
	}
}

//-----------------------------------------------------------------------------
// Purpose: calculate the bone to world transforms for a processed sequence
//-----------------------------------------------------------------------------
void SlerpBones( Vector4D q1[MAXSTUDIOBONES], Vector pos1[MAXSTUDIOBONES], int sequence, const Vector4D q2[MAXSTUDIOBONES], const Vector pos2[MAXSTUDIOBONES], float s )
{
	int	i;
	Vector4D	q3, q4;
	float	s1, s2;

	s_sequence_t *pseqdesc = &g_sequence[sequence];

	if( s <= 0.0f ) 
	{
		return;
	}
	else if( s > 1.0f )
	{
		s = 1.0f;		
	}

	if( pseqdesc->flags & STUDIO_DELTA )
	{
		for( i = 0; i < g_numbones; i++ )
		{

			s2 = s * pseqdesc->weight[i];	// blend in based on this bones weight
			if( s2 > 0.0 )
			{
				if( pseqdesc->flags & STUDIO_POST )
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
	}
	else
	{
		for( i = 0; i < g_numbones; i++ )
		{
			s2 = s * pseqdesc->weight[i];	// blend in based on this animations weights

			if( s2 > 0.0f )
			{
				s1 = 1.0f - s2;

				if( g_bonetable[i].flags & BONE_FIXED_ALIGNMENT )
					QuaternionSlerpNoAlign( q2[i], q1[i], s1, q3 );
				else QuaternionSlerp( q2[i], q1[i], s1, q3 );

				pos1[i] = pos1[i] * s1 + pos2[i] * s2;
				q1[i] = q3;
			}
		}
	}
}

void CalcPoseSingle( Vector pos[], Vector4D q[], int sequence, float frame )
{
	s_sequence_t *pseqdesc = &g_sequence[sequence];
	s_animation_t *panim = pseqdesc->panim[0];

	// FIXME: is this modulo correct?
	int iframe = ((int)frame) % panim->numframes;

	for( int k = 0; k < g_numbones; k++ )
	{
		// FIXME: this isn't doing a fractional frame
		AngleQuaternion( panim->sanim[iframe][k].rot, q[k] );
		pos[k] = panim->sanim[iframe][k].pos;
	}
}

void AccumulatePose( Vector pos[], Vector4D q[], int sequence, float frame, float flWeight )
{
	Vector	pos2[MAXSTUDIOBONES];
	Vector4D	q2[MAXSTUDIOBONES];

	CalcPoseSingle( pos2, q2, sequence, frame );
	SlerpBones( q, pos, sequence, q2, pos2, flWeight );
	AccumulateSeqLayers( pos, q, sequence, frame, flWeight );
}

void AccumulateSeqLayers( Vector pos[], Vector4D q[], int sequence, float frame, float flWeight )
{
	s_sequence_t *pseqdesc = &g_sequence[sequence];

	for( int i = 0; i < pseqdesc->numautolayers; i++ )
	{
		s_autolayer_t *pLayer = &pseqdesc->autolayer[i];

		float layerFrame = frame;
		float layerWeight = flWeight;

		if( pLayer->start != pLayer->end )
		{
			float s = 1.0;
			float index;

			if( !FBitSet( pLayer->flags, STUDIO_AL_POSE ))
			{
				index = frame;
			}
			else
			{
				int iPose = pLayer->pose;
				if( iPose != -1 )
				{
					index = 0; // undefined?
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

			if( pLayer->flags & STUDIO_AL_SPLINE )
			{
				s = 3 * s * s - 2 * s * s * s;
			}

			if(( pLayer->flags & STUDIO_AL_XFADE ) && ( frame > pLayer->tail ))
			{
				layerWeight = ( s * flWeight ) / ( 1 - flWeight + s * flWeight );
			}
			else if( pLayer->flags & STUDIO_AL_NOBLEND )
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			if( !FBitSet( pLayer->flags, STUDIO_AL_POSE ))
			{
				layerFrame = ((frame - pLayer->start) / (pLayer->end - pLayer->start)) * (g_sequence[pLayer->sequence].panim[0]->numframes - 1);
			}
			else
			{
				layerFrame = (frame / g_sequence[sequence].panim[0]->numframes - 1) * (g_sequence[pLayer->sequence].panim[0]->numframes - 1);
			}
		}

		AccumulatePose( pos, q, pLayer->sequence, layerFrame, layerWeight );
	}
}

void CalcSeqTransforms( int sequence, int frame, matrix3x4 *pBoneToWorld )
{
	Vector	pos[MAXSTUDIOBONES];
	Vector4D	q[MAXSTUDIOBONES];
	int	k;

	for( k = 0; k < g_numbones; k++ )
	{
		AngleQuaternion( g_bonetable[k].rot, q[k] );
		pos[k] = g_bonetable[k].pos;
	}

	AccumulatePose( pos, q, sequence, frame, 1.0 );

	for( k = 0; k < g_numbones; k++ )
	{
		matrix3x4	bonematrix = matrix3x4( pos[k], q[k] );

		if( g_bonetable[k].parent == -1 ) pBoneToWorld[k] = bonematrix;
		else pBoneToWorld[k] = pBoneToWorld[g_bonetable[k].parent].ConcatTransforms( bonematrix );
	}
}

void buildAnimationWeights( void )
{
	int	i, j, k;

	// relink animation weights
	for( i = 0; i < g_numweightlist; i++ )
	{
		if( i == 0 )
		{
			// initialize weights
			for( j = 0; j < g_numbones; j++ )
			{
				if( g_bonetable[j].parent != -1 )
				{
					// set child bones to uninitialized
					g_weightlist[i].weight[j] = -1.0f;
				}
				else if( i == 0 )
				{
					// set root bones to 1
					g_weightlist[i].weight[j] = 1.0f;
					g_weightlist[i].posweight[j] = 1.0f;
				}
			}
		}
		else
		{
			// initialize weights
			for( j = 0; j < g_numbones; j++ )
			{
				if( g_bonetable[j].parent != -1 )
				{
					// set child bones to uninitialized
					g_weightlist[i].weight[j] = g_weightlist[0].weight[j];
					g_weightlist[i].posweight[j] = g_weightlist[0].posweight[j];
				}
				else
				{
					// set root bones to 0
					g_weightlist[i].weight[j] = 0.0f;
					g_weightlist[i].posweight[j] = 0.0f;
				}
			}
		}

		// match up weights
		for( j = 0; j < g_weightlist[i].numbones; j++ )
		{
			k = findGlobalBone( g_weightlist[i].bonename[j] );
			if( k == -1 )
			{
				COM_FatalError( "unknown bone reference '%s' in weightlist '%s'\n",
				g_weightlist[i].bonename[j], g_weightlist[i].name );
			}
			g_weightlist[i].weight[k] = g_weightlist[i].boneweight[j];
			g_weightlist[i].posweight[k] = g_weightlist[i].boneposweight[j];
		}
	}

	for( i = 0; i < g_numweightlist; i++ )
	{
		// copy weights forward
		for( j = 0; j < g_numbones; j++ )
		{
			if( g_weightlist[i].weight[j] < 0.0f )
			{
				if( g_bonetable[j].parent != -1 )
				{
					g_weightlist[i].weight[j] = g_weightlist[i].weight[g_bonetable[j].parent];
					g_weightlist[i].posweight[j] = g_weightlist[i].posweight[g_bonetable[j].parent];
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: subtract each frame running interpolation of the first frame to the last frame
//-----------------------------------------------------------------------------
void linearDelta( s_animation_t *psrc, s_animation_t *pdest, int srcframe, int flags )
{
 	// create delta animations
	s_bone_t	src0[MAXSTUDIOSRCBONES];
	s_bone_t	src1[MAXSTUDIOSRCBONES];
	int	j, k;

	for( k = 0; k < g_numbones; k++ )
	{
		src0[k].pos = psrc->sanim[0][k].pos;
		src0[k].rot = psrc->sanim[0][k].rot;
		src1[k].pos = psrc->sanim[srcframe][k].pos;
		src1[k].rot = psrc->sanim[srcframe][k].rot;
	}

	if( pdest->numframes == 1 )
	{
		MsgDev( D_WARN, "%s too short for splinedelta\n", pdest->name );
	}

	for( k = 0; k < g_numbones; k++ )
	{
		for( j = 0; j < pdest->numframes; j++ )
		{	
			float s = 1;
			if( pdest->numframes > 1 )
			{
				s = (float)j / (pdest->numframes - 1);
			}

			// make it a spline curve
			if( flags & STUDIO_AL_SPLINE )
			{
				s = 3 * s * s - 2 * s * s * s;
			}

			if( pdest->weight[k] > 0.0f )
			{
				s_bone_t	src;

				src.pos = src0[k].pos * (1 - s) + src1[k].pos * s;
				QuaternionSlerp( src0[k].rot, src1[k].rot, s, src.rot );

				// calc differences between two rotations
				if( flags & STUDIO_AL_POST )
				{
					// find pdest in src's reference frame  
					QuaternionSMAngles( -1.0f, src.rot, pdest->sanim[j][k].rot, pdest->sanim[j][k].rot );
					pdest->sanim[j][k].pos = pdest->sanim[j][k].pos - src.pos;
				}
				else
				{
					// find src in pdest's reference frame?
					QuaternionMAAngles( pdest->sanim[j][k].rot, -1.0f, src.rot, pdest->sanim[j][k].rot );
					pdest->sanim[j][k].pos = src.pos - pdest->sanim[j][k].pos;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: turn the animation into a lower fps encoded version
//-----------------------------------------------------------------------------
void reencodeAnimation( s_animation_t *panim, int frameskip )
{
	int	j, k, n;

	for( n = 1, j = frameskip; j < panim->numframes; j += frameskip )
	{	
		for( k = 0; k < g_numbones; k++ )
		{
			panim->sanim[n][k] = panim->sanim[j][k];
		}
		n++;
	}

	panim->numframes = n;
	panim->fps = panim->fps / frameskip;
}

//-----------------------------------------------------------------------------
// Purpose: clip or pad the animation as nessesary to be a specified number of frames
//-----------------------------------------------------------------------------
void forceNumframes( s_animation_t *panim, int numframes )
{
	int size = g_numbones * sizeof( s_bone_t );

	// copy
	for( int j = panim->numframes; j < numframes; j++ )
	{	
		panim->sanim[j] = (s_bone_t *)Mem_Alloc( size );
		memcpy( panim->sanim[j], panim->sanim[panim->numframes-1], size );
	}
	panim->numframes = numframes;
}

//-----------------------------------------------------------------------------
// Purpose: subtract each frame from the previous to calculate the animations derivative
//-----------------------------------------------------------------------------
void createDerivative( s_animation_t *panim, float scale )
{
	s_bone_t	orig[MAXSTUDIOSRCBONES];
	int	j, k;

	j = panim->numframes - 1;

	if( FBitSet( panim->flags, STUDIO_LOOPING ))
		j--;

	for( k = 0; k < g_numbones; k++ )
	{
		orig[k].pos = panim->sanim[j][k].pos;
		orig[k].rot = panim->sanim[j][k].rot;
	}

	for( j = panim->numframes - 1; j >= 0; j-- )
	{	
		s_bone_t	*psrc, *pdest;
		Vector4D	q;

		if( j - 1 >= 0 )
			psrc = panim->sanim[j-1];
		else psrc = orig;
		pdest = panim->sanim[j];

		for( k = 0; k < g_numbones; k++ )
		{
			if( panim->weight[k] > 0.0f )
			{
				// find pdest in src's reference frame  
				QuaternionSMAngles( -1, psrc[k].rot, pdest[k].rot, pdest[k].rot );
				pdest[k].pos = pdest[k].pos - psrc[k].pos;

				// rescale results (not sure what basis physics system is expecting)
				AngleQuaternion( pdest[k].rot, q );
				QuaternionScale( q, scale, q );
				QuaternionAngle( q, pdest[k].rot );
				pdest[k].pos *= scale;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: subtract each frame from the previous to calculate the animations derivative
//-----------------------------------------------------------------------------
void clearAnimations( s_animation_t *panim )
{
	panim->flags |= STUDIO_DELTA;
	panim->flags |= STUDIO_ALLZEROS;
	
	panim->numframes = 1;
	panim->startframe = 0;
	panim->endframe = 1;

	for( int k = 0; k < g_numbones; k++ )
	{
		panim->sanim[0][k].pos = Vector( 0, 0, 0 );
		panim->sanim[0][k].rot = Radian( 0, 0, 0 );
		panim->posweight[k] = 0.0f;
		panim->weight[k] = 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: remove all world rotation from a bone
//-----------------------------------------------------------------------------
void counterRotateBone( s_animation_t *panim, int iBone, Vector target )
{
	matrix3x4	boneToWorld[MAXSTUDIOBONES];
	matrix3x4	defaultBoneToWorld;
	Vector	pos;

	defaultBoneToWorld = matrix3x4( g_vecZero, target );

	for( int j = 0; j < panim->numframes; j++ )
	{
		CalcBoneTransforms( panim, j, boneToWorld );

		boneToWorld[iBone].GetOrigin( pos );
		defaultBoneToWorld.SetOrigin( pos );
		boneToWorld[iBone] = defaultBoneToWorld;
		solveBone( panim, j, iBone, boneToWorld );
	}
}

void setAnimationWeight( s_animation_t *panim, int index )
{
	// copy weightlists to animations
	for( int k = 0; k < g_numbones; k++ )
	{
		panim->weight[k] = g_weightlist[index].weight[k];
		panim->posweight[k] = g_weightlist[index].posweight[k];
	}
}

void addDeltas( s_animation_t *panim, int frame, float s, Vector delta_pos[], Vector4D delta_q[] )
{
	for( int k = 0; k < g_numbones; k++ )
	{
		if( panim->weight[k] > 0.0f )
		{
			QuaternionSMAngles( s, delta_q[k], panim->sanim[frame][k].rot, panim->sanim[frame][k].rot );
			panim->sanim[frame][k].pos += delta_pos[k] * s;
		}
	}
}

void extractUnusedMotion( s_animation_t *panim )
{
	float	motion[6];
	int	type, j, k;

	type = panim->motiontype;

	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].parent == -1 )
		{
			motion[0] = panim->sanim[0][k].pos.x;
			motion[1] = panim->sanim[0][k].pos.y;
			motion[2] = panim->sanim[0][k].pos.z;
			motion[3] = panim->sanim[0][k].rot.x;
			motion[4] = panim->sanim[0][k].rot.y;
			motion[5] = panim->sanim[0][k].rot.z;

			for( j = 0; j < panim->numframes; j++ )
			{	
				if( type & STUDIO_X )
					panim->sanim[j][k].pos.x = motion[0];
				if( type & STUDIO_Y )
					panim->sanim[j][k].pos.y = motion[1];
				if( type & STUDIO_Z )
					panim->sanim[j][k].pos.z = motion[2];
				if( type & STUDIO_XR )
					panim->sanim[j][k].rot.x = motion[3];
				if( type & STUDIO_YR )
					panim->sanim[j][k].rot.y = motion[4];
				if( type & STUDIO_ZR )
					panim->sanim[j][k].rot.z = motion[5];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: find the difference between the src and dest animations, then add that
//			difference to all the frames of the dest animation.
//-----------------------------------------------------------------------------
void processMatch( s_animation_t *psrc, s_animation_t *pdest, int flags )
{
	// process "match"
	Vector	delta_pos[MAXSTUDIOSRCBONES];
	Vector4D	delta_q[MAXSTUDIOSRCBONES];
	int	j, k;

	for( k = 0; k < g_numbones; k++ )
	{
		if( flags ) delta_pos[k] = psrc->sanim[0][k].pos - pdest->sanim[0][k].pos;
		QuaternionSM( -1.0f, pdest->sanim[0][k].rot, psrc->sanim[0][k].rot, delta_q[k] );
	}

	for( j = 0; j < pdest->numframes; j++ )
	{
		for( k = 0; k < g_numbones; k++ )
		{
			if( pdest->weight[k] > 0.0f )
			{
				if( flags ) pdest->sanim[j][k].pos += delta_pos[k];
				QuaternionMAAngles( pdest->sanim[j][k].rot, 1.0f, delta_q[k], pdest->sanim[j][k].rot );
			}
		}	
	}
}


//-----------------------------------------------------------------------------
// Purpose: blend the psrc animation overtop the pdest animation, but blend the 
//			quaternions in world space instead of parent bone space.
//			Also, blend bone lengths, but only for non root animations.
//-----------------------------------------------------------------------------
void worldspaceBlend( s_animation_t *psrc, s_animation_t *pdest, int srcframe, int flags )
{
	int	j, k, n;

	// process "match"
	Vector4D	srcQ[MAXSTUDIOSRCBONES];
	Vector	srcPos[MAXSTUDIOSRCBONES];
	matrix3x4	srcBoneToWorld[MAXSTUDIOBONES];
	matrix3x4	destBoneToWorld[MAXSTUDIOBONES];
	Vector	tmp;

	if( !flags )
	{
		CalcBoneTransforms( psrc, srcframe, srcBoneToWorld );
		for( k = 0; k < g_numbones; k++ )
		{
			srcQ[k] = srcBoneToWorld[k].GetQuaternion();
			srcPos[k] = psrc->sanim[srcframe][k].pos;
		}
	}

	Vector4D	targetQ, destQ;

	for( j = 0; j < pdest->numframes; j++ )
	{
		if( flags )
		{
			// pull from a looping source
			float flCycle = (float)j / (pdest->numframes - 1);
			flCycle += (float)srcframe / (psrc->numframes - 1);
			CalcBoneTransformsCycle( psrc, psrc, flCycle, srcBoneToWorld );

			for( k = 0; k < g_numbones; k++ )
			{
				srcQ[k] = srcBoneToWorld[k].GetQuaternion();

				n = g_bonetable[k].parent;
				if( n == -1 )
				{
					srcPos[k] = srcBoneToWorld[k].GetOrigin();
				}
				else
				{
					matrix3x4 worldToBone = srcBoneToWorld[n].Invert();
					matrix3x4 local = worldToBone.ConcatTransforms( srcBoneToWorld[k] );
					srcPos[k] = local.GetOrigin();
				}
			}
		}


		CalcBoneTransforms( pdest, j, destBoneToWorld );

		for( k = 0; k < g_numbones; k++ )
		{
			if( pdest->weight[k] > 0 )
			{
				// blend the boneToWorld transforms in world space
				destQ = destBoneToWorld[k].GetQuaternion();
				tmp = destBoneToWorld[k].GetOrigin();

				QuaternionSlerp( destQ, srcQ[k], pdest->weight[k], targetQ );
				destBoneToWorld[k] = matrix3x4( tmp, targetQ );
			}

			// back solve
			n = g_bonetable[k].parent;
			if( n == -1 )
			{
				destBoneToWorld[k].GetAngles( pdest->sanim[j][k].rot );

				// FIXME: it's not clear if this should blend position or not....it'd be 
				// better if weight lists could do quat and pos independently. 
			}
			else
			{
				matrix3x4	worldToBone = destBoneToWorld[n].Invert();
				matrix3x4 local = worldToBone.ConcatTransforms( destBoneToWorld[k] );
				local.GetAngles( pdest->sanim[j][k].rot );

				// blend bone lengths (local space)
				VectorLerp( pdest->sanim[j][k].pos, pdest->posweight[k], srcPos[k], pdest->sanim[j][k].pos );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: match one animations position/orientation to another animations position/orientation
//-----------------------------------------------------------------------------
void processAutoorigin( s_animation_t *psrc, s_animation_t *pdest, int motiontype, int srcframe, int destframe, int bone )
{	
	matrix3x4	srcBoneToWorld[MAXSTUDIOBONES];
	matrix3x4	destBoneToWorld[MAXSTUDIOBONES];
	matrix3x4	adjmatrix;
	int	j, k;

	CalcBoneTransforms( psrc, srcframe, srcBoneToWorld );
	CalcBoneTransforms( pdest, destframe, destBoneToWorld );

	// find rotation
	Radian	rot( 0, 0, 0 );
	Vector	srcPos = srcBoneToWorld[bone].GetOrigin();
	Vector	destPos = destBoneToWorld[bone].GetOrigin();
	Vector4D	q0 = srcBoneToWorld[bone].GetQuaternion();
	Vector4D	q2 = destBoneToWorld[bone].GetQuaternion();

	if( FBitSet( motiontype, STUDIO_LXR | STUDIO_LYR | STUDIO_LZR | STUDIO_XR | STUDIO_YR | STUDIO_ZR ))
	{
		Vector4D	deltaQ2;
		Vector4D	q4;
		Radian	a3;

		QuaternionMA( q2, -1.0f, q0, deltaQ2 );

		if( FBitSet( motiontype, STUDIO_LXR | STUDIO_XR ))
		{
			q4.Init( deltaQ2.x, 0, 0, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );
			rot.x = a3.x;
		}

		if( FBitSet( motiontype, STUDIO_LYR | STUDIO_YR ))
		{
			q4.Init( 0, deltaQ2.y, 0, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );
			rot.y = a3.y;
		}

		if( FBitSet( motiontype, STUDIO_LZR | STUDIO_ZR ))
		{
			q4.Init( 0, 0, deltaQ2.z, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );
			rot.z = a3.z;
		}

		if( FBitSet( motiontype, STUDIO_XR ) && FBitSet( motiontype, STUDIO_YR ) && FBitSet( motiontype, STUDIO_ZR ))
		{
			QuaternionAngle( deltaQ2, rot );
		}
	}

	// find movement
	Vector p0 = srcPos;
	adjmatrix = matrix3x4( g_vecZero, rot ).Invert();
	Vector p2 = adjmatrix.VectorRotate( destPos );

	Vector adj = p0 - p2;

	if( !FBitSet( motiontype, STUDIO_X | STUDIO_LX ))
		adj.x = 0;
	if( !FBitSet( motiontype, STUDIO_Y | STUDIO_LY ))
		adj.y = 0;
	if( !FBitSet( motiontype, STUDIO_Z | STUDIO_LZ ))
		adj.z = 0;

	adjmatrix.SetOrigin( adj );

	if( bone != g_rootIndex )
	{
		MsgDev( D_REPORT, "%s aligning to %s - %.2f %.2f %.2f\n", pdest->name, g_bonetable[bone].name, adj.x, adj.y, adj.z );
	}

	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].parent == -1 )
		{
			for( j = 0; j < pdest->numframes; j++ )
			{
				matrix3x4	bonematrix = matrix3x4( pdest->sanim[j][k].pos, pdest->sanim[j][k].rot );
				bonematrix = adjmatrix.ConcatTransforms( bonematrix );
				bonematrix.GetStudioTransform( pdest->sanim[j][k].pos, pdest->sanim[j][k].rot );
			}
		}
	}	
}

//-----------------------------------------------------------------------------
// Purpose: subtract one animaiton from animation to create an animation of the "difference"
//-----------------------------------------------------------------------------
void subtractBaseAnimations( s_animation_t *psrc, s_animation_t *pdest, int srcframe, int flags )
{
	int	j, k;

 	// create delta animations
	s_bone_t	src[MAXSTUDIOSRCBONES];

	if( srcframe >= psrc->numframes )
	{
		COM_FatalError( "subtract frame %d out of range for %s\n", srcframe, psrc->name );
	}

	for( k = 0; k < g_numbones; k++ )
	{
		src[k].pos = psrc->sanim[srcframe][k].pos;
		src[k].rot = psrc->sanim[srcframe][k].rot;
	}

	for( k = 0; k < g_numbones; k++ )
	{
		for( j = 0; j < pdest->numframes; j++ )
		{	
			if( pdest->weight[k] > 0.0f )
			{
				// calc differences between two rotations
				if( FBitSet( flags, STUDIO_POST ))
				{
					// find pdest in src's reference frame  
					QuaternionSMAngles( -1.0f, src[k].rot, pdest->sanim[j][k].rot, pdest->sanim[j][k].rot );
					pdest->sanim[j][k].pos = pdest->sanim[j][k].pos - src[k].pos;
				}
				else
				{
					// find src in pdest's reference frame?
					QuaternionMAAngles( pdest->sanim[j][k].rot, -1.0f, src[k].rot, pdest->sanim[j][k].rot );
					pdest->sanim[j][k].pos = src[k].pos - pdest->sanim[j][k].pos;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: rotate the animation so that it's moving in the specified angle
//-----------------------------------------------------------------------------
void makeAngle( s_animation_t *panim, float angle )
{
	float	da = 0.0f;

	if( panim->numpiecewisekeys != 0 )
	{
		// look for movement in total piecewise movement
		Vector	pos = panim->piecewisemove[panim->numpiecewisekeys-1].pos;

		if( pos[0] != 0 || pos[1] != 0 )
		{
			float a = atan2( pos[1], pos[0] ) * (180 / M_PI);
			da = angle - a;
		}

		for( int i = 0; i < panim->numpiecewisekeys; i++ )
		{
			panim->piecewisemove[i].pos = VectorYawRotate( panim->piecewisemove[i].pos, da );
			panim->piecewisemove[i].vector = VectorYawRotate( panim->piecewisemove[i].vector, da );
		}
	}
	else
	{
		// look for movement in root bone
		Vector pos = panim->sanim[(panim->numframes - 1)][g_rootIndex].pos - panim->sanim[0][g_rootIndex].pos;

		if( pos[0] != 0 || pos[1] != 0 )
		{
			float a = atan2( pos[1], pos[0] ) * (180 / M_PI);
			da = angle - a;
		}
	}

	matrix3x4	rootxform;
	matrix3x4	src, dest;

	rootxform = matrix3x4( g_vecZero, Vector( 0.0f, da, 0.0f ));

	for( int j = 0; j < panim->numframes; j++ )
	{
		for( int k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].parent == -1 )
			{
				src = matrix3x4( panim->sanim[j][k].pos, panim->sanim[j][k].rot );
				dest = rootxform.ConcatTransforms( src );
				dest.GetStudioTransform( panim->sanim[j][k].pos, panim->sanim[j][k].rot );
			}
		}
	}

	// FIXME: not finished
}

//-----------------------------------------------------------------------------
// Purpose: find the difference between the overlapping frames and spread out
//			the difference over multiple frames.
// start:	negative number, specifies how far back from the end to start blending
// end:		positive number, specifies how many frames from the beginning to blend
//-----------------------------------------------------------------------------
void fixupLoopingDiscontinuities( s_animation_t *panim, int start, int end )
{
	int	j, k, m, n;

	// fix C0 errors on looping animations
	m = panim->numframes - 1;

	Vector	delta_pos[MAXSTUDIOSRCBONES];
	Vector4D	delta_q[MAXSTUDIOSRCBONES];

	// skip if there's nothing to smooth
	if( m == 0 ) return;

	for( k = 0; k < g_numbones; k++ )
	{
		delta_pos[k] = panim->sanim[m][k].pos - panim->sanim[0][k].pos;
		QuaternionMA( panim->sanim[m][k].rot, -1, panim->sanim[0][k].rot, delta_q[k] );
	}

	// HACK: skip fixup for motion that'll be matched with linear extraction
	// FIXME: remove when "global" extraction moved into normal ordered processing loop
	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].parent == -1 )
		{
			if( FBitSet( panim->motiontype, STUDIO_LX ))
				delta_pos[k].x = 0.0f;
			if( FBitSet( panim->motiontype, STUDIO_LY ))
				delta_pos[k].y = 0.0f;
			if( FBitSet( panim->motiontype, STUDIO_LZ ))
				delta_pos[k].z = 0.0f;
			// FIXME: add rotation
		}
	}

	// make sure loop doesn't exceed animation length
	if(( end - start ) > panim->numframes )
	{
		end = panim->numframes + start;
		if( end < 0 )
		{
			end = 0;
			start = -(panim->numframes - 1);
		}
	}

	// FIXME: figure out S
	float s = 0;
	float nf = end - start;
	
	for( j = start + 1; j <= 0; j++ )
	{	
		n = j - start;
		s = (n / nf);
		s = 3 * s * s - 2 * s * s * s;
		addDeltas( panim, m+j, -s, delta_pos, delta_q );
	}

	for( j = 0; j < end; j++ )
	{	
		n = end - j;
		s = (n / nf);
		s = 3 * s * s - 2 * s * s * s;
		addDeltas( panim, j, s, delta_pos, delta_q );
	}
}

void matchBlend( s_animation_t *pDestAnim, s_animation_t *pSrcAnimation, int iSrcFrame, int iDestFrame, int iPre, int iPost )
{
	int	j, k;

	if( FBitSet( pDestAnim->flags, STUDIO_LOOPING ))
	{
		iPre = Q_max( iPre, -pDestAnim->numframes );
		iPost = Q_min( iPost, pDestAnim->numframes );
	}
	else
	{
		iPre = Q_max( iPre, -iDestFrame );
		iPost = Q_min( iPost, pDestAnim->numframes - iDestFrame );
	}

	Vector	delta_pos[MAXSTUDIOSRCBONES];
	Vector4D	delta_q[MAXSTUDIOSRCBONES];

	for( k = 0; k < g_numbones; k++ )
	{
		delta_pos[k] = pSrcAnimation->sanim[iSrcFrame][k].pos - pDestAnim->sanim[iDestFrame][k].pos;
		QuaternionMA( pSrcAnimation->sanim[iSrcFrame][k].rot, -1, pDestAnim->sanim[iDestFrame][k].rot, delta_q[k] );
	}

	// HACK: skip fixup for motion that'll be matched with linear extraction
	// FIXME: remove when "global" extraction moved into normal ordered processing loop
	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].parent == -1 )
		{
			if( FBitSet( pDestAnim->motiontype, STUDIO_LX ))
				delta_pos[k].x = 0.0f;
			if( FBitSet( pDestAnim->motiontype, STUDIO_LY ))
				delta_pos[k].y = 0.0f;
			if( FBitSet( pDestAnim->motiontype, STUDIO_LZ ))
				delta_pos[k].z = 0.0f;
			// FIXME: add rotation
		}
	}

	// FIXME: figure out S
	float s = 0;

	for( j = iPre; j <= iPost; j++ )
	{	
		if( j < 0 )
		{
			s = j / (float)(iPre - 1);
		}
		else
		{
			s = j / (float)(iPost + 1);
		}

		s = SimpleSpline( 1 - s );
		k = iDestFrame + j;

		if( k < 0 )
		{
			k += (pDestAnim->numframes - 1);
		}
		else
		{
			k = k % (pDestAnim->numframes - 1);
		}

		addDeltas( pDestAnim, k, s, delta_pos, delta_q );

		// make sure final frame of a looping animation matches frame 0
		if( FBitSet( pDestAnim->flags, STUDIO_LOOPING ) && k == 0 )
		{
			addDeltas( pDestAnim, pDestAnim->numframes - 1, s, delta_pos, delta_q );
		}
	}
}

void forceAnimationLoop( s_animation_t *panim )
{
	// force looping animations to be looping
	if( FBitSet( panim->flags, STUDIO_LOOPING ))
	{
		int	n = 0;
		int	m = panim->numframes - 1;

		for( int k = 0; k < g_numbones; k++ )
		{
			int	type = panim->motiontype;

			if( !FBitSet( type, STUDIO_LX ))
				panim->sanim[m][k].pos[0] = panim->sanim[n][k].pos[0];
			if( !FBitSet( type, STUDIO_LY ))
				panim->sanim[m][k].pos[1] = panim->sanim[n][k].pos[1];
			if( !FBitSet( type, STUDIO_LZ ))
				panim->sanim[m][k].pos[2] = panim->sanim[n][k].pos[2];

			if( !FBitSet( type, STUDIO_LXR ))
				panim->sanim[m][k].rot[0] = panim->sanim[n][k].rot[0];
			if( !FBitSet( type, STUDIO_LYR ))
				panim->sanim[m][k].rot[1] = panim->sanim[n][k].rot[1];
			if( !FBitSet( type, STUDIO_LZR ))
				panim->sanim[m][k].rot[2] = panim->sanim[n][k].rot[2];
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: find the linear movement/rotation between two frames, subtract that 
//	  out of the animation and add it back on as a "piecewise movement" command
// panim		- current animation
// motiontype	- what to extract
// iStartFrame	- first frame to apply motion over
// iEndFrame	- last end frame to apply motion over
// iSrcFrame	- match refFrame against what frame of the current animation
// pRefAnim	- reference animtion
// iRefFrame	- frame of reference animation to match
//-----------------------------------------------------------------------------
void extractLinearMotion( s_animation_t *panim, int motiontype, int iStartFrame, int iEndFrame, int iSrcFrame, s_animation_t *pRefAnim, int iRefFrame )
{
	matrix3x4	adjmatrix;
	int	j, k;

	// Can't extract motion with only 1 frame of animation!
	if( panim->numframes <= 1 )
	{
		COM_FatalError( "Can't extract motion from sequence %s (%s).  Check your QC options!\n", panim->name, panim->filename );
	}

	if( panim->numpiecewisekeys >= MAXSTUDIOMOVEKEYS )
	{
		COM_FatalError( "Too many piecewise movement keys in %s (%s)\n", panim->name, panim->filename );
	}

	if( iEndFrame > panim->numframes - 1 )
		iEndFrame = panim->numframes - 1;

	if( iSrcFrame > panim->numframes - 1 )
		iSrcFrame = panim->numframes - 1;

	if( iStartFrame >= iEndFrame )
	{
		MsgDev( D_WARN, "Motion extraction ignored, no frames remaining in %s (%s)\n", panim->name, panim->filename );
		return;
	}

	float fFrame = (iStartFrame + iSrcFrame) / 2.0f;
	int iMidFrame = (int)fFrame;
	float s = fFrame - iMidFrame;

	// find rotation
	Radian	rot( 0, 0, 0 );

	if( FBitSet( motiontype, STUDIO_LXR | STUDIO_LYR | STUDIO_LZR ))
	{
		Vector4D	q0, q1, q2, q4, q5;

		AngleQuaternion( pRefAnim->sanim[iRefFrame][g_rootIndex].rot, q0 );
		AngleQuaternion( panim->sanim[iMidFrame][g_rootIndex].rot, q1 ); // only used for rotation checking
		AngleQuaternion( panim->sanim[iSrcFrame][g_rootIndex].rot, q2 );

		Vector4D	deltaQ1, deltaQ2;

		QuaternionMA( q1, -1.0f, q0, deltaQ1 );
		QuaternionMA( q2, -1.0f, q0, deltaQ2 );

		// FIXME: this is still wrong, but it should be slightly more robust
		Radian	a3, a5;

		if( FBitSet( motiontype, STUDIO_LXR ))
		{
			q4.Init( deltaQ2.x, 0, 0, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );
			rot.x = a3.x;
		}

		if( FBitSet( motiontype, STUDIO_LYR ))
		{
			q4.Init( 0, deltaQ2.y, 0, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );
			rot.y = a3.y;
		}

		if( FBitSet( motiontype, STUDIO_LZR ))
		{
			q4.Init( 0, 0, deltaQ2.z, deltaQ2.w );
			q4 = q4.Normalize();
			QuaternionAngle( q4, a3 );

			// check for possible rotations >180 degrees by looking at the 
			// halfway point and seeing if it's rotating a different direction
			// than the shortest path to the end point
			Radian a5;
			q5.Init( 0, 0, deltaQ1.z, deltaQ1.w );
			q5 = q5.Normalize();
			QuaternionAngle( q5, a5 );
			if( a3.z > M_PI ) a5.z -= 2 * M_PI;
			if( a3.z < -M_PI ) a5.z += 2 * M_PI;
			if( a5.z > M_PI ) a5.z -= 2 * M_PI;
			if( a5.z < -M_PI ) a5.z += 2 * M_PI;
			if( a5.z > M_PI / 4 && a3.z < 0 )
				a3.z += 2 * M_PI;
			if( a5.z < -M_PI / 4 && a3.z > 0 )
				a3.z -= 2*M_PI;
			rot.z = a3.z;
		}
	}

	// find movement
	Vector p0;

	adjmatrix = matrix3x4( g_vecZero, rot );
	p0 = adjmatrix.VectorRotate( pRefAnim->sanim[iRefFrame][g_rootIndex].pos );

	Vector p2 = panim->sanim[iSrcFrame][g_rootIndex].pos;
	Vector p1 = panim->sanim[iMidFrame][g_rootIndex].pos * (1.0f - s) + panim->sanim[iMidFrame+1][g_rootIndex].pos * s;

	p2 = p2 - p0;
	p1 = p1 - p0;

	if( !FBitSet( motiontype, STUDIO_LX ))
	{
		p2.x = 0;
		p1.x = 0;
	}

	if( !FBitSet( motiontype, STUDIO_LY ))
	{
		p2.y = 0;
		p1.y = 0;
	}

	if( !FBitSet( motiontype, STUDIO_LZ ))
	{
		p2.z = 0;
		p1.z = 0;
	}

	float d1 = p1.Length();
	float d2 = p2.Length();

	float v0 = -1.0f * d2 + 4 * d1;
	float v1 = 3 * d2 - 4 * d1;

	MsgDev( D_REPORT, "%s : %d - %d : %.1f %.1f %.1f\n", panim->name, iStartFrame, iEndFrame, p2.x, p2.y, RAD2DEG( rot[2] ));

	int numframes = iEndFrame - iStartFrame + 1;
	if( numframes < 1 ) return;

	float n = numframes - 1;

	if( FBitSet( motiontype, STUDIO_LINEAR ))
	{
		v0 = v1 = p2.Length();
	}
	else if( v0 < 0.0f )
	{
		v0 = 0.0;
		v1 = p2.Length() * 2.0f;
	}
	else if( v1 < 0.0f )
	{
		v0 = p2.Length() * 2.0f;
		v1 = 0.0;
	}
	else if(( v0 + v1 ) > 0.01f && (fabs( v0 - v1 ) / ( v0 + v1 )) < 0.2f )
	{
		// if they're within 10% of each other, assume no acceleration
		v0 = v1 = p2.Length();
	}

	Vector v = p2.Normalize();
	Vector A, B, C;

	if( FBitSet( motiontype, STUDIO_QUADRATIC_MOTION ))
	{
		SolveInverseQuadratic( 0, 0, 0.5, p1.x, 1.0, p2.x, A.x, B.x, C.x );
		SolveInverseQuadratic( 0, 0, 0.5, p1.y, 1.0, p2.y, A.y, B.y, C.y );
		SolveInverseQuadratic( 0, 0, 0.5, p1.z, 1.0, p2.z, A.z, B.z, C.z );
	}

	Vector	adjpos;
	Radian	adjangle;
	matrix3x4	bonematrix;

	for( j = 0; j < numframes; j++ )
	{	
		float	t = (j / n);

		if( FBitSet( motiontype, STUDIO_QUADRATIC_MOTION ))
		{
			adjpos.x = t * t * A.x + t * B.x + C.x;
			adjpos.y = t * t * A.y + t * B.y + C.y;
			adjpos.z = t * t * A.z + t * B.z + C.z;
		}
		else
		{
			adjpos = v * ( v0 * t + 0.5f * (v1 - v0) * t * t );
		}

		adjangle = rot * t;
		adjmatrix = matrix3x4( adjpos, adjangle ).Invert();

		for( k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].parent == -1 )
			{
				bonematrix = matrix3x4( panim->sanim[j+iStartFrame][k].pos, panim->sanim[j+iStartFrame][k].rot );
				bonematrix = adjmatrix.ConcatTransforms( bonematrix );
				bonematrix.GetStudioTransform( panim->sanim[j+iStartFrame][k].pos, panim->sanim[j+iStartFrame][k].rot );
			}
		}
	}

	// use adjmatrix form last frame
	for( ; j + iStartFrame < panim->numframes; j++ )
	{
		for( k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].parent == -1 )
			{
				bonematrix = matrix3x4( panim->sanim[j+iStartFrame][k].pos, panim->sanim[j+iStartFrame][k].rot );
				bonematrix = adjmatrix.ConcatTransforms( bonematrix );
				bonematrix.GetStudioTransform( panim->sanim[j+iStartFrame][k].pos, panim->sanim[j+iStartFrame][k].rot );
			}
		}
	}

	// create piecewise motion paths
	s_linearmove_t *pmove = &panim->piecewisemove[panim->numpiecewisekeys++];

	pmove->endframe = iEndFrame;
	pmove->flags = motiontype;

	// concatinate xforms
	if( panim->numpiecewisekeys > 1 )
	{
		bonematrix = matrix3x4( adjpos, adjangle );
		adjmatrix = matrix3x4( pmove[-1].pos, pmove[-1].rot );
		bonematrix = adjmatrix.ConcatTransforms( bonematrix );
		bonematrix.GetStudioTransform( pmove[0].pos, pmove[0].rot );
		pmove->vector = pmove[0].pos - pmove[-1].pos;
	}
	else
	{
		VectorCopy( adjpos, pmove[0].pos );
		VectorCopy( adjangle, pmove[0].rot );
		pmove->vector = pmove[0].pos;
	}

	pmove->vector = pmove->vector.Normalize();
	pmove->v0 = v0;
	pmove->v1 = v1;

	if( iStartFrame == 0 && iSrcFrame == ( panim->numframes - 1 ))
		panim->linearmovement = pmove[0].pos; // for goldsource movement
}

//-----------------------------------------------------------------------------
// Purpose: process the "piecewise movement" commands and return where the animation
//			would move to on a given frame (assuming frame 0 is at the origin)
//-----------------------------------------------------------------------------
Vector calcPosition( s_animation_t *panim, int iFrame )
{
	Vector vecPos = g_vecZero;

	if( panim->numpiecewisekeys == 0 )
		return vecPos;

	if( panim->numframes == 1 )
		return vecPos;

	int iLoops = 0;
	while( iFrame >= ( panim->numframes - 1 ))
	{
		iLoops++;
		iFrame = iFrame - (panim->numframes - 1);
	}

	float prevframe = 0.0f;

	for( int i = 0; i < panim->numpiecewisekeys; i++ )
	{
		s_linearmove_t *pmove = &panim->piecewisemove[i];

		if( pmove->endframe >= iFrame )
		{
			float f = (iFrame - prevframe) / (pmove->endframe - prevframe);
			float d = pmove->v0 * f + 0.5 * (pmove->v1 - pmove->v0) * f * f;

			vecPos = vecPos + d * pmove->vector;

			if( iLoops != 0 )
			{
				s_linearmove_t *pmove = &panim->piecewisemove[panim->numpiecewisekeys - 1];
				vecPos = vecPos + iLoops * pmove->pos; 
			}
			return vecPos;
		}
		else
		{
			prevframe = pmove->endframe;
			vecPos = pmove->pos;
		}
	}

	return vecPos;
}

//-----------------------------------------------------------------------------
// Purpose: calculate how far an animation travels between two frames
//-----------------------------------------------------------------------------
Vector calcMovement( s_animation_t *panim, int iFrom, int iTo )
{
	Vector p1 = calcPosition( panim, iFrom );
	Vector p2 = calcPosition( panim, iTo );

	return p2 - p1;
}

//-----------------------------------------------------------------------------
// Purpose: try to calculate a "missing" frame of animation, i.e the overlapping frame
//-----------------------------------------------------------------------------
void fixupMissingFrame( s_animation_t *panim )
{
	// the animations DIDN'T have the end frame the same as the start frame, so fudge it
	int size = g_numbones * sizeof( s_bone_t );
	int j = panim->numframes;

	float scale = 1.0f / (j - 1.0f);

	panim->sanim[j] = (s_bone_t *)Mem_Alloc( size );

	Vector deltapos;

	for( int k = 0; k < g_numbones; k++ )
	{
		deltapos = panim->sanim[j-1][k].pos - panim->sanim[0][k].pos;
		panim->sanim[j-1][k].pos += deltapos * scale;
		panim->sanim[j][k].rot = panim->sanim[0][k].rot;
	}

	panim->numframes = j + 1;
}

//-----------------------------------------------------------------------------
// Purpose: shift the frames of the animation so that it starts on the desired frame
//-----------------------------------------------------------------------------
void realignLooping( s_animation_t *panim )
{	
	int	j, k;

	// realign looping animations
	if( panim->numframes > 1 && panim->looprestart )
	{
		if( panim->looprestart >= panim->numframes )
		{
			COM_FatalError( "loopstart (%d) out of range for animation %s (%d)", panim->looprestart, panim->name, panim->numframes );
		}

		for( k = 0; k < g_numbones; k++ )
		{
			Vector	shiftpos[MAXSTUDIOANIMATIONS];
			Radian	shiftrot[MAXSTUDIOANIMATIONS];
			int	n;

			// printf("%f %f %f\n", motion[0], motion[1], motion[2] );
			for( j = 0; j < panim->numframes - 1; j++ )
			{	
				n = (j + panim->looprestart) % (panim->numframes - 1);
				shiftpos[j] = panim->sanim[n][k].pos;
				shiftrot[j] = panim->sanim[n][k].rot;
			}

			n = panim->looprestart;
			j = panim->numframes - 1;
			shiftpos[j] = panim->sanim[n][k].pos;
			shiftrot[j] = panim->sanim[n][k].rot;

			for( j = 0; j < panim->numframes; j++ )
			{	
				panim->sanim[j][k].pos = shiftpos[j];
				panim->sanim[j][k].rot = shiftrot[j];
			}
		}
	}
}

void OptimizeAnimations( void )
{
	int	i, j;

	// optimize animations
	for (i = 0; i < g_numseq; i++)
	{
		for( j = 0; j < g_sequence[i].numevents; j++ )
		{
			if( g_sequence[i].event[j].frame < g_sequence[i].panim[0]->startframe )
			{
				MsgDev( D_WARN, "sequence %s has event (%d) before first frame (%d)\n",
				g_sequence[i].name, g_sequence[i].event[j].frame, g_sequence[i].panim[0]->startframe );
				g_sequence[i].event[j].frame = g_sequence[i].panim[0]->startframe;
			}

			if( g_sequence[i].event[j].frame > g_sequence[i].panim[0]->endframe )
			{
				MsgDev( D_WARN, "sequence %s has event (%d) after last frame (%d)\n",
				g_sequence[i].name, g_sequence[i].event[j].frame, g_sequence[i].panim[0]->endframe );
				g_sequence[i].event[j].frame = g_sequence[i].panim[0]->endframe;
			}
		}

		g_sequence[i].linearmovement = g_sequence[i].panim[0]->linearmovement;
		g_sequence[i].frameoffset = g_sequence[i].panim[0]->startframe;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Acculumate quaternions and try to find the swept area of rotation 
//			so that a "midpoint" of the rotation area can be found
//-----------------------------------------------------------------------------
void findAnimQuaternionAlignment( int k, int i, Vector4D &qBase, Vector4D &qMin, Vector4D &qMax )
{
	int	j;

	AngleQuaternion( g_panimation[i]->sanim[0][k].rot, qBase );

	qMin = qBase;
	float dMin = 1.0;
	qMax = qBase;
	float dMax = 1.0;
	
	for( j = 1; j < g_panimation[i]->numframes; j++ )
	{
		Vector4D	q;

		AngleQuaternion( g_panimation[i]->sanim[j][k].rot, q );
		QuaternionAlign( qBase, q, q );

		float d0 = DotProduct( q, qBase );
		float d1 = DotProduct( q, qMin );
		float d2 = DotProduct( q, qMax );

		if( d1 >= d0 )
		{
			if( d0 < dMin )
			{
				qMin = q;
				dMin = d0;
				if( dMax == 1.0 )
				{
					QuaternionMA( qBase, -0.01f, qMin, qMax );
					QuaternionAlign( qBase, qMax, qMax );
				}
			}
		}
		else if( d2 >= d0 )
		{
			if( d0 < dMax )
			{
				qMax = q;
				dMax = d0;
			}
		}

		QuaternionSlerpNoAlign( qMin, qMax, 0.5f, qBase );

		dMin = DotProduct( qBase, qMin );
		dMax = DotProduct( qBase, qMax );
	}
}

//-----------------------------------------------------------------------------
// Purpose: For specific bones, try to find the total valid area of rotation so 
//			that their mid point of rotation can be used at run time to "pre-align"
//			the quaternions so that rotations > 180 degrees don't get blended the 
//			"short way round".
//-----------------------------------------------------------------------------
void limitBoneRotations( void )
{
	int	i, j, k;

	for( i = 0; i < g_numlimitrotation; i++ )
	{
		Vector4D	qBase;

		k = findGlobalBone( g_limitrotation[i].name );
		if( k == -1 )
		{
			COM_FatalError( "unknown bone \"%s\" in $limitrotation\n", g_limitrotation[i].name );
		}

		AngleQuaternion( g_bonetable[k].rot, qBase );

		if( g_limitrotation[i].numseq == 0 )
		{
			for( j = 0; j < g_numani; j++ )
			{
				if( !FBitSet( g_panimation[j]->flags, STUDIO_DELTA ) && g_panimation[j]->numframes > 3 )
				{
					Vector4D	qBase2, qMin2, qMax2;

					findAnimQuaternionAlignment( k, j, qBase2, qMin2, qMax2 );
					QuaternionAdd( qBase, qBase2, qBase );
				}
			}

			qBase = qBase.Normalize();
		}
		else
		{
			for (j = 0; j < g_limitrotation[i].numseq; j++)
			{

			}
		}

		g_bonetable[k].qAlignment = qBase;
		g_bonetable[k].flags |= BONE_FIXED_ALIGNMENT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Realign the matrix so that its X axis points along the desired axis.
//-----------------------------------------------------------------------------
void AlignIKMatrix( matrix3x4 &mMat, const Vector &vAlignTo )
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
bool solveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, Vector &targetKneePos, Vector &targetKneeDir, matrix3x4 *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	worldThigh = pBoneToWorld[iThigh].GetOrigin();
	worldKnee = pBoneToWorld[iKnee].GetOrigin();
	worldFoot = pBoneToWorld[iFoot].GetOrigin();

	Vector ikFoot, ikTargetKnee, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = targetKneePos - worldThigh;

	float l1 = (worldKnee - worldThigh).Length();
	float l2 = (worldFoot - worldKnee).Length();

	// exaggerate knee targets for legs that are nearly straight
	// FIXME: should be configurable, and the ikKnee should be from the original animation, not modifed
	float d = (targetFoot - worldThigh).Length() - Q_min( l1, l2 );
	d = Q_max( l1 + l2, d );

	// FIXME: too short knee directions cause trouble
	d = d * 100.0f;

	ikTargetKnee = ikKnee + targetKneeDir * d;

	// too far away? (0.9998 is about 1 degree)
	if( ikFoot.Length() > ( l1 + l2 ) * KNEEMAX_EPSILON )
	{
		ikFoot = ikFoot.Normalize();
		ikFoot *= (l1 + l2) * KNEEMAX_EPSILON;
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

	CIKSolver ik;

	if( ik.solve( l1, l2, ikFoot, ikTargetKnee, ikKnee ))
	{
		matrix3x4	&mWorldThigh = pBoneToWorld[ iThigh ];
		matrix3x4	&mWorldKnee = pBoneToWorld[ iKnee ];
		matrix3x4	&mWorldFoot = pBoneToWorld[ iFoot ];

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

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Solve Knee position for a known hip and foot location, but no specific knee direction preference
//-----------------------------------------------------------------------------
bool solveIK( int iThigh, int iKnee, int iFoot, Vector &targetFoot, matrix3x4 *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	worldThigh = pBoneToWorld[iThigh].GetOrigin();
	worldKnee = pBoneToWorld[iKnee].GetOrigin();
	worldFoot = pBoneToWorld[iFoot].GetOrigin();

	Vector ikFoot, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = worldKnee - worldThigh;

	float l1 = (worldKnee - worldThigh).Length();
	float l2 = (worldFoot - worldKnee).Length();
	float l3 = (worldFoot - worldThigh).Length();

	// leg too straight to figure out knee?
	if( l3 > (l1 + l2) * KNEEMAX_EPSILON )
		return false;

	Vector ikHalf = (worldFoot - worldThigh) * (l1 / l3);

	// FIXME: what to do when the knee completely straight?
	Vector ikKneeDir = (ikKnee - ikHalf).Normalize();

	return solveIK( iThigh, iKnee, iFoot, targetFoot, worldKnee, ikKneeDir, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: calc the influence of a ik rule for a specific point in the animation cycle
//-----------------------------------------------------------------------------
float IKRuleWeight( s_ikrule_t *pRule, float flCycle )
{
	if( pRule->end > 1.0f && flCycle < pRule->start )
	{
		flCycle = flCycle + 1.0f;
	}

	float value = 0.0f;
	if( flCycle < pRule->start )
	{
		return 0.0f;
	}
	else if( flCycle < pRule->peak )
	{
		value = (flCycle - pRule->start) / (pRule->peak - pRule->start);
	}
	else if( flCycle < pRule->tail )
	{
		return 1.0f;
	}
	else if( flCycle < pRule->end )
	{
		value = 1.0f - ((flCycle - pRule->tail) / (pRule->end - pRule->tail));
	}

	return 3.0f * value * value - 2.0f * value * value * value;
}

//-----------------------------------------------------------------------------
// Purpose: Lock the ik target to a specific location in order to clean up bad animations (shouldn't be needed).
//-----------------------------------------------------------------------------
void fixupIKErrors( s_animation_t *panim, s_ikrule_t *pRule )
{
	int	k;

	if( pRule->start == 0 && pRule->peak == 0 && pRule->tail == 0 && pRule->end == 0 )
	{
		pRule->tail = panim->numframes - 1;
		pRule->end = panim->numframes - 1;
	}

	// check for wrapping
	if( pRule->peak < pRule->start )
	{
		pRule->peak += panim->numframes - 1;
	}

	if( pRule->tail < pRule->peak )
	{
		pRule->tail += panim->numframes - 1;
	}

	if( pRule->end < pRule->tail )
	{
		pRule->end += panim->numframes - 1;
	}

	if( pRule->contact == -1 )
	{
		pRule->contact = pRule->peak;
	}

	if( panim->numframes <= 1 )
		return;

	pRule->errorData.numerror = pRule->end - pRule->start + 1;
	
	switch( pRule->type )
	{
	case IK_SELF:
		break;
	case IK_WORLD:
	case IK_GROUND:
		{
			matrix3x4	boneToWorld[MAXSTUDIOBONES];

			int bone = g_ikchain[pRule->chain].link[2].bone;
			CalcBoneTransforms( panim, pRule->contact, boneToWorld );
			// FIXME: add in motion

			Vector footfall = boneToWorld[bone].GetOrigin();

			for( k = 0; k < pRule->errorData.numerror; k++ )
			{
				CalcBoneTransforms( panim, k + pRule->start, boneToWorld );

				float cycle = (panim->numframes <= 1) ? 0 : (float)(k + pRule->start) / (panim->numframes - 1);
				float s = IKRuleWeight( pRule, cycle );
				s = 1.0f; // FIXME - the weight rule is wrong

				Vector orig = boneToWorld[g_ikchain[pRule->chain].link[2].bone].GetOrigin();

				Vector pos = (footfall + calcMovement( panim, k + pRule->start, pRule->contact )) * s + orig * (1.0 - s);

				solveIK( g_ikchain[pRule->chain].link[0].bone, g_ikchain[pRule->chain].link[1].bone, g_ikchain[pRule->chain].link[2].bone, pos, boneToWorld );
				solveBone( panim, k + pRule->start, g_ikchain[pRule->chain].link[0].bone, boneToWorld );  
				solveBone( panim, k + pRule->start, g_ikchain[pRule->chain].link[1].bone, boneToWorld );  
				solveBone( panim, k + pRule->start, g_ikchain[pRule->chain].link[2].bone, boneToWorld );  
			}
		}	
	}

	forceAnimationLoop( panim ); // !!!
}

//-----------------------------------------------------------------------------
// Purpose: For specific bones, try to find the total valid area of rotation so 
//			that their mid point of rotation can be used at run time to "pre-align"
//			the quaternions so that rotations > 180 degrees don't get blended the 
//			"short way round".
//-----------------------------------------------------------------------------
void limitIKChainLength( void )
{
	matrix3x4	boneToWorld[MAXSTUDIOSRCBONES];	// bone transformation matrix
	int	i, j, k;

	for( k = 0; k < g_numikchains; k++ )
	{
		Vector	kneeDir = g_ikchain[k].link[0].kneeDir;
		bool	needsFixup = false;
		bool	hasKnees = false;

		if( kneeDir.Length() > 0.0f )
		{
			hasKnees = true;
		}
		else
		{
			for( i = 0; i < g_numani; i++ )
			{
				s_animation_t *panim = g_panimation[i];

				if( FBitSet( panim->flags, STUDIO_DELTA ))
					continue;

				if( FBitSet( panim->flags, STUDIO_HIDDEN ))
					continue;

				for( j = 0; j < panim->numframes; j++ )
				{
					CalcBoneTransforms( panim, j, boneToWorld );

					Vector	worldThigh;
					Vector	worldKnee;
					Vector	worldFoot;

					boneToWorld[g_ikchain[k].link[0].bone].GetOrigin( worldThigh );
					boneToWorld[g_ikchain[k].link[1].bone].GetOrigin( worldKnee );
					boneToWorld[g_ikchain[k].link[2].bone].GetOrigin( worldFoot );

					float l1 = (worldKnee-worldThigh).Length();
					float l2 = (worldFoot-worldKnee).Length();
					float l3 = (worldFoot-worldThigh).Length();

					Vector ikHalf = (worldFoot + worldThigh) * 0.5;

					// FIXME: what to do when the knee completely straight?
					Vector ikKneeDir = (worldKnee - ikHalf).Normalize();
					// ikTargetKnee = ikKnee + ikKneeDir * l1;

					// leg too straight to figure out knee?
					if( l3 > ( l1 + l2 ) * 0.999f )
					{
						needsFixup = true;
					}
					else
					{
						// rotate knee into local space
						Vector tmp = boneToWorld[g_ikchain[k].link[0].bone].VectorIRotate( ikKneeDir );
						float bend = (((DotProduct( worldThigh - worldKnee, worldFoot - worldKnee )) / (l1 * l3)) + 1.0f ) / 2.0f;
						kneeDir += tmp * bend;
						hasKnees = true;
					}
				}
			}
		}

		if( !needsFixup )
			continue;

		if( !hasKnees )
		{
			MsgDev( D_WARN, "ik rules for %s but no clear knee direction\n", g_ikchain[k].name );
			continue;
		}

		kneeDir = kneeDir.Normalize();
		g_ikchain[k].link[0].kneeDir = kneeDir;
		MsgDev( D_REPORT, "knee %s %f %f %f\n", g_ikchain[k].name, kneeDir.x, kneeDir.y, kneeDir.z );
	}
}

void MakeTransitions( void )
{
	bool	iHit = g_multistagegraph;
	int	i, j, k;

	// add in direct node transitions
	for( i = 0; i < g_numseq; i++ )
	{
		if( g_sequence[i].entrynode != g_sequence[i].exitnode )
		{
			g_xnode[g_sequence[i].entrynode-1][g_sequence[i].exitnode-1] = g_sequence[i].exitnode;
			if( g_sequence[i].nodeflags )
				g_xnode[g_sequence[i].exitnode-1][g_sequence[i].entrynode-1] = g_sequence[i].entrynode;
		}

		if( g_sequence[i].entrynode > g_numxnodes )
			g_numxnodes = g_sequence[i].entrynode;
	}

	// calculate multi-stage transitions 
	while( iHit )
	{
		iHit = false;
		for( i = 1; i <= g_numxnodes; i++ )
		{
			for( j = 1; j <= g_numxnodes; j++ )
			{
				// if I can't go there directly
				if( i != j && g_xnode[i-1][j-1] == 0 )
				{
					for( k = 1; k <= g_numxnodes; k++ )
					{
						// but I found someone who knows how that I can get to
						if( g_xnode[k-1][j-1] > 0 && g_xnode[i-1][k-1] > 0 )
						{
							// then go to them
							g_xnode[i-1][j-1] = -g_xnode[i-1][k-1];
							iHit = true;
							break;
						}
					}
				}
			}
		}
		// reset previous pass so the links can be used in the next pass
		for( i = 1; i <= g_numxnodes; i++ )
		{
			for( j = 1; j <= g_numxnodes; j++ )
			{
				g_xnode[i-1][j-1] = abs( g_xnode[i-1][j-1] );
			}
		}
	}

	// add in allowed "skips"
	for( i = 0; i < g_numxnodeskips; i++ )
	{
		g_xnode[g_xnodeskip[i][0]-1][g_xnodeskip[i][1]-1] = 0;
	}

	if( g_dump_graph )
	{
		for( j = 1; j <= g_numxnodes; j++ )
		{
			Msg( "%2d : %s\n", j, g_xnodename[j] );
		}
		Msg( "    " );
		for( j = 1; j <= g_numxnodes; j++ )
		{
			Msg( "%2d ", j );
		}
		Msg( "\n" );

		for( i = 1; i <= g_numxnodes; i++ )
		{
			Msg( "%2d: ", i );
			for( j = 1; j <= g_numxnodes; j++ )
			{
				Msg( "%2d ", g_xnode[i-1][j-1] );
			}
			Msg( "\n" );
		}
	}
}

void processAnimations( void )
{
	int	i, j;

	// find global root bone.
	if( Q_strlen( rootname ))
	{
		g_rootIndex = findGlobalBone( rootname );
		if( g_rootIndex == -1 ) g_rootIndex = 0;
	}

	buildAnimationWeights( );

	for( i = 0; i < g_numani; i++ )
	{
		s_animation_t *panim = g_panimation[i];

		extractUnusedMotion( panim ); // FIXME: this should be part of LinearMotion()

		setAnimationWeight( panim, 0 );

		int startframe = 0;

		if( panim->fudgeloop )
		{
			fixupMissingFrame( panim );
		}

		for( j = 0; j < panim->numcmds; j++ )
		{
			s_animcmd_t *pcmd = &panim->cmds[j];

			switch( pcmd->cmd )
			{
			case CMD_WEIGHTS:
				setAnimationWeight( panim, pcmd->weightlist.index );
				break;
			case CMD_SUBTRACT:
				panim->flags |= STUDIO_DELTA;
				subtractBaseAnimations( pcmd->subtract.ref, panim, pcmd->subtract.frame, pcmd->subtract.flags );
				break;
			case CMD_AO:
				{
					int bone = g_rootIndex;
					if( pcmd->ao.pBonename != NULL )
					{
						bone = findGlobalBone( pcmd->ao.pBonename );
						if( bone == -1 )
						{
							COM_FatalError("unable to find bone %s to alignbone\n", pcmd->ao.pBonename );
						}
					}
					processAutoorigin( pcmd->ao.ref, panim, pcmd->ao.motiontype, pcmd->ao.srcframe, pcmd->ao.destframe, bone );
				}
				break;
			case CMD_MATCH:
				processMatch( pcmd->match.ref, panim, false );
				break;
			case CMD_FIXUP:
				fixupLoopingDiscontinuities( panim, pcmd->fixuploop.start, pcmd->fixuploop.end );
				break;
			case CMD_ANGLE:
				makeAngle( panim, pcmd->angle.angle );
				break;
			case CMD_IKFIXUP:
				break;
			case CMD_IKRULE:
				// processed later
				break;
			case CMD_MOTION:
				extractLinearMotion( panim, pcmd->motion.motiontype, startframe, pcmd->motion.iEndFrame, pcmd->motion.iEndFrame, panim, startframe );
				startframe = pcmd->motion.iEndFrame;
				break;
			case CMD_REFMOTION:
				extractLinearMotion( panim, pcmd->motion.motiontype, startframe, pcmd->motion.iEndFrame, pcmd->motion.iSrcFrame, pcmd->motion.pRefAnim, pcmd->motion.iRefFrame );
				startframe = pcmd->motion.iEndFrame;
				break;
			case CMD_DERIVATIVE:
				createDerivative( panim, pcmd->derivative.scale );
				break;
			case CMD_NOANIMATION:
				clearAnimations( panim );
				break;
			case CMD_LINEARDELTA:
				panim->flags |= STUDIO_DELTA;
				linearDelta( panim, panim, panim->numframes - 1, pcmd->linear.flags );
				break;
			case CMD_COMPRESS:
				reencodeAnimation( panim, pcmd->compress.frames );
				break;
			case CMD_NUMFRAMES:
				forceNumframes( panim, pcmd->numframes.frames );
				break;
			case CMD_COUNTERROTATE:
				{
					int bone = findGlobalBone( pcmd->counterrotate.pBonename );
					if( bone != -1 )
					{
						Vector	target;

						if( !pcmd->counterrotate.bHasTarget )
						{
							matrix3x4	rootxform = matrix3x4( g_vecZero, panim->rotation );
							matrix3x4	defaultBoneToWorld;
							defaultBoneToWorld = rootxform.ConcatTransforms( g_bonetable[bone].boneToPose );
							target = defaultBoneToWorld.GetAngles();
						}
						else
						{
							target = Vector( pcmd->counterrotate.targetAngle );
						}

						counterRotateBone( panim, bone, target );
					}
					else
					{
						COM_FatalError( "unable to find bone %s to counterrotate\n", pcmd->counterrotate.pBonename );
					}
				}
				break;
			case CMD_WORLDSPACEBLEND:
				worldspaceBlend( pcmd->world.ref, panim, pcmd->world.startframe, pcmd->world.loops );
				break;
			case CMD_MATCHBLEND:
				matchBlend( panim, pcmd->match.ref, pcmd->match.srcframe, pcmd->match.destframe, pcmd->match.destpre, pcmd->match.destpost );
				break;
			}
		}

		if( panim->motiontype )
		{
			int	lastframe;

			if( !FBitSet( panim->flags, STUDIO_LOOPING ))
			{
				// roll back 0.2 seconds to try to prevent popping
				int frames = panim->fps * panim->motionrollback;
				lastframe = Q_max( Q_min( startframe + 1, panim->numframes - 1 ), panim->numframes - frames - 1 );
			}
			else
			{
				lastframe = panim->numframes - 1;
			}

			extractLinearMotion( panim, panim->motiontype, startframe, lastframe, panim->numframes - 1, panim, startframe );
			startframe = panim->numframes - 1;
		}

		realignLooping( panim );
		forceAnimationLoop( panim );
	}

	// merge weightlists
	for( i = 0; i < g_numseq; i++ )
	{
		for( int n = 0; n < g_numbones; n++ )
		{
			g_sequence[i].weight[n] = 0.0f;

			for( int j = 0; j < g_sequence[i].groupsize[0]; j++ )
			{
				for( int k = 0; k < g_sequence[i].groupsize[1]; k++ )
				{
					int q = j + g_sequence[i].groupsize[0] * k;
					g_sequence[i].weight[n] = Q_max( g_sequence[i].weight[n], g_sequence[i].panim[q]->weight[n] );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// CompressAnimations
//-----------------------------------------------------------------------------
static void CompressAnimations( void )
{
	int	i, j, k, n, m;
	float	v;

	// find scales for all bones
	for( j = 0; j < g_numbones; j++ )
	{
		for( k = 0; k < 6; k++ )
		{
			float	minv, maxv, scale;

			if( k < 3 ) 
			{
				minv = -128.0f;
				maxv = 128.0f;
			}
			else
			{
				minv = -M_PI / 8.0;
				maxv = M_PI / 8.0;
			}

			for( i = 0; i < g_numani; i++ )
			{
				s_animation_t *panim = g_panimation[i];

				for( n = 0; n < panim->numframes; n++ )
				{
					switch( k )
					{
					case 0: 
					case 1: 
					case 2: 
						if( panim->flags & STUDIO_DELTA ) v = panim->sanim[n][j].pos[k]; 
						else v = ( panim->sanim[n][j].pos[k] - g_bonetable[j].pos[k] ); 
						break;
					case 3:
					case 4:
					case 5:
						if( panim->flags & STUDIO_DELTA ) v = panim->sanim[n][j].rot[k-3]; 
						else v = ( panim->sanim[n][j].rot[k-3] - g_bonetable[j].rot[k-3] ); 
						clip_rotations( v );
						break;
					}

					minv = Q_min( v, minv );
					maxv = Q_max( v, maxv );
				}
			}

			if( minv < maxv )
			{
				if( -minv > maxv )
					scale = minv / -32768.0f;
				else scale = maxv / 32767.0f;
			}
			else
			{
				scale = 1.0f / 32.0f;
			}

			switch( k )
			{
			case 0: 
			case 1: 
			case 2: 
				g_bonetable[j].posscale[k] = scale;
				break;
			case 3:
			case 4:
			case 5:
				g_bonetable[j].rotscale[k-3] = scale;
				break;
			}
		}
	}

	int	changes = 0;
	int	total = 0;

	// reduce animations
	for( i = 0; i < g_numani; i++ )
	{
		s_animation_t *panim = g_panimation[i];

		for( j = 0; j < g_numbones; j++ )
		{
			if( FBitSet( g_bonetable[j].flags, BONE_ALWAYS_PROCEDURAL ))
				continue;

			// skip bones that have no influence
			if( panim->weight[j] < 0.001f )
				continue;

			for( k = 0; k < 6; k++ )
			{
				mstudioanimvalue_t data[MAXSTUDIOANIMATIONS];
				mstudioanimvalue_t *pcount, *pvalue;
				short value[MAXSTUDIOANIMATIONS];

				if( panim->numframes <= 0 )
					COM_FatalError( "no animation frames: \"%s\"\n", panim->name );

				// find deltas from default pose
				for( n = 0; n < panim->numframes; n++ )
				{
					s_bone_t *psrcdata = &panim->sanim[n][j];

					switch( k )
					{
					case 0: 
					case 1: 
					case 2: 
						if( panim->flags & STUDIO_DELTA )
						{
							value[n] = psrcdata->pos[k] / g_bonetable[j].posscale[k]; 
							// pre-scale pos delta since format only has room for "overall" weight
							float r = panim->posweight[j] / panim->weight[j];
							value[n] *= r;
						}
						else
						{
							v = ( psrcdata->pos[k] - g_bonetable[j].pos[k] );
							value[n] = v / g_bonetable[j].posscale[k];
						}
						break;
					case 3:
					case 4:
					case 5:
						if( panim->flags & STUDIO_DELTA ) v = psrcdata->rot[k-3]; 
						else v = ( psrcdata->rot[k-3] - g_bonetable[j].rot[k-3] ); 
						clip_rotations( v );
						value[n] = v / g_bonetable[j].rotscale[k-3]; 
						break;
					}
				}

				// FIXME: this compression algorithm needs work

				// initialize animation RLE block
				panim->numanim[j][k] = 0;

				memset( data, 0, sizeof( data )); 
				pcount = data; 
				pvalue = pcount + 1;

				pcount->num.valid = 1;
				pcount->num.total = 1;
				pvalue->value = value[0];
				pvalue++;
				changes++;
				total++;

				for( m = 1; m < n; m++ )
				{
					if( pcount->num.total == 255 )
					{
						// chain too long, force a new entry
						pcount = pvalue;
						pvalue = pcount + 1;
						pcount->num.valid++;
						pvalue->value = value[m];
						pvalue++;
						changes++;
					} 

					// insert value if they're not equal, 
					// or if we're not on a run and the run is less than 3 units
					else if( !cmp_animvalue( m, m - 1 ) || (( pcount->num.total == pcount->num.valid )
					&& (( m < n - 1 ) && !cmp_animvalue( m, m + 1 ))))
					{
						if( pcount->num.total != pcount->num.valid )
						{
							pcount = pvalue;
							pvalue = pcount + 1;
						}
						pcount->num.valid++;
						pvalue->value = value[m];
						pvalue++;
						changes++;
					}
					pcount->num.total++;
					total++;
				}

				panim->numanim[j][k] = pvalue - data;
				if( panim->numanim[j][k] == 2 && value[0] == 0 )
				{
					panim->numanim[j][k] = 0;
				}
				else
				{
					size_t anim_size = ( pvalue - data ) * sizeof( mstudioanimvalue_t );
					panim->anim[j][k] = (mstudioanimvalue_t *)Mem_Alloc( anim_size );
					memmove( panim->anim[j][k], data, anim_size );
				}
			}
		}
	}

	if( total != 0 )
		MsgDev( D_INFO, "animation compressed of %.1f%c at original size\n", ((float)changes / (float)total ) * 100.0f, '%' );
}

//-----------------------------------------------------------------------------
// Compress a single animation stream
//-----------------------------------------------------------------------------
static void CompressSingle( s_animationstream_t *pStream )
{
	int	k, n, m;

	if( pStream->numerror == 0 )
		return;

	for( k = 0; k < 6; k++ )
	{
		float	minv, maxv, scale;
		Radian	ang;

		if( k < 3 ) 
		{
			minv = -128.0f;
			maxv = 128.0f;
		}
		else
		{
			minv = -M_PI / 8.0;
			maxv = M_PI / 8.0;
		}

		for( n = 0; n < pStream->numerror; n++ )
		{
			float v = 0.0f;
			switch( k )
			{
			case 0: 
			case 1: 
			case 2: 
				v = pStream->pError[n].pos[k];
				break;
			case 3:
			case 4:
			case 5:
				QuaternionAngle( pStream->pError[n].q, ang );
				v = ang[k-3];
				clip_rotations( v );
				break;
			}

			minv = Q_min( v, minv );
			maxv = Q_max( v, maxv );
		}

		if( minv < maxv )
		{
			if( -minv > maxv )
				scale = minv / -32768.0f;
			else scale = maxv / 32767.0f;
		}
		else
		{
			scale = 1.0f / 32.0f;
		}

		pStream->scale[k] = scale;
		
		mstudioanimvalue_t *pcount, *pvalue;
		short value[MAXSTUDIOANIMATIONS];
		mstudioanimvalue_t data[MAXSTUDIOANIMATIONS];
		float v;

		// find deltas from default pose
		for( n = 0; n < pStream->numerror; n++ )
		{
			switch( k )
			{
			case 0:	// X Position
			case 1:	// Y Position
			case 2:	// Z Position
				value[n] = pStream->pError[n].pos[k] / pStream->scale[k]; 
				break;
			case 3:	// X Rotation
			case 4:	// Y Rotation
			case 5:	// Z Rotation
				QuaternionAngle( pStream->pError[n].q, ang );
				v = ang[k-3];
				clip_rotations( v );
				value[n] = v / pStream->scale[k];
				break;
			}
		}

		// initialize animation RLE block
		pStream->numanim[k] = 0;

		memset( data, 0, sizeof( data )); 
		pcount = data; 
		pvalue = pcount + 1;

		pcount->num.valid = 1;
		pcount->num.total = 1;
		pvalue->value = value[0];
		pvalue++;

		// build a RLE of deltas from the default pose
		for( m = 1; m < n; m++ )
		{
			if( pcount->num.total == 255 )
			{
				// chain too long, force a new entry
				pcount = pvalue;
				pvalue = pcount + 1;
				pcount->num.valid++;
				pvalue->value = value[m];
				pvalue++;
			} 

			// insert value if they're not equal, 
			// or if we're not on a run and the run is less than 3 units
			else if( !cmp_animvalue( m, m - 1 ) || (( pcount->num.total == pcount->num.valid )
			&& (( m < n - 1 ) && !cmp_animvalue( m, m + 1 ))))
			{
				if( pcount->num.total != pcount->num.valid )
				{
					pcount = pvalue;
					pvalue = pcount + 1;
				}
				pcount->num.valid++;
				pvalue->value = value[m];
				pvalue++;
			}
			pcount->num.total++;
		}

		pStream->numanim[k] = pvalue - data;
		pStream->anim[k] = (mstudioanimvalue_t *)Mem_Alloc(( pvalue - data ) * sizeof( mstudioanimvalue_t ));
		memmove( pStream->anim[k], data, (pvalue - data) * sizeof( mstudioanimvalue_t ));
	}
}

static void CalcSequenceBoundingBoxes( void )
{
	int	i, j, k;
	int	n, m;

	// find bounding box for each sequence
	for( i = 0; i < g_numseq; i++ )
	{
		Vector	bmin, bmax;

		// find intersection box volume for each bone
		ClearBounds( bmin, bmax );

		s_animation_t *panim = g_panimation[i];

		for( n = 0; n < panim->numframes; n++ )
		{
			matrix3x4	bonetransform[MAXSTUDIOBONES];	// bone transformation matrix
			matrix3x4	posetransform[MAXSTUDIOBONES];	// bone transformation matrix
			matrix3x4	bonematrix;			// local transformation matrix
			Vector pos, tmp;

			for( j = 0; j < g_numbones; j++ )
			{
				bonematrix = matrix3x4( panim->sanim[n][j].pos, panim->sanim[n][j].rot );
				if( g_bonetable[j].parent == -1 ) bonetransform[j] = bonematrix;
				else bonetransform[j] = bonetransform[g_bonetable[j].parent].ConcatTransforms( bonematrix );

				bonematrix = g_bonetable[j].boneToPose.Invert();
				posetransform[j] = bonetransform[j].ConcatTransforms( bonematrix );
			}

			// include bones as well.
			for( k = 0; k < g_numbones; k++ )
			{
				Vector tmpMin, tmpMax;
				TransformAABB( bonetransform[k], g_bonetable[k].bmin, g_bonetable[k].bmax, tmpMin, tmpMax );
				AddPointToBounds( tmpMin, bmin, bmax );
				AddPointToBounds( tmpMax, bmin, bmax );
			}

			// include vertices
			for( k = 0; k < g_nummodels; k++ )
			{
				for( j = 0; j < g_model[k]->numsrcverts; j++ )
				{
					s_srcvertex_t *v = &g_model[k]->srcvert[j];
					pos = g_vecZero;

					for( m = 0; m < v->globalWeight.numbones; m++ )
					{
						if( has_boneweights )
							tmp = posetransform[v->globalWeight.bone[m]].VectorTransform( v->vert );
						else tmp = bonetransform[v->globalWeight.bone[m]].VectorTransform( v->vert );
						pos += tmp * v->globalWeight.weight[m];
					}
					AddPointToBounds( pos, bmin, bmax );
				}
			}
		}

		panim->bmin = bmin;
		panim->bmax = bmax;
	}

	for( i = 0; i < g_numseq; i++ )
	{
		Vector	bmin, bmax;
		
		// find intersection box volume for each bone
		ClearBounds( bmin, bmax );

		for( j = 0; j < g_sequence[i].numblends; j++ )
		{
			s_animation_t *panim = g_sequence[i].panim[j];
			AddPointToBounds( panim->bmin, bmin, bmax );
			AddPointToBounds( panim->bmax, bmin, bmax );
		}

		g_sequence[i].bmin = bmin;
		g_sequence[i].bmax = bmax;
	}
}

//-----------------------------------------------------------------------------
// Links bone controllers
//-----------------------------------------------------------------------------
static void LinkBoneControllers( void )
{
	for( int i = 0; i < g_numbonecontrollers; i++ )
	{
		int j = findGlobalBone( g_bonecontroller[i].name );
		if( j == -1 )
			COM_FatalError( "unknown g_bonecontroller link '%s'\n", g_bonecontroller[i].name );
		g_bonecontroller[i].bone = j;
	}
}

//-----------------------------------------------------------------------------
// Find autolayers
//-----------------------------------------------------------------------------
static void FindAutolayers( void )
{
	for( int i = 0; i < g_numseq; i++ )
	{
		for( int k = 0; k < g_sequence[i].numautolayers; k++ )
		{
			int	j;

			for( j = 0; j < g_numseq; j++)
			{
				if( !Q_stricmp( g_sequence[i].autolayer[k].name, g_sequence[j].name ))
				{
					g_sequence[i].autolayer[k].sequence = j;
					break;
				}
			}

			if( j == g_numseq )
			{
				COM_FatalError( "sequence \"%s\" cannot find autolayer sequence \"%s\"\n", g_sequence[i].name, g_sequence[i].autolayer[k].name );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Links screen aligned bones
//-----------------------------------------------------------------------------
static void TagScreenAlignedBones( void )
{
	for( int i = 0; i < g_numscreenalignedbones; i++ )
	{
		int j = findGlobalBone( g_screenalignedbone[i].name );
		if( j == -1 )
		{
			COM_FatalError( "unknown screenaligned bone link '%s'\n", g_screenalignedbone[i].name );
		}

		g_bonetable[j].flags |= g_screenalignedbone[i].flags;
		MsgDev( D_REPORT, "tagging bone: %s as screen aligned (index %i, flags:%x)\n", g_bonetable[j].name, j, g_bonetable[j].flags );
	}
}

//-----------------------------------------------------------------------------
// Links attachments
//-----------------------------------------------------------------------------
static void LinkAttachments( void )
{
	int i, j, k;

	matrix3x4	boneToPose;
	matrix3x4	world;
	matrix3x4	poseToBone;

	// attachments may be connected to bones that can be optimized out
	// so search through all the sources and move to a valid location
	for( i = 0; i < g_numattachments; i++ )
	{
		bool	found = false;

		// search through known bones
		k = findGlobalBone( g_attachment[i].bonename );
		if( k != -1 )
		{
			g_attachment[i].bone = k;
			boneToPose = g_bonetable[k].boneToPose;
			poseToBone = boneToPose.Invert();
			found = true;
		}

		if( !found )
		{
			// search all the loaded sources for the first occurance of the named bone
			for( j = 0; j < g_nummodels && !found; j++ )
			{
				for( k = 0; k < g_model[j]->numbones && !found; k++ )
				{
					if( !Q_stricmp( g_attachment[i].bonename, g_model[j]->localBone[k].name ) )
					{
						boneToPose = g_model[j]->boneToPose[k];

						// check to make sure that this bone is actually referenced in the output model
						// if not, try parent bone until we find a referenced bone in this chain
						while( k != -1 && g_model[j]->boneGlobalToLocal[g_model[j]->boneLocalToGlobal[k]] != k )
						{
							k = g_model[j]->localBone[k].parent;
						}
						if( k == -1 )
						{
							COM_FatalError( "unable to find valid bone for attachment %s:%s\n", 
								g_attachment[i].name, g_attachment[i].bonename );
						}

						poseToBone = g_model[j]->boneToPose[k].Invert();
						g_attachment[i].bone = g_model[j]->boneLocalToGlobal[k];
						found = true;
					}
				}
			}
		}

		if( !found ) COM_FatalError( "unknown attachment link '%s'\n", g_attachment[i].bonename );

		if( g_attachment[i].type & IS_ABSOLUTE ) world = g_attachment[i].local;
		else world = boneToPose.ConcatTransforms( g_attachment[i].local );
	
		g_attachment[i].local = poseToBone.ConcatTransforms( world );
	}

	// flag all bones used by attachments
	for( i = 0; i < g_numattachments; i++ )
	{
		j = g_attachment[i].bone;
		while( j != -1 )
		{
			g_bonetable[j].flags |= BONE_USED_BY_ATTACHMENT;
			j = g_bonetable[j].parent;
		}
	}
}

static void SetupHitBoxes( void )
{
	int	i, j, k, n;

	// set hitgroups
	for( k = 0; k < g_numbones; k++ )
	{
		g_bonetable[k].group = -9999;
	}

	for( j = 0; j < g_numhitgroups; j++ )
	{
		k = findGlobalBone( g_hitgroup[j].name );
		if( k != -1 )
		{
			g_bonetable[k].group = g_hitgroup[j].group;
		}
		else
		{
			COM_FatalError( "cannot find bone %s for hitgroup %d\n", g_hitgroup[j].name, g_hitgroup[j].group );
		}
	}

	for( k = 0; k < g_numbones; k++ )
	{
		if( g_bonetable[k].group == -9999 )
		{
			if( g_bonetable[k].parent != -1 )
				g_bonetable[k].group = g_bonetable[g_bonetable[k].parent].group;
			else g_bonetable[k].group = 0;
		}
	}

	if( g_hitboxsets.Size() == 0 )
	{
		int index = g_hitboxsets.AddToTail();
		s_hitboxset_t *set = &g_hitboxsets[index];
		memset( set, 0, sizeof( *set ));
		Q_strncpy( set->hitboxsetname, "default", sizeof( set->hitboxsetname ));

		// find intersection box volume for each bone
		for( k = 0; k < g_numbones; k++ )
		{
			g_bonetable[k].bmin = g_vecZero;
			g_bonetable[k].bmax = g_vecZero;
		}

		// try all the connect vertices
		for( i = 0; i < g_nummodels; i++ )
		{
			Vector	p;

			for( j = 0; j < g_model[i]->numsrcverts; j++ )
			{
				for( n = 0; n < g_model[i]->srcvert[j].globalWeight.numbones; n++ )
				{
					k = g_model[i]->srcvert[j].globalWeight.bone[n];

					if( has_boneweights )
						p = g_bonetable[k].boneToPose.VectorITransform( g_model[i]->srcvert[j].vert );
					else p = g_model[i]->srcvert[j].vert;

					if( p[0] < g_bonetable[k].bmin[0] ) g_bonetable[k].bmin[0] = p[0];
					if( p[1] < g_bonetable[k].bmin[1] ) g_bonetable[k].bmin[1] = p[1];
					if( p[2] < g_bonetable[k].bmin[2] ) g_bonetable[k].bmin[2] = p[2];
					if( p[0] > g_bonetable[k].bmax[0] ) g_bonetable[k].bmax[0] = p[0];
					if( p[1] > g_bonetable[k].bmax[1] ) g_bonetable[k].bmax[1] = p[1];
					if( p[2] > g_bonetable[k].bmax[2] ) g_bonetable[k].bmax[2] = p[2];
				}
			}
		}
		// add in all your children as well
		for( k = 0; k < g_numbones; k++ )
		{
			if(( j = g_bonetable[k].parent ) != -1 )
			{
				if( g_bonetable[k].pos[0] < g_bonetable[j].bmin[0] ) g_bonetable[j].bmin[0] = g_bonetable[k].pos[0];
				if( g_bonetable[k].pos[1] < g_bonetable[j].bmin[1] ) g_bonetable[j].bmin[1] = g_bonetable[k].pos[1];
				if( g_bonetable[k].pos[2] < g_bonetable[j].bmin[2] ) g_bonetable[j].bmin[2] = g_bonetable[k].pos[2];
				if( g_bonetable[k].pos[0] > g_bonetable[j].bmax[0] ) g_bonetable[j].bmax[0] = g_bonetable[k].pos[0];
				if( g_bonetable[k].pos[1] > g_bonetable[j].bmax[1] ) g_bonetable[j].bmax[1] = g_bonetable[k].pos[1];
				if( g_bonetable[k].pos[2] > g_bonetable[j].bmax[2] ) g_bonetable[j].bmax[2] = g_bonetable[k].pos[2];
			}
		}

		for( k = 0; k < g_numbones; k++ )
		{
			if( g_bonetable[k].bmin[0] < g_bonetable[k].bmax[0] - 1.0f
				&& g_bonetable[k].bmin[1] < g_bonetable[k].bmax[1] - 1.0f
				&& g_bonetable[k].bmin[2] < g_bonetable[k].bmax[2] - 1.0f )
			{
				set->hitbox[set->numhitboxes].bone = k;
				set->hitbox[set->numhitboxes].group = g_bonetable[k].group;
				set->hitbox[set->numhitboxes].bmin = g_bonetable[k].bmin;
				set->hitbox[set->numhitboxes].bmax = g_bonetable[k].bmax;

				if( g_dump_hboxes )
				{
					Msg( "$hbox %d \"%s\" %.2f %.2f %.2f  %.2f %.2f %.2f\n",
						set->hitbox[set->numhitboxes].group,
						g_bonetable[set->hitbox[set->numhitboxes].bone].name, 
						set->hitbox[set->numhitboxes].bmin[0], set->hitbox[set->numhitboxes].bmin[1], set->hitbox[set->numhitboxes].bmin[2],
						set->hitbox[set->numhitboxes].bmax[0], set->hitbox[set->numhitboxes].bmax[1], set->hitbox[set->numhitboxes].bmax[2] );

				}
				set->numhitboxes++;
			}
		}
	}
	else
	{
		for( int s = 0; s < g_hitboxsets.Size(); s++ )
		{
			s_hitboxset_t *set = &g_hitboxsets[s];

			for( j = 0; j < set->numhitboxes; j++ )
			{
				k = findGlobalBone( set->hitbox[j].name );

				if( k != -1 )
				{
					set->hitbox[j].bone = k;
				}
				else
				{
					COM_FatalError( "cannot find bone %s for bbox\n", set->hitbox[j].name );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static float CalcPoseParameterValue( int control, Radian &angle, Vector &pos )
{
	switch( control )
	{
	case STUDIO_X:
		return pos.x;
	case STUDIO_Y:
		return pos.y;
	case STUDIO_Z:
		return pos.z;
	case STUDIO_XR:
		return RAD2DEG( angle.x );
	case STUDIO_YR:
		return RAD2DEG( angle.y );
	case STUDIO_ZR:
		return RAD2DEG( angle.z );
	}
	return 0.0f;
}

static void CalcPoseParameters( void )
{
	matrix3x4	boneToWorld[MAXSTUDIOBONES];
	Radian	angles;
	Vector	pos;

	for( int i = 0; i < g_numseq; i++ )
	{
		s_sequence_t *pseq = &g_sequence[i];

		for( int iPose = 0; iPose < 2; iPose++ )
		{
			if( pseq->groupsize[iPose] > 1 )
			{
				if( pseq->paramattachment[iPose] != -1 )
				{
					int j0 = pseq->paramindex[iPose];
					int n0 = pseq->paramattachment[iPose];
					int k0 = g_attachment[n0].bone;

					matrix3x4	boneToWorldRel;
					matrix3x4	boneToWorldMid;
					matrix3x4	worldToBoneMid;
					matrix3x4	boneRel;

					if( pseq->paramanim == NULL )
					{
						pseq->paramanim = g_panimation[0];
					}

					if( pseq->paramcompanim == NULL )
					{
						pseq->paramcompanim = pseq->paramanim;
					}

					// calculate what "zero" looks like to the attachment
					CalcBoneTransforms( pseq->paramanim, 0, boneToWorld );
					boneToWorldMid = boneToWorld[k0].ConcatTransforms( g_attachment[n0].local );
					boneToWorldMid.GetStudioTransform( pos, angles );
					worldToBoneMid = boneToWorldMid.Invert();

					MsgDev( D_REPORT, "%s : %s", pseq->name, g_pose[j0].name );

					// for 2D animation, figure out what opposite row/column to use
					// FIXME: make these 2D instead of 2 1D!
					bool found = false;
					int m[2];

					if( pseq->paramcenter != NULL )
					{
						for( int i0 = 0; !found && i0 < pseq->groupsize[0]; i0++ )
						{
							for( int i1 = 0; !found && i1 < pseq->groupsize[1]; i1++ )
							{
								int q = i0 + pseq->groupsize[0] * i1;
								if( pseq->panim[q] == pseq->paramcenter )
								{
									m[0] = i0;
									m[1] = i1;
									found = true;
								}
							}
						}
					}

					if( !found )
					{
						m[1-iPose] = (pseq->groupsize[1-iPose]) / 2;
					}

					// find changes to attachment
					for( m[iPose] = 0; m[iPose] < pseq->groupsize[iPose]; m[iPose]++ )
					{
						int q = m[0] + pseq->groupsize[0] * m[1];
						CalcBoneTransforms( pseq->panim[q], pseq->paramcompanim, 0, boneToWorld );
						boneToWorldRel = boneToWorld[k0].ConcatTransforms( g_attachment[n0].local );
						boneRel = worldToBoneMid.ConcatTransforms( boneToWorldRel );
						boneRel.GetStudioTransform( pos, angles );

						float v = CalcPoseParameterValue( pseq->paramcontrol[iPose], angles, pos );

						MsgDev( D_REPORT, " %6.2f", v );

						if( iPose == 0 )
						{
							pseq->param0[m[iPose]] = v;
						}
						else
						{
							pseq->param1[m[iPose]] = v;
						}

						if( m[iPose] == 0 )
						{
							pseq->paramstart[iPose] = (iPose == 0) ? pseq->param0[m[iPose]] : pseq->param1[m[iPose]];
						}

						if( m[iPose] == pseq->groupsize[iPose] - 1 )
						{
							pseq->paramend[iPose] = (iPose == 0) ? pseq->param0[m[iPose]] : pseq->param1[m[iPose]];
						}
					}

					MsgDev( D_REPORT, "\n" );

					if( fabs( pseq->paramstart[iPose] - pseq->paramend[iPose] ) < 0.01 )
					{
						COM_FatalError( "calcblend failed in %s\n", pseq->name );
					}

					g_pose[j0].min = Q_min( g_pose[j0].min, pseq->paramstart[iPose] );
					g_pose[j0].max = Q_max( g_pose[j0].max, pseq->paramstart[iPose] );
					g_pose[j0].min = Q_min( g_pose[j0].min, pseq->paramend[iPose] );
					g_pose[j0].max = Q_max( g_pose[j0].max, pseq->paramend[iPose] );
				}
				else
				{
					for( int m = 0; m < pseq->groupsize[iPose]; m++ )
					{
						float f = (m / (float)(pseq->groupsize[iPose] - 1.0));
						if( iPose == 0 )
						{
							pseq->param0[m] = pseq->paramstart[iPose] * (1.0 - f) + pseq->paramend[iPose] * f;
						}
						else
						{
							pseq->param1[m] = pseq->paramstart[iPose] * (1.0 - f) + pseq->paramend[iPose] * f;
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Process IK links
//-----------------------------------------------------------------------------
s_ikrule_t *FindPrevIKRule( s_animation_t *panim, int iRule )
{
	int i, j;

	s_ikrule_t *pRule = &panim->ikrule[iRule];

	for( i = 1; i < panim->numikrules; i++ )
	{
		j = ( iRule - i + panim->numikrules) % panim->numikrules;
		if( panim->ikrule[j].chain == pRule->chain )
			return &panim->ikrule[j];
	}

	return pRule;
}

s_ikrule_t *FindNextIKRule( s_animation_t *panim, int iRule )
{
	int i, j;

	s_ikrule_t *pRule = &panim->ikrule[iRule];

	for( i = 1; i < panim->numikrules; i++ )
	{
		j = (iRule + i ) % panim->numikrules;
		if( panim->ikrule[j].chain == pRule->chain )
			return &panim->ikrule[j];
	}

	return pRule;
}

//-----------------------------------------------------------------------------
// Link ikchains
//-----------------------------------------------------------------------------
static void LinkIKChains( void )
{
	int	i, k;

	// create IK links
	for( i = 0; i < g_numikchains; i++ )
	{
		g_ikchain[i].numlinks = 3;

		k = findGlobalBone( g_ikchain[i].bonename );
		if( k == -1 )
		{
			COM_FatalError( "unknown bone '%s' in ikchain '%s'\n", g_ikchain[i].bonename, g_ikchain[i].name );
		}

		g_ikchain[i].link[2].bone = k;
		g_bonetable[k].flags |= BONE_USED_BY_ATTACHMENT;

		k = g_bonetable[k].parent;
		if( k == -1 )
		{
			COM_FatalError("ikchain '%s' too close to root, no parent knee/elbow\n", g_ikchain[i].name );
		}

		g_ikchain[i].link[1].bone = k;
		g_bonetable[k].flags |= BONE_USED_BY_ATTACHMENT;

		k = g_bonetable[k].parent;
		if( k == -1 )
		{
			COM_FatalError("ikchain '%s' too close to root, no parent hip/shoulder\n", g_ikchain[i].name );
		}

		g_ikchain[i].link[0].bone = k;
		g_bonetable[k].flags |= BONE_USED_BY_ATTACHMENT;

		// FIXME: search for toes
	}
}

//-----------------------------------------------------------------------------
// Link ikchains
//-----------------------------------------------------------------------------
static void LinkIKLocks( void )
{
	int	i, j, k;

	// create IK links
	for( i = 0; i < g_numikautoplaylocks; i++ )
	{
		for( j = 0; j < g_numikchains; j++ )
		{
			if( !Q_stricmp( g_ikchain[j].name, g_ikautoplaylock[i].name ))
				break;
		}

		if( j == g_numikchains )
		{
			COM_FatalError( "unknown chain '%s' in ikautoplaylock\n", g_ikautoplaylock[i].name );
		}
		g_ikautoplaylock[i].chain = j;
	}

	for( k = 0; k < g_numseq; k++ )
	{
		for( i = 0; i < g_sequence[k].numiklocks; i++ )
		{
			for( j = 0; j < g_numikchains; j++ )
			{
				if( !Q_stricmp( g_ikchain[j].name, g_sequence[k].iklock[i].name ))
					break;
			}

			if( j == g_numikchains )
			{
				COM_FatalError( "unknown chain '%s' in sequence iklock\n", g_sequence[k].iklock[i].name );
			}
			g_sequence[k].iklock[i].chain = j;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: go through all the IK rules and calculate the animated path the IK'd 
//			end point moves relative to its IK target.
//-----------------------------------------------------------------------------
static void ProcessIKRules( void )
{
	int i, j, k;

	// copy source animations
	for( i = 0; i < g_numani; i++ )
	{
		s_animation_t *panim = g_panimation[i];
		const char *pAnimationName = g_panimation[i]->name;

		for( j = 0; j < panim->numcmds; j++ )
		{
			if( panim->cmds[j].cmd == CMD_IKFIXUP )
			{
				fixupIKErrors( panim, panim->cmds[j].ikfixup.pRule );
			}

			if( panim->cmds[j].cmd != CMD_IKRULE )
				continue;

			if( panim->numikrules >= MAXSTUDIOIKRULES )
			{
				COM_FatalError("Too many IK rules in %s (%s)\n", panim->name, panim->filename );
			}

			s_ikrule_t *pRule = &panim->ikrule[panim->numikrules++];

			// make a copy of the rule;
			*pRule = *panim->cmds[j].ikrule.pRule;
		}

		for( j = 0; j < panim->numikrules; j++ )
		{
			s_ikrule_t *pRule = &panim->ikrule[j];

			if( pRule->start == 0 && pRule->peak == 0 && pRule->tail == 0 && pRule->end == 0 )
			{
				pRule->tail = panim->numframes - 1;
				pRule->end = panim->numframes - 1;
			}

			if( pRule->start != -1 && pRule->peak == -1 && pRule->tail == -1 && pRule->end != -1 )
			{
				pRule->peak = (pRule->start + pRule->end) / 2;
				pRule->tail = (pRule->start + pRule->end) / 2;
			}

			if( pRule->start != -1 && pRule->peak == -1 && pRule->tail != -1 )
			{
				pRule->peak = (pRule->start + pRule->tail) / 2;
			}

			if( pRule->peak != -1 && pRule->tail == -1 && pRule->end != -1 )
			{
				pRule->tail = (pRule->peak + pRule->end) / 2;
			}

			if( pRule->peak == -1 )
			{
				pRule->start = 0;
				pRule->peak = 0;
			}

			if( pRule->tail == -1 )
			{
				pRule->tail = panim->numframes - 1;
				pRule->end = panim->numframes - 1;
			}

			if( pRule->contact == -1 )
			{
				pRule->contact = pRule->peak;
			}

			// huh, make up start and end numbers
			if( pRule->start == -1 )
			{
				s_ikrule_t *pPrev = FindPrevIKRule( panim, j );

				if( pPrev->slot == pRule->slot )
				{
					if( pRule->peak < pPrev->tail )
					{
						pRule->start = pRule->peak + (pPrev->tail - pRule->peak) / 2;
					}
					else
					{
						pRule->start = pRule->peak + (pPrev->tail - pRule->peak + panim->numframes - 1) / 2;
					}

					pRule->start = (pRule->start + panim->numframes / 2) % (panim->numframes - 1);
					pPrev->end = (pRule->start + panim->numframes - 1) % (panim->numframes - 1);
				}
				else
				{
					pRule->start = pPrev->tail;
					pPrev->end = pRule->peak;
				}
			}

			// huh, make up start and end numbers
			if( pRule->end == -1 )
			{
				s_ikrule_t *pNext = FindNextIKRule( panim, j );

				if( pNext->slot == pRule->slot )
				{
					if( pNext->peak < pRule->tail )
					{
						pNext->start = pNext->peak + (pRule->tail - pNext->peak) / 2;
					}
					else
					{
						pNext->start = pNext->peak + (pRule->tail - pNext->peak + panim->numframes - 1) / 2;
					}

					pNext->start = (pNext->start + panim->numframes / 2) % (panim->numframes - 1);
					pRule->end = (pNext->start + panim->numframes - 1) % (panim->numframes - 1);
				}
				else
				{
					pNext->start = pRule->tail;
					pRule->end = pNext->peak;
				}
			}

			// check for wrapping
			if( pRule->peak < pRule->start )
			{
				pRule->peak += panim->numframes - 1;
			}

			if( pRule->tail < pRule->peak )
			{
				pRule->tail += panim->numframes - 1;
			}

			if( pRule->end < pRule->tail )
			{
				pRule->end += panim->numframes - 1;
			}

			if( pRule->contact < pRule->start )
			{
				pRule->contact += panim->numframes - 1;
			}

			pRule->errorData.numerror = pRule->end - pRule->start + 1;
			if( pRule->end >= panim->numframes )
				pRule->errorData.numerror = pRule->errorData.numerror + 2;

			pRule->errorData.pError = (s_streamdata_t *)Mem_Alloc( pRule->errorData.numerror * sizeof( s_streamdata_t ));

			int n = 0;

			if( pRule->usesequence )
			{
				// FIXME: bah, this is horrendously hacky, add a damn back pointer
				for( n = 0; n < g_numseq; n++ )
				{
					if( g_sequence[n].panim[0] == panim )
						break;
				}
			}

			switch( pRule->type )
			{
			case IK_SELF:
				{
					matrix3x4	boneToWorld[MAXSTUDIOBONES];
					matrix3x4	worldToBone;
					matrix3x4	local;

					if( !Q_strlen( pRule->bonename ))
					{
						pRule->bone = -1;
					}
					else
					{
						pRule->bone = findGlobalBone( pRule->bonename );

						if( pRule->bone == -1 )
							COM_FatalError( "unknown bone '%s' in ikrule\n", pRule->bonename );
					}

					for( k = 0; k < pRule->errorData.numerror; k++ )
					{
						if( pRule->usesequence )
						{
							CalcSeqTransforms( n, k + pRule->start, boneToWorld );
						}
						else if( pRule->usesource )
						{
							matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
							BuildRawTransforms( panim, k + pRule->start + panim->startframe - panim->source.startframe, panim->adjust, panim->rotation, srcBoneToWorld );
							TranslateAnimations( panim->boneGlobalToLocal, srcBoneToWorld, boneToWorld );
						}
						else 
						{
							CalcBoneTransforms( panim, k + pRule->start, boneToWorld );
						}

						if( pRule->bone != -1 )
						{
							worldToBone = boneToWorld[pRule->bone].Invert();
							local = worldToBone.ConcatTransforms( boneToWorld[g_ikchain[pRule->chain].link[2].bone] );
						}
						else
						{
							local = boneToWorld[g_ikchain[pRule->chain].link[2].bone];
						}

						pRule->errorData.pError[k].q = local.GetQuaternion();
						pRule->errorData.pError[k].pos = local.GetOrigin();
					}
				}
				break;
			case IK_WORLD:
				break;
			case IK_ATTACHMENT:
				{
					matrix3x4	boneToWorld[MAXSTUDIOBONES];
					matrix3x4	worldToBone;
					matrix3x4	local;

					int bone = g_ikchain[pRule->chain].link[2].bone;
					CalcBoneTransforms( panim, pRule->contact, boneToWorld );
					// FIXME: add in motion

					if( !Q_strlen( pRule->bonename ))
					{
						if( pRule->bone != -1 )
						{
							pRule->bone = bone;
						}
					}
					else
					{
						pRule->bone = findGlobalBone( pRule->bonename );
						if( pRule->bone == -1 )
						{
							COM_FatalError( "unknown bone '%s' in ikrule\n", pRule->bonename );
						}
					}

					if( pRule->bone != -1 )
					{
						// FIXME: look for local bones...
						CalcBoneTransforms( panim, pRule->contact, boneToWorld );
						pRule->q = boneToWorld[pRule->bone].GetQuaternion();
						pRule->pos = boneToWorld[pRule->bone].GetOrigin();
					}

					for( k = 0; k < pRule->errorData.numerror; k++ )
					{
						int t = k + pRule->start;

						if( pRule->usesequence )
						{
							CalcSeqTransforms( n, t, boneToWorld );
						}
						else if( pRule->usesource )
						{
							matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
							BuildRawTransforms( panim, t + panim->startframe - panim->source.startframe, g_vecZero, g_radZero, srcBoneToWorld );
							TranslateAnimations( panim->boneGlobalToLocal, srcBoneToWorld, boneToWorld );
						}
						else 
						{
							CalcBoneTransforms( panim, t, boneToWorld );
						}

						Vector pos = pRule->pos + calcMovement( panim, t, pRule->contact );

						local = matrix3x4( pos, pRule->q );
						worldToBone = local.Invert();

						// calc position error
						local = worldToBone.ConcatTransforms( boneToWorld[bone] );
						pRule->errorData.pError[k].q = local.GetQuaternion();
						pRule->errorData.pError[k].pos = local.GetOrigin();
					}
				}
				break;
			case IK_GROUND:
				{
					matrix3x4	boneToWorld[MAXSTUDIOBONES];
					matrix3x4	worldToBone;
					matrix3x4	local;

					int bone = g_ikchain[pRule->chain].link[2].bone;

					if( pRule->usesequence )
					{
						CalcSeqTransforms( n, pRule->contact, boneToWorld );
					}
					else if (pRule->usesource)
					{
						matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
						BuildRawTransforms( panim, pRule->contact + panim->startframe, panim->adjust, panim->rotation, srcBoneToWorld );
						TranslateAnimations( panim->boneGlobalToLocal, srcBoneToWorld, boneToWorld );
					}
					else 
					{
						CalcBoneTransforms( panim, pRule->contact, boneToWorld );
					}

					// FIXME: add in motion

					Vector footfall = boneToWorld[bone].VectorTransform( g_ikchain[pRule->chain].center );
					footfall.z = pRule->floor;

					local = matrix3x4( footfall, g_radZero );
					worldToBone = local.Invert();

					pRule->pos = footfall;
					pRule->q = g_radZero;	// auto conversion Radian->Quaternion

					float s;

					for( k = 0; k < pRule->errorData.numerror; k++ )
					{
						int t = k + pRule->start;

						if( pRule->usesequence )
						{
							CalcSeqTransforms( n, t, boneToWorld );
						}
						else if( pRule->usesource )
						{
							matrix3x4	srcBoneToWorld[MAXSTUDIOSRCBONES];
							BuildRawTransforms( panim, pRule->contact + panim->startframe, panim->adjust, panim->rotation, srcBoneToWorld );
							TranslateAnimations( panim->boneGlobalToLocal, srcBoneToWorld, boneToWorld );
						}
						else 
						{
							CalcBoneTransforms( panim, t, boneToWorld );
						}

						Vector pos = pRule->pos + calcMovement( panim, t, pRule->contact );
						s = 0.0;

						Vector cur = boneToWorld[bone].VectorTransform( g_ikchain[pRule->chain].center );
						cur.z = pos.z;

						if( t < pRule->start || t >= pRule->end )
						{
							pos = cur;
						}
						else if( t < pRule->peak )
						{
							s = (float)(pRule->peak - t) / (pRule->peak - pRule->start);
							s = 3 * s * s - 2 * s * s * s;
							pos = pos * (1 - s) + cur * s;
						}
						else if( t > pRule->tail )
						{
							s = (float)(t - pRule->tail) / (pRule->end - pRule->tail);
							s = 3 * s * s - 2 * s * s * s;
							pos = pos * (1 - s) + cur * s;
						}

						local = matrix3x4( pos, pRule->q );
						worldToBone = local.Invert();

						// calc position error
						local = worldToBone.ConcatTransforms( boneToWorld[bone] );
						pRule->errorData.pError[k].q = local.GetQuaternion();
						pRule->errorData.pError[k].pos = local.GetOrigin();
					}
				}
				break;
			case IK_RELEASE:
			case IK_UNLATCH:
				break;
			}
		}

		if( FBitSet( panim->flags, STUDIO_DELTA ) || panim->noAutoIK )
			continue;

		// auto release ik chains that are moved but not referenced and have no explicit rules
		int count[16];

		for( j = 0; j < g_numikchains; j++ )
		{
			count[j] = 0;
		}

		for( j = 0; j < panim->numikrules; j++ )
		{
			count[panim->ikrule[j].chain]++;
		}

		for( j = 0; j < g_numikchains; j++ )
		{
			if( count[j] == 0 && panim->weight[g_ikchain[j].link[2].bone] > 0.0f )
			{
				k = panim->numikrules++;
				panim->ikrule[k].chain = j;
				panim->ikrule[k].slot = j;
				panim->ikrule[k].type = IK_RELEASE;
				panim->ikrule[k].start = 0;
				panim->ikrule[k].peak = 0;
				panim->ikrule[k].tail = panim->numframes - 1;
				panim->ikrule[k].end = panim->numframes - 1;
			}
		}
	}

	// realign IK across multiple animations
	for( i = 0; i < g_numseq; i++ )
	{
		for( j = 0; j < g_sequence[i].groupsize[0]; j++ )
		{
			for( k = 0; k < g_sequence[i].groupsize[1]; k++ )
			{
				int q = j + g_sequence[i].groupsize[0] * k;
				g_sequence[i].numikrules = Q_max( g_sequence[i].numikrules, g_sequence[i].panim[q]->numikrules );
			}
		}

		// check for mismatched ik rules
		s_animation_t *panim1 = g_sequence[i].panim[0];
		for( j = 0; j < g_sequence[i].groupsize[0]; j++ )
		{
			for( k = 0; k < g_sequence[i].groupsize[1]; k++ )
			{
				int q = j + g_sequence[i].groupsize[0] * k;
				s_animation_t *panim2 = g_sequence[i].panim[q];
				if( panim1->numikrules != panim2->numikrules )
				{
					COM_FatalError( "%s - mismatched number of IK rules: \"%s\" \"%s\"\n", g_sequence[i].name, panim1->name, panim2->name );
				}

				for( int n = 0; n < panim1->numikrules; n++ )
				{
					if(( panim1->ikrule[n].type != panim2->ikrule[n].type ) ||
					( panim1->ikrule[n].chain != panim2->ikrule[n].chain ) ||
					( panim1->ikrule[n].slot != panim2->ikrule[n].slot ))
					{
						COM_FatalError( "%s - mismatched IK rule %d: \n\"%s\" : %d %d %d\n\"%s\" : %d %d %d\n", 
						g_sequence[i].name, n, 
						panim1->name, panim1->ikrule[n].type, panim1->ikrule[n].chain, panim1->ikrule[n].slot,
						panim2->name, panim2->ikrule[n].type, panim2->ikrule[n].chain, panim2->ikrule[n].slot );
					}
				}
			}
		}

		// FIXME: this doesn't check alignment!!!
		for( j = 0; j < g_sequence[i].groupsize[0]; j++ )
		{
			for( k = 0; k < g_sequence[i].groupsize[1]; k++ )
			{
				int q = j + g_sequence[i].groupsize[0] * k;
				for( int n = 0; n < g_sequence[i].panim[q]->numikrules; n++ )
				{
					g_sequence[i].panim[q]->ikrule[n].index = n;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Compress all the IK data
//-----------------------------------------------------------------------------
static void CompressIKErrors( void )
{
	// find scales for all bones
	for( int i = 0; i < g_numani; i++ )
	{
		for( int j = 0; j < g_panimation[i]->numikrules; j++ )
		{
			s_ikrule_t *pRule = &g_panimation[i]->ikrule[j];

			if( pRule->errorData.numerror == 0 )
				continue;

			CompressSingle( &pRule->errorData );
		}
	}
}

void SimplifyModel( void )
{
	if( g_numseq == 0 )
		COM_FatalError( "model has no sequences\n" );

	RemapBones( );

	LinkIKChains();

	LinkIKLocks();

	RealignBones( );

	RemapVertices( );

	BuildVertexArrays( );

	RemapAnimations( );

	processAnimations( );

	OptimizeAnimations( );	// FIXME: remove

	limitBoneRotations();

	limitIKChainLength();

	RemapProceduralBones( );

	MakeTransitions( );

	FindAutolayers();

	LinkBoneControllers( );

	// link screen aligned bones
	TagScreenAlignedBones( );

	LinkAttachments( );

	// procedural bone needs to propagate its bone usage up its chain
	// ensures runtime sets up dependent bone hierarchy
	MarkProceduralBoneChain( );

	ProcessIKRules();

	CompressIKErrors( );

	CalcPoseParameters();

	SetupHitBoxes( );

	CompressAnimations( );

	CalcSequenceBoundingBoxes( );

	// auto groups
	if( g_numseqgroups == 1 && maxseqgroupsize < 1024 * 1024 ) 
	{	
		int	i, j, k, q;
		int	current = 0;

		g_numseqgroups = 2;

		for( i = 0; i < g_numseq; i++ )
		{
			int accum = 0;

			if( g_sequence[i].activity == 0 )
			{
				for( q = 0; q < g_sequence[i].numblends; q++ )
				{
					for( j = 0; j < g_numbones; j++ )
					{
						for( k = 0; k < 6; k++ )
						{
							accum += g_sequence[i].panim[q]->numanim[j][k] * sizeof( mstudioanimvalue_t );
						}
					}
				}

				accum += g_sequence[i].numblends * g_numbones * sizeof( mstudioanim_t );
			
				if( current && current + accum > maxseqgroupsize )
				{
					g_numseqgroups++;
					current = accum;
				}
				else
				{
					current += accum;
				}
				// printf("%d %d %d\n", g_numseqgroups, current, accum );
				g_sequence[i].seqgroup = g_numseqgroups - 1;
			}
			else
			{
				g_sequence[i].seqgroup = 0;
			}
		}
	}
}