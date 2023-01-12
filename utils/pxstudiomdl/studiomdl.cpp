/*
studiomodel.cpp - generates a studio .mdl file from a .qc script
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

#include "port.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "scriplib.h"
#include "filesystem.h"
#define EXTERN
#include "studio.h"
#include "studiomdl.h"
#include "activity.h"
#include "activitymap.h"
#include "crashhandler.h"
#include "app_info.h"
#include "build_info.h"

CUtlArray< char >	g_KeyValueText;
char		filename[1024];
char		line[1024];
int		linecount;
FILE		*input;

//-----------------------------------------------------------------------------
// Parsed data from a .qc file
//-----------------------------------------------------------------------------
/*
=================
ClearModel

FIXME: finalize this if you want to compile models in cycle
=================
*/
void ClearModel( void )
{
	int	i, j, k;

	// release memory occupied by animations
	for( i = 0; i < g_real_numani; i++ )
	{
		// we never know how many frames are used
		for( j = 0; j < MAXSTUDIOANIMATIONS; j++ )
		{
			if( g_panimation[i]->rawanim[j] )
				Mem_Free( g_panimation[i]->rawanim[j] );

			if( g_panimation[i]->sanim[j] )
				Mem_Free( g_panimation[i]->sanim[j] );
		}

		for( j = 0; j < g_real_numbones; j++ )
		{
			for( k = 0; k < 6; k++ )
			{
				Mem_Free( g_panimation[i]->anim[j][k] );
			}
		}

		for( j = 0; j < g_panimation[i]->numcmds; j++ )
		{
			s_animcmd_t *pcmd = &g_panimation[i]->cmds[j];

			if( pcmd->cmd == CMD_IKRULE && pcmd->cmd_source == CMDSRC_LOCAL )
				Mem_Free( pcmd->ikrule.pRule );

			if( pcmd->cmd == CMD_IKFIXUP && pcmd->cmd_source == CMDSRC_LOCAL )
				Mem_Free( pcmd->ikfixup.pRule );
		}

		for( j = 0; j < g_panimation[i]->numikrules; j++ )
		{
			s_ikrule_t *ikrule = &g_panimation[i]->ikrule[j];

			Mem_Free( ikrule->errorData.pError );

			for( k = 0; k < 6; k++ )
				Mem_Free( ikrule->errorData.anim[k] );
		}

		Mem_Free( g_panimation[i] );
	}

	for( i = 0; i < g_numcmdlists; i++ )
	{
		s_cmdlist_t *cmdlist = &g_cmdlist[i];

		for( j = 0; j < cmdlist->numcmds; j++ )
		{
			s_animcmd_t *pcmd = &cmdlist->cmds[j];

			if( pcmd->cmd == CMD_IKRULE && pcmd->cmd_source == CMDSRC_GLOBAL )
				Mem_Free( pcmd->ikrule.pRule );
			if( pcmd->cmd == CMD_IKFIXUP && pcmd->cmd_source == CMDSRC_GLOBAL )
				Mem_Free( pcmd->ikfixup.pRule );			
		}
	}

	memset( g_cmdlist, 0, sizeof( g_cmdlist ));
	g_numcmdlists = 0;

	for( i = 0; i < g_nummodels; i++ )
	{
		for( j = 0; j < g_model[i]->nummesh; j++ )
		{
			Mem_Free( g_model[i]->pmesh[j]->triangle );
			Mem_Free( g_model[i]->pmesh[j] ); // iteslf
		}

		Mem_Free( g_model[i] );
	}

	for( i = 0; i < g_real_numseq; i++ )
	{
		g_sequence[i].KeyValue.Purge();
	}

	for( i = 1; i <= g_numxnodes; i++ )
		Mem_Free( g_xnodename[i] );

	for( i = 0; i < g_numtextures; i++ )
		Mem_Free( g_texture[i].pdata );

	memset( g_xnode, 0, sizeof( g_xnode ));
	memset( g_xnodename, 0, sizeof( g_xnodename ));
	memset( g_xnodeskip, 0, sizeof( g_xnodeskip ));
	g_numxnodes = g_numxnodeskips = 0;

	memset( g_texture, 0, sizeof( g_texture ));
	g_numtextures = 0;

	memset( g_sequencegroup, 0, sizeof( g_sequencegroup ));
	g_numseqgroups = 0;

	memset( g_sequence, 0, sizeof( g_sequence ));
	g_real_numseq = 0;
	g_numseq = 0;

	memset( g_model, 0, sizeof( g_model ));
	g_nummodels = 0;

	memset( g_panimation, 0, sizeof( g_panimation ));
	g_real_numani = 0;
	g_numani = 0;

	memset( g_pose, 0, sizeof( g_pose ));
	g_numposeparameters = 0;

	memset( g_bonetable, 0, sizeof( g_bonetable ));
	g_real_numbones = 0;
	g_numbones = 0;

	memset( g_renamedbone, 0, sizeof( g_renamedbone ));
	g_numrenamedbones = 0;

	memset( g_importbone, 0, sizeof( g_importbone ));
	g_numimportbones = 0;

	memset( g_hitgroup, 0, sizeof( g_hitgroup ));
	g_numhitgroups = 0;

	memset( g_bonecontroller, 0, sizeof( g_bonecontroller ));
	g_numbonecontrollers = 0;

	memset( g_screenalignedbone, 0, sizeof( g_screenalignedbone ));
	g_numscreenalignedbones = 0;

	memset( g_attachment, 0, sizeof( g_attachment ));
	g_numattachments = 0;

	g_hitboxsets.Purge();

	g_BoneMerge.Purge();
}

bool GetLineInput( void )
{
	while( fgets( line, sizeof( line ), input ) != NULL ) 
	{
		linecount++;
		// skip comments
		if( line[0] == '/' && line[1] == '/' )
			continue;
		return true;
	}

	return false;
}

bool IsEnd( char const *pLine )
{
	if( !Q_strncmp( "end", pLine, 3 )) 
		return true;
	return ( pLine[3] == '\0' ) || ( pLine[3] == '\n' );
}

int verify_atoi_dbg( const char *token, const int line )
{
	if( token[0] != '-' && ( token[0] < '0' || token[0] > '9' ))
	{
		TokenError( "expecting number, got \"%s\"\ncalled at line %d", token, line );
	}
	return atoi( token );
}

float verify_atof_dbg( const char *token, const int line )
{
	if( token[0] != '-' && token[0] != '.' && ( token[0] < '0' || token[0] > '9' ))
	{
		TokenError( "expecting number, got \"%s\"\ncalled at line %d", token, line );
	}
	return atof( token );
}

float verify_atof_with_null( const char *token )
{
	if( !Q_strcmp( token, ".." ))
		return -1;

	return verify_atof( token );
}

//-----------------------------------------------------------------------------
// Key value block
//-----------------------------------------------------------------------------
static void AppendKeyValueText( CUtlArray< char > *pKeyValue, const char *pString )
{
	int nLen = Q_strlen( pString );
	int nFirst = pKeyValue->AddMultipleToTail( nLen );
	memcpy( pKeyValue->Base() + nFirst, pString, nLen );
}

int KeyValueTextSize( CUtlArray< char > *pKeyValue )
{
	return pKeyValue->Count();
}

const char *KeyValueText( CUtlArray< char > *pKeyValue )
{
	return pKeyValue->Base();
}

const char *RenameBone( const char *pName )
{
	for( int k = 0; k < g_numrenamedbones; k++ )
	{
		if( !Q_stricmp( pName, g_renamedbone[k].from ))
			return g_renamedbone[k].to;
	}

	return pName;
}

bool IsGlobalBoneXSI( const char *name, const char *bonename )
{
	name = RenameBone( name );

	int	len = Q_strlen( name );
	int	len2 = Q_strlen( bonename );

	if( len2 == len && Q_strchr( bonename, '.' ) == NULL && !Q_stricmp( bonename, name ))
		return true;

	if( len2 > len )
	{
		if( bonename[len2-len-1] == '.' )
		{
			if( !Q_stricmp( &bonename[len2-len], name ))
			{
				return true;
			}
		}
	}
	return false;
}

int findGlobalBone( const char *name )
{
	name = RenameBone( name );

	for( int k = 0; k < g_numbones; k++ )
	{
		if( !Q_stricmp( g_bonetable[k].name, name ))
			return k;
	}

	return -1;
}

int findLocalBone( const s_animation_t *panim, const char *name )
{
	if( name )
	{
		int	i;

		for( i = 0; i < panim->numbones; i++ )
		{
			if( !Q_stricmp( name, panim->localBone[i].name ))
				return i;
		}

		name = RenameBone( name );

		for( i = 0; i < panim->numbones; i++ )
		{
			if( !Q_stricmp( name, panim->localBone[i].name ))
				return i;
		}
	}

	return -1;
}

int findGlobalBoneXSI( const char *name )
{
	name = RenameBone( name );

	for( int k = 0; k < g_numbones; k++ )
	{
		if( IsGlobalBoneXSI( name, g_bonetable[k].name ))
			return k;
	}
	
	return -1;
}

int SortAndBalanceBones( int iCount, int iMaxCount, int bones[], float weights[] )
{
	int	i, bShouldSort;
	float	w, t;

	// collapse duplicate bone weights
	for( i = 0; i < iCount-1; i++ )
	{
		for( int j = i + 1; j < iCount; j++ )
		{
			if( bones[i] == bones[j] )
			{
				weights[i] += weights[j];
				weights[j] = 0.0;
			}
		}
	}

	// do sleazy bubble sort
	do {
		bShouldSort = false;
		for( i = 0; i < iCount-1; i++ )
		{
			if( weights[i+1] > weights[i] )
			{
				int j = bones[i+1];
				bones[i+1] = bones[i];
				bones[i] = j;
				w = weights[i+1];
				weights[i+1] = weights[i];
				weights[i] = w;
				bShouldSort = true;
			}
		}
	} while( bShouldSort );

	// throw away all weights less than 1/20th
	while( iCount > 1 && weights[iCount-1] < 0.005 )
		iCount--;

	// clip to the top iMaxCount bones
	if( iCount > iMaxCount )
		iCount = iMaxCount;

	t = 0.0f;

	for( i = 0; i < iCount; i++ )
		t += weights[i];

	if( t <= 0.0f )
	{
		// missing weights?, go ahead and evenly share?
		// FIXME: shouldn't this error out?
		t = 1.0 / iCount;

		for( i = 0; i < iCount; i++ )
			weights[i] = t;
	}
	else
	{
		// scale to sum to 1.0
		t = 1.0 / t;

		for( i = 0; i < iCount; i++ )
			weights[i] = weights[i] * t;
	}

	return iCount;
}

int LookupControl( const char *string )
{
	if( !Q_stricmp( string, "X" )) return STUDIO_X;
	if( !Q_stricmp( string, "Y" )) return STUDIO_Y;
	if( !Q_stricmp( string, "Z" )) return STUDIO_Z;
	if( !Q_stricmp( string, "XR" )) return STUDIO_XR;
	if( !Q_stricmp( string, "YR" )) return STUDIO_YR;
	if( !Q_stricmp( string, "ZR" )) return STUDIO_ZR;

	if( !Q_stricmp( string, "LX" )) return STUDIO_LX;
	if( !Q_stricmp( string, "LY" )) return STUDIO_LY;
	if( !Q_stricmp( string, "LZ" )) return STUDIO_LZ;
	if( !Q_stricmp( string, "LXR" )) return STUDIO_LXR;
	if( !Q_stricmp( string, "LYR" )) return STUDIO_LYR;
	if( !Q_stricmp( string, "LZR" )) return STUDIO_LZR;

	if( !Q_stricmp( string, "LM" )) return STUDIO_LINEAR;
	if( !Q_stricmp( string, "LQ" )) return STUDIO_QUADRATIC_MOTION;

	return -1;
}

int LookupPoseParameter( const char *name )
{
	int	i;

	for( i = 0; i < g_numposeparameters; i++)
	{
		if( !Q_stricmp( name, g_pose[i].name ))
			return i;
	}

	Q_strncpy( g_pose[i].name, name, sizeof( g_pose[0].name ));
	g_numposeparameters = i + 1;

	if( g_numposeparameters > MAXSTUDIOPOSEPARAM )
		TokenError( "too many pose parameters (max %d)\n", MAXSTUDIOPOSEPARAM );

	return i;
}

s_sequence_t *LookupSequence( const char *name )
{
	for( int i = 0; i < g_numseq; i++ )
	{
		if ( !Q_stricmp( g_sequence[i].name, name ))
			return &g_sequence[i];
	}

	return NULL;
}

s_animation_t *LookupAnimation( const char *name )
{
	for( int i = 0; i < g_numani; i++ )
	{
		if( !Q_stricmp( g_panimation[i]->name, name ))
			return g_panimation[i];
	}

	s_sequence_t *pseq = LookupSequence( name );
	return pseq ? pseq->panim[0] : NULL;
}

int LookupAttachment( const char *name )
{
	for( int i = 0; i < g_numattachments; i++ )
	{
		if( !Q_stricmp( g_attachment[i].name, name ))
			return i;
	}

	return -1;
}

int LookupTexture( const char *texturename )
{
	int	i;

	for( i = 0; i < g_numtextures; i++ )
	{
		if( !Q_stricmp( g_texture[i].name, texturename ))
			return i;
	}

	Q_strncpy( g_texture[i].name, texturename, sizeof( g_texture[0].name ));

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
	if( Q_stristr( texturename, "chrome" ) != NULL )
		g_texture[i].flags |= STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	if( Q_stristr( texturename, "bright" ) != NULL )
		g_texture[i].flags |= STUDIO_NF_FULLBRIGHT;
	g_numtextures++;

	return i;
}

s_mesh_t *LookupMesh( s_model_t *pmodel, char *texturename )
{
	int	i, j;

	j = LookupTexture( texturename );

	for( i = 0; i < pmodel->nummesh; i++ )
	{
		if( pmodel->pmesh[i]->skinref == j )
			return pmodel->pmesh[i];
	}
	
	if( i >= MAXSTUDIOMESHES )
		COM_FatalError( "too many meshes in model: \"%s\"\n", pmodel->name );

	pmodel->nummesh = i + 1;
	pmodel->pmesh[i] = (s_mesh_t *)Mem_Alloc( sizeof( s_mesh_t ));
	pmodel->pmesh[i]->skinref = j;

	return pmodel->pmesh[i];
}

s_trianglevert_t *LookupTriangle( s_mesh_t *pmesh, int index )
{
	if( index >= MAXSTUDIOTRIANGLES )
		COM_FatalError( "max studio triangles exceeds 65536\n" );

	if( index >= pmesh->alloctris )
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;

		if( pmesh->triangle )
		{
			pmesh->triangle = (s_trianglevert_t (*)[3])Mem_Realloc( pmesh->triangle, pmesh->alloctris * sizeof( *pmesh->triangle ));
			memset( &pmesh->triangle[start], 0, ( pmesh->alloctris - start ) * sizeof( *pmesh->triangle ));
		} 
		else
		{
			pmesh->triangle = (s_trianglevert_t (*)[3])Mem_Alloc( pmesh->alloctris * sizeof( *pmesh->triangle ));
		}
	}

	return pmesh->triangle[index];
}

int LookupNormal( s_model_t *pmodel, s_srcvertex_t *srcv )
{
	for (int i = 0; i < pmodel->norm.Count(); i++)
	{
		if( DotProduct( pmodel->norm[i].org, srcv->norm ) > g_normal_blend && pmodel->norm[i].skinref == srcv->skinref
			&& !memcmp( &pmodel->norm[i].globalWeight, &srcv->globalWeight, sizeof( s_boneweight_t ))) {
			return i;
		}
	}
	
	int k = pmodel->norm.AddToTail();
	pmodel->norm[k].org = srcv->norm;
	pmodel->norm[k].globalWeight = srcv->globalWeight;
	pmodel->norm[k].skinref = srcv->skinref;

	if (k == MAXSTUDIOVERTS) {
		MsgDev(D_WARN, "exceed MAXSTUDIOVERTS limit in model: \"%s\"\nModel may not work with some mods or software (except PrimeXT)\n", pmodel->name);
	}

	return k;
}

int LookupVertex( s_model_t *pmodel, s_srcvertex_t *srcv )
{
	// assume 3 digits of accuracy
	srcv->vert.x = (int)(srcv->vert.x * 1000) / 1000.0;
	srcv->vert.y = (int)(srcv->vert.y * 1000) / 1000.0;
	srcv->vert.z = (int)(srcv->vert.z * 1000) / 1000.0;

	for (int i = 0; i < pmodel->vert.Count(); i++)
	{
		if( pmodel->vert[i].org == srcv->vert && !memcmp( &pmodel->vert[i].globalWeight, &srcv->globalWeight, sizeof( s_boneweight_t )))
			return i;
	}

	int k = pmodel->vert.AddToTail();
	pmodel->vert[k].org = srcv->vert;
	pmodel->vert[k].globalWeight = srcv->globalWeight;

	if (k == MAXSTUDIOVERTS) {
		MsgDev(D_WARN, "exceed MAXSTUDIOVERTS limit in model: \"%s\"\nModel may not work with some mods or software (except PrimeXT)\n", pmodel->name);
	}

	return k;
}

int LookupActivity( const char *szActivity )
{
	int	i;

	for( i = 0; activity_map[i].name; i++ )
	{
		if( !Q_stricmp( szActivity, activity_map[i].name ))
			return activity_map[i].type;
	}

	// match ACT_#
	if( !Q_strnicmp( szActivity, "ACT_", 4 ))
		return verify_atoi( &szActivity[4] );

	return 0;
}

int LookupXNode( const char *name )
{
	int	i;

	// FIXME: check transitions in-game
	if( Q_isdigit( name ))
		return verify_atoi( name );

	for( i = 1; i <= g_numxnodes; i++ )
	{
		if( !Q_stricmp( name, g_xnodename[i] ))
			return i;
	}

	g_xnodename[i] = strdup( name );
	g_numxnodes = i;

	return i;
}

void clip_rotations( float &rot )
{
	// clip everything to : -M_PI <= x < M_PI
	while( rot >= M_PI ) rot -= M_PI * 2.0f;
	while( rot < -M_PI ) rot += M_PI * 2.0f;
}

void clip_rotations( Radian &rot )
{
	clip_rotations( rot.x );
	clip_rotations( rot.y );
	clip_rotations( rot.z );
}

void scale_animation( s_animation_t *panim )
{
	for( int t = 0; t < panim->source.numframes; t++ )
	{
		for( int j = 0; j < panim->numbones; j++ )
			panim->rawanim[t][j].pos *= panim->scale;
	}
}

void Build_Reference( s_model_t *pmodel )
{
	for( int i = 0; i < pmodel->numbones; i++ )
	{
		matrix3x4	bonematrix = matrix3x4( pmodel->skeleton[i].pos, pmodel->skeleton[i].rot );
		int parent = pmodel->localBone[i].parent;

		if( parent == -1 )
		{
			// scale the done pos.
			// calc rotational matrices
			pmodel->boneToPose[i] = bonematrix;

		}
		else
		{
			// calc compound rotational matrices
			pmodel->boneToPose[i] = pmodel->boneToPose[parent].ConcatTransforms( bonematrix );
		}
	}
}

void Grab_Triangles( s_model_t *pmodel )
{
	int	i, j, k;
	int	ncount = 0;
	float	vmin = 9999.0f;

	// load the base triangles
	while( 1 ) 
	{
		if( !GetLineInput( )) 
			break;

		// check for end
		if( IsEnd( line )) 
			break;

		char		texturename[64];
		s_mesh_t		*pmesh;
		s_trianglevert_t	*ptriv;
		int		bone;
		Vector		vert[3];
		Vector		norm[3];

		Q_strncpy( texturename, line, sizeof( texturename ));

		// strip off trailing smag
		for( i = Q_strlen( texturename ) - 1; i >= 0 && !isgraph( texturename[i] ); i-- );
		texturename[i + 1] = '\0';

		// funky texture overrides
		for( i = 0; i < numrep; i++ )  
		{
			if( sourcetexture[i][0] == '\0' ) 
			{
				Q_strncpy( texturename, defaulttexture[i], sizeof( texturename ));
				break;
			}

			if( !Q_stricmp( texturename, sourcetexture[i] )) 
			{
				Q_strncpy( texturename, defaulttexture[i], sizeof( texturename ));
				break;
			}
		}

		if( texturename[0] == '\0' )
		{
			// weird model problem, skip them
			GetLineInput();
			GetLineInput();
			GetLineInput();
			continue;
		}

		if( Q_stristr( texturename, "null.bmp" ) || Q_stristr( texturename, "null.tga" ))
		{
			// skip all faces with the null texture on them.
			GetLineInput();
			GetLineInput();
			GetLineInput();
			continue;
		}

		COM_DefaultExtension( texturename, ".tga" ); // Crowbar decompiler issues
		pmesh = LookupMesh( pmodel, texturename );

		for( j = 0; j < 3; j++ ) 
		{
			if( pmodel->flip_triangles )
				ptriv = LookupTriangle( pmesh, pmesh->numtris ) + 2 - j; // quake wants them in the reverse order
			else ptriv = LookupTriangle( pmesh, pmesh->numtris ) + j;

			if( !GetLineInput( )) 
			{
				COM_FatalError( "%s: error on line %d: %s", filename, linecount, line );
			}

			pmodel->srcvert.AddToTail();
			s_srcvertex_t	*srcv = &pmodel->srcvert[pmodel->srcvert.Count() - 1];
			int		iCount = 0, bones[MAXSTUDIOSRCBONES];
			float		weights[MAXSTUDIOSRCBONES];
			s_boneweight_t	boneWeight;

			// clean memory before use to avoid bug
			memset(srcv, 0, sizeof(*srcv));

			// get support for Source bone weights description
			i = sscanf( line, "%d %f %f %f %f %f %f %f %f %d %d %f %d %f %d %f %d %f",
			&bone, 
			&srcv->vert[0], &srcv->vert[1], &srcv->vert[2], 
			&srcv->norm[0], &srcv->norm[1], &srcv->norm[2], 
			&ptriv->u, &ptriv->v,
			&iCount,
			&bones[0], &weights[0], &bones[1], &weights[1], &bones[2], &weights[2], &bones[3], &weights[3] );

			if( i < 9 ) continue; 

			if( bone < 0 || bone >= pmodel->numbones ) 
			{
				COM_FatalError( "bogus bone index\n%d %s :\n%s", linecount, filename, line );
			}

			// continue parsing more bones.
			if( iCount > MAXSTUDIOBONEWEIGHTS )
			{
				char	*token;
				int	ctr = 0;

				for( k = 0; k < 18; k++ )
				{
					while( line[ctr] == ' ' )
					{
						ctr++;
					}

					token = strtok( &line[ctr], " " );
					ctr += Q_strlen( token ) + 1;
				}

				for( k = 4; k < iCount && k < MAXSTUDIOSRCBONES; k++ )
				{
					while( line[ctr] == ' ' )
					{
						ctr++;
					}

					token = strtok( &line[ctr], " " );
					ctr += Q_strlen( token ) + 1;

					bones[k] = verify_atoi( token );

					token = strtok( &line[ctr], " " );
					ctr += strlen( token ) + 1;
			
					weights[k] = verify_atof( token );
				}
			}

			vmin = Q_min( srcv->vert.z, vmin );
			srcv->skinref = pmesh->skinref;
			srcv->vert *= pmodel->scale;

			vert[j] = srcv->vert;
			norm[j] = srcv->norm;

			// initialize boneweigts
			for( k = 0; k < MAXSTUDIOBONEWEIGHTS; k++ )
			{
				boneWeight.weight[k] = 0.0f;
				boneWeight.bone[k] = -1;
			}

			if( i == 9 || iCount == 0 )
			{
				boneWeight.weight[0] = 1.0f;
				boneWeight.bone[0] = bone;
				boneWeight.numbones = 1;
			}
			else
			{
				iCount = SortAndBalanceBones( iCount, MAXSTUDIOBONEWEIGHTS, bones, weights );

				if( allow_boneweights )
				{
					for( k = 0; k < iCount; k++ )
					{
						boneWeight.bone[k] = bound( 0, bones[k], MAXSTUDIOBONES - 1 );
						boneWeight.weight[k] = weights[k];
					}

					boneWeight.numbones = iCount;
					has_boneweights = true;
				}
				else
				{
					boneWeight.bone[0] = bones[0];
					boneWeight.weight[0] = 1.0f;
					boneWeight.numbones = 1;
				}
			}

			srcv->localWeight = boneWeight;
			ptriv->vertindex = ptriv->normindex = pmodel->srcvert.Count() - 1;

			// tag bone as being used
			// pmodel->bone[bone].ref = 1;
		}

		if( tag_reversed || tag_normals )
		{
			// check triangle direction
			if( DotProduct( norm[0], norm[1] ) < 0 || DotProduct( norm[1], norm[2] ) < 0 || DotProduct( norm[2], norm[0] ) < 0 )
			{
				ncount++;

				if( tag_normals ) 
				{
					// steal the triangle and make it white
					s_trianglevert_t	*ptriv2;

					pmesh = LookupMesh( pmodel, "#white.bmp" );
					ptriv2 = LookupTriangle( pmesh, pmesh->numtris );

					ptriv2[0] = ptriv[0];
					ptriv2[1] = ptriv[1];
					ptriv2[2] = ptriv[2];
				}
			} 
			else 
			{
				Vector	a1, a2, sn;
				float	x, y, z;

				a1 = vert[1] - vert[0];
				a2 = vert[2] - vert[0];
				sn = CrossProduct( a1, a2 ).Normalize();

				x = DotProduct( sn, norm[0] );
				y = DotProduct( sn, norm[1] );
				z = DotProduct( sn, norm[2] );

				if( x < 0.0 || y < 0.0 || z < 0.0 ) 
				{
					if( tag_reversed ) 
					{
						// steal the triangle and make it white
						s_trianglevert_t	*ptriv2;

						MsgDev( D_INFO, "triangle reversed (%f %f %f)\n",
							DotProduct( norm[0], norm[1] ),
							DotProduct( norm[1], norm[2] ),
							DotProduct( norm[2], norm[0] ));

						pmesh = LookupMesh( pmodel, "#white.bmp" );
						ptriv2 = LookupTriangle( pmesh, pmesh->numtris );

						ptriv2[0] = ptriv[0];
						ptriv2[1] = ptriv[1];
						ptriv2[2] = ptriv[2];
					}
				}
			}
		}

		pmesh->numtris++;
	}

	if( ncount ) MsgDev( D_WARN, "%d triangles with misdirected normals\n", ncount );
	if( vmin != 0.0 ) MsgDev( D_REPORT, "lowest vector at %f\n", vmin );
}

bool Grab_AnimFrames( s_animation_t *panim )
{
	Vector	pos;
	Radian	rot;
	char	cmd[1024];
	int	index, size;
	int	t = -99999999;

	size = panim->numbones * sizeof( s_bone_t );
	panim->source.startframe = -1;
	panim->source.endframe = 0;

	while( GetLineInput( ))
	{
		if( sscanf( line, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] ) == 7 )
		{
			if( panim->source.startframe < 0 )
				COM_FatalError( "missing frame start(%d) : %s\n", linecount, line );

			panim->rawanim[t][index].pos = pos;
			panim->rawanim[t][index].rot = rot;
			continue;
		}

		if( sscanf( line, "%1023s %d", cmd, &index ) == 0 )
		{
			COM_FatalError( "(%d) : %s", linecount, line );
			continue;
		}

		if( !Q_stricmp( cmd, "time" )) 
		{
			t = index;

			if( panim->source.startframe == -1 )
				panim->source.startframe = t;

			if( t < panim->source.startframe )
				COM_FatalError( "frame error(%d) : %s\n", linecount, line );

			if( t > panim->source.endframe )
				panim->source.endframe = t;

			t -= panim->source.startframe;

			if( t > MAXSTUDIOANIMFRAMES )
			{
				MsgDev( D_ERROR, "animation %s has too many frames. Cutted at %d\n", panim->name, MAXSTUDIOANIMFRAMES );
				panim->source.numframes = MAXSTUDIOANIMFRAMES - 1;
				panim->source.endframe = MAXSTUDIOANIMFRAMES - 1;
				return false;
			}

			if( panim->rawanim[t] != NULL )
				continue;

			panim->rawanim[t] = (s_bone_t *)Mem_Alloc( size );

			// duplicate previous frames keys
			if( t > 0 && panim->rawanim[t-1] )
			{
				for( int j = 0; j < panim->numbones; j++ )
				{
					panim->rawanim[t][j].pos = panim->rawanim[t-1][j].pos;
					panim->rawanim[t][j].rot = panim->rawanim[t-1][j].rot;
				}
			}
			continue;
		}

		if( !Q_stricmp( cmd, "end" )) 
		{
			panim->source.numframes = panim->source.endframe - panim->source.startframe + 1;

			for( t = 0; t < panim->source.numframes; t++ )
			{
				if( panim->rawanim[t] == NULL )
					COM_FatalError( "%s is missing frame %d\n", panim->name, t + panim->source.startframe );
			}
			return true;
		}

		COM_FatalError( "(%d) : %s", linecount, line );
	}

	COM_FatalError( "unexpected EOF: %s\n", panim->name );

	return true;
}

void Grab_Skeleton( s_model_t *pmodel )
{
	Vector	pos;
	Radian	rot;
	char	cmd[1024];
	int	index;

	while( GetLineInput( ))
	{
		if( sscanf( line, "%d %f %f %f %f %f %f", &index, &pos.x, &pos.y, &pos.z, &rot.x, &rot.y, &rot.z ) == 7 )
		{
			pos *= pmodel->scale;
			pmodel->skeleton[index].pos = pos;
			pmodel->skeleton[index].rot = rot;
		}
		else if( sscanf( line, "%s %d", cmd, &index ))
		{
			if( !Q_strcmp( cmd, "time" )) 
			{
				// begin building skeleton
			}
			else if( !Q_strcmp( cmd, "end" )) 
			{
				Build_Reference( pmodel );
				return;
			}
		}
	}
}

int Grab_Nodes( s_node_t *pnodes )
{
	int	index, parent;
	int	numbones = 0;
	char	name[1024];

	for( index = 0; index < MAXSTUDIOSRCBONES; index++ )
		pnodes[index].parent = -1;

	while( GetLineInput( ))
	{
		if( sscanf( line, "%d \"%[^\"]\" %d", &index, name, &parent ) == 3 )
		{
			Q_strncpy( pnodes[index].name, name, sizeof( pnodes[0].name ));
			pnodes[index].parent = parent;
			numbones = Q_max( numbones, index );
		}
		else 
		{
			return numbones + 1;
		}
	}

	COM_FatalError( "Unexpected EOF at line %d\n", linecount );

	return 0;
}

void Grab_Studio( s_model_t *pmodel )
{
	char	cmd[1024];
	int	option;

	Q_snprintf( filename, sizeof( filename ), "%s/%s", cddir[numdirs], pmodel->name );
	COM_DefaultExtension( filename, ".smd" );

	if( !COM_FileExists( filename ))
		COM_FatalError( "%s doesn't exist\n", filename );

	if(( input = fopen( filename, "r" )) == 0 )
		COM_FatalError( "%s couldn't be open\n", filename );

	MsgDev( D_INFO, "grabbing: %s.smd\t\t[^1mesh^7]\n", pmodel->name );
	linecount = 0;

	while( GetLineInput( ))
	{
		int	numRead = sscanf( line, "%s %d", cmd, &option );

		// blank line
		if(( numRead == EOF ) || ( numRead == 0 ))
			continue;

		if( !Q_strcmp( cmd, "version" ))
		{
			if( option != 1 )
				COM_FatalError( "%s version %i should be 1\n", filename, option );
		}
		else if( !Q_strcmp( cmd, "nodes" ))
		{
			pmodel->numbones = Grab_Nodes( pmodel->localBone );
		}
		else if( !Q_strcmp( cmd, "skeleton" ))
		{
			Grab_Skeleton( pmodel );
		}
		else if( !Q_strcmp( cmd, "triangles" ))
		{
			Grab_Triangles( pmodel );
		}
		else 
		{
			MsgDev( D_WARN, "unknown studio command\n" );
		}
	}

	fclose( input );
}

void Grab_Animation( const char *name, s_animation_t *panim )
{
	char	cmd[1024];
	int	option;

	Q_snprintf( filename, sizeof( filename ), "%s/%s", cddir[numdirs], name );
	COM_DefaultExtension( filename, ".smd" );

	if( !COM_FileExists( filename ))
		COM_FatalError ("%s doesn't exist\n", filename);

	if(( input = fopen( filename, "r" )) == 0 )
		COM_FatalError( "%s couldn't be open\n", filename );
	linecount = 0;

	while( GetLineInput( ))
	{
		sscanf( line, "%s %d", cmd, &option );
		if( !Q_strcmp( cmd, "version" ))
		{
			if( option != 1 )
				COM_FatalError( "%s version %i should be 1\n", filename, option );
		}
		else if( !Q_strcmp( cmd, "nodes" ))
		{
			panim->numbones = Grab_Nodes( panim->localBone );
		}
		else if( !Q_strcmp( cmd, "skeleton" ))
		{
			if( !Grab_AnimFrames( panim ))
				break; // animation was cutted
		}
		else 
		{
			// some artists use mesh reference as default animation
			if( Q_strcmp( cmd, "triangles" ))
				MsgDev( D_WARN, "unknown studio command\n" );

			while( GetLineInput( ))
			{
				if( IsEnd( line ))
					break;
			}
		}
	}

	fclose( input );
}

void Cmd_Eyeposition( void )
{
	// rotate points into frame of reference
	// so model points down the positive x axis
	GetToken( false );
	eyeposition.y = verify_atof( token );

	GetToken( false );
	eyeposition.x = -verify_atof( token );

	GetToken( false );
	eyeposition.z = verify_atof( token );
}

void Cmd_Flags( void )
{
	GetToken( false );
	gflags = verify_atoi( token );
}

void Cmd_Modelname( void )
{
	GetToken( false );
	Q_strncpy( outname, token, sizeof( outname ));
}

void Option_Studio( s_model_t *pmodel )
{
	if( !GetToken( false ))
		return;

	Q_strncpy( pmodel->name, token, sizeof( pmodel->name ));
	pmodel->flip_triangles = flip_triangles;
	pmodel->scale = g_defaultscale;

	while( TokenAvailable( ))
	{
		GetToken( false );
		if( !Q_stricmp( "reverse", token ))
		{
			pmodel->flip_triangles = 0;
		}
		else if( !Q_stricmp( "scale", token ))
		{
			GetToken( false );
			pmodel->scale = verify_atof( token );
		}
		else if( !Q_stricmp( "faces", token ))
		{
			GetToken( false );
			GetToken( false );
			continue;
		}
		else if( !Q_stricmp( "bias", token ))
		{
			GetToken( false );
			continue;
		}
		else if( !Q_stricmp( "{", token ))
		{
			UnGetToken( );
			break;
		}
		else
		{
			COM_FatalError( "unknown command \"%s\"\n", token );
		}
	}

	Grab_Studio( pmodel );
}

int Option_Blank( void )
{
	g_model[g_nummodels] = (s_model_t *)Mem_Alloc( sizeof( s_model_t ));
	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
	Q_strncpy( g_model[g_nummodels]->name, "blank", sizeof( g_model[0]->name ));

	g_bodypart[g_numbodyparts].nummodels++;
	g_nummodels++;

	return 0;
}

void Cmd_Bodygroup( void )
{
	int	depth = 0;

	if( !GetToken( false ))
		return;

	if( g_numbodyparts == 0 ) g_bodypart[g_numbodyparts].base = 1;
	else g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	Q_strncpy( g_bodypart[g_numbodyparts].name, token, sizeof( g_bodypart[g_numbodyparts].name ));

	do
	{
		GetToken( true );

		if( endofscript )
		{
			return;
		}
		else if( token[0] == '{' )
		{
			depth++;
		}
		else if( token[0] == '}' )
		{
			depth--;
			break;
		}
		else if( !Q_stricmp( "studio", token ))
		{
			g_model[g_nummodels] = (s_model_t *)Mem_Alloc( sizeof( s_model_t ));
			g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
			g_bodypart[g_numbodyparts].nummodels++;

			Option_Studio( g_model[g_nummodels] );
			g_nummodels++;
		}
		else if( !Q_stricmp( "blank", token ))
		{
			Option_Blank( );
		}
		else
		{
			COM_FatalError( "unknown bodygroup option: \"%s\"\n", token );
		}
	} while( 1 );

	g_numbodyparts++;
}

void Cmd_Body( void )
{
	if( !GetToken( false ))
		return;

	if( g_numbodyparts == 0 ) g_bodypart[g_numbodyparts].base = 1;
	else g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	Q_strncpy(g_bodypart[g_numbodyparts].name, token, sizeof( g_bodypart[0].name ));

	g_model[g_nummodels] = (s_model_t *)Mem_Alloc( sizeof( s_model_t ));
	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
	g_bodypart[g_numbodyparts].nummodels = 1;

	Option_Studio( g_model[g_nummodels] );

	g_numbodyparts++;
	g_nummodels++;
}

int Cmd_Model( void )
{
	int	depth = 0;

	g_model[g_nummodels] = (s_model_t *)Mem_Alloc( sizeof( s_model_t ));
	
	// name
	if( !GetToken( false ))
		return 0;

	Q_strncpy( g_model[g_nummodels]->name, token, sizeof( g_model[0]->name ));

	// fake bodypart stuff
	if( g_numbodyparts == 0 ) g_bodypart[g_numbodyparts].base = 1;
	else g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	Q_strncpy(g_bodypart[g_numbodyparts].name, token, sizeof( g_bodypart[0].name ));

	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
	g_bodypart[g_numbodyparts].nummodels = 1;
	g_numbodyparts++;

	Option_Studio( g_model[g_nummodels] );

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if( depth != 0 )
				COM_FatalError( "missing }\n" );
			return 1;
		}

		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}
		else
		{
			// NOTE: cmd $model has many various params
			// that completely ignored in goldsource
			// so we just use it as equal of $body command
			// and skip all this unknown commands 
			while( TryToken( ));
		}

		if( depth < 0 )
			COM_FatalError( "missing {\n" );
	}
	g_nummodels++;

	return 0;
}

int Option_Activity( s_sequence_t *psequence )
{
	int	i;

	GetToken( false );

	if(( i = LookupActivity( token )) != 0 )
	{
		psequence->activity = i;
		GetToken( false );
		psequence->actweight = verify_atoi( token );
		return 0;
	}

	return 1;
}

int Option_Event( s_sequence_t *psequence )
{
	if(( psequence->numevents + 1 ) >= MAXSTUDIOEVENTS )
		COM_FatalError( "too many events\n" );

	GetToken( false );
	psequence->event[psequence->numevents].event = verify_atoi( token );

	GetToken( false );
	psequence->event[psequence->numevents].frame = verify_atoi( token );

	psequence->numevents++;

	// option token
	if( TokenAvailable( ))
	{
		GetToken( false );

		if( token[0] == '}' ) // opps, hit the end
			return 1;

		// found an option
		Q_strncpy( psequence->event[psequence->numevents-1].options, token, sizeof( psequence->event[0].options ));
	}

	return 0;
}

void Option_IKRule( s_ikrule_t *pRule )
{
	int	i;

	// chain
	GetToken( false );

	for( i = 0; i < g_numikchains; i++)
	{
		if( !Q_stricmp( token, g_ikchain[i].name ))
			break;
	}

	if( i >= g_numikchains )
		TokenError( "unknown chain \"%s\" in ikrule\n", token );

	pRule->chain = i;
	// default slot
	pRule->slot = i;

	// type
	GetToken( false );
	if( !Q_stricmp( token, "touch" ))
	{
		pRule->type = IK_SELF;

		// bone
		GetToken( false );
		Q_strncpy( pRule->bonename, token, sizeof( pRule->bonename ));
	}
	else if( !Q_stricmp( token, "footstep" ))
	{
		pRule->type = IK_GROUND;

		pRule->height = g_ikchain[pRule->chain].height;
		pRule->floor = g_ikchain[pRule->chain].floor;
		pRule->radius = g_ikchain[pRule->chain].radius;
	}
	else if( !Q_stricmp( token, "attachment" ))
	{
		pRule->type = IK_ATTACHMENT;

		// name of attachment
		GetToken( false );
		Q_strncpy( pRule->attachment, token, sizeof( pRule->attachment ));
	}
	else if( !Q_stricmp( token, "release" ))
	{
		pRule->type = IK_RELEASE;
	}
	else if( !Q_stricmp( token, "unlatch" ))
	{
		pRule->type = IK_UNLATCH;
	}

	pRule->contact = -1;

	while( TokenAvailable( ))
	{
		GetToken( false );
		if( !Q_stricmp( token, "height" ))
		{
			GetToken( false );
			pRule->height = verify_atof( token );
		}
		else if( !Q_stricmp( token, "target" ))
		{
			// slot
			GetToken( false );
			pRule->slot = verify_atoi( token );
		}
		else if( !Q_stricmp( token, "range" ))
		{
			// ramp
			GetToken( false );
			if( token[0] == '.' ) pRule->start = -1;
			else pRule->start = verify_atoi( token );

			GetToken( false );
			if( token[0] == '.' ) pRule->peak = -1;
			else pRule->peak = verify_atoi( token );
	
			GetToken( false );
			if( token[0] == '.' ) pRule->tail = -1;
			else pRule->tail = verify_atoi( token );

			GetToken( false );
			if( token[0] == '.' ) pRule->end = -1;
			else pRule->end = verify_atoi( token );
		}
		else if( !Q_stricmp( token, "floor" ))
		{
			GetToken( false );
			pRule->floor = verify_atof( token );
		}
		else if( !Q_stricmp( token, "pad" ))
		{
			GetToken( false );
			pRule->radius = verify_atof( token ) / 2.0f;
		}
		else if( !Q_stricmp( token, "radius" ))
		{
			GetToken( false );
			pRule->radius = verify_atof( token );
		}
		else if( !Q_stricmp( token, "contact" ))
		{
			GetToken( false );
			pRule->contact = verify_atoi( token );
		}
		else if( !Q_stricmp( token, "usesequence" ))
		{
			pRule->usesequence = true;
			pRule->usesource = false;
		}
		else if( !Q_stricmp( token, "usesource" ))
		{
			pRule->usesequence = false;
			pRule->usesource = true;
		}
		else if( !Q_stricmp( token, "fakeorigin" ))
		{
			GetToken( false );
			pRule->pos.x = verify_atof( token );
			GetToken( false );
			pRule->pos.y = verify_atof( token );
			GetToken( false );
			pRule->pos.z = verify_atof( token );

			pRule->bone = -1;
		}
		else if( !Q_stricmp( token, "fakerotate" ))
		{
			Vector	ang;

			GetToken( false );
			ang.x = verify_atof( token );
			GetToken( false );
			ang.y = verify_atof( token );
			GetToken( false );
			ang.z = verify_atof( token );
			AngleQuaternion( ang, pRule->q );

			pRule->bone = -1;
		}
		else if( !Q_stricmp( token, "bone" ))
		{
			GetToken( false );
			Q_strncpy( pRule->bonename, token, sizeof( pRule->bonename ));
		}
		else
		{
			UnGetToken();
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Key value block!
//-----------------------------------------------------------------------------
void Option_KeyValues( CUtlArray< char > *pKeyValue )
{
	// Simply read in the block between { }s as text 
	// and plop it out unchanged into the .mdl file. 
	// Make sure to respect the fact that we may have nested {}s
	int nLevel = 1;

	if( !GetToken( true ))
		return;

	if( token[0] != '{' )
		return;

	AppendKeyValueText( pKeyValue, "mdlkeyvalue\n{\n" );

	while( GetToken( true ))
	{
		if( !Q_stricmp( token, "}" ))
		{
			if( --nLevel <= 0 )
				break;
			AppendKeyValueText( pKeyValue, " }\n" );
		}
		else if( !Q_stricmp( token, "{" ))
		{
			AppendKeyValueText( pKeyValue, "{\n" );
			nLevel++;
		}
		else
		{
			// tokens inside braces are quoted
			if( nLevel > 1 )
			{
				AppendKeyValueText( pKeyValue, "\"" );
				AppendKeyValueText( pKeyValue, token );
				AppendKeyValueText( pKeyValue, "\" " );
			}
			else
			{
				AppendKeyValueText( pKeyValue, token );
				AppendKeyValueText( pKeyValue, " " );
			}
		}
	}

	if( nLevel >= 1 )
	{
		TokenError( "Keyvalue block missing matching braces.\n" );
	}

	AppendKeyValueText( pKeyValue, "}\n" );
}

void Cmd_PoseParameter( void )
{
	if( g_numposeparameters >= MAXSTUDIOPOSEPARAM )
		TokenError( "too many pose parameters (max %d)\n", MAXSTUDIOPOSEPARAM );

	int i = LookupPoseParameter( token );

	// name
	GetToken( false );
	Q_strncpy( g_pose[i].name, token, sizeof( g_pose[0].name ));

	if( TokenAvailable( ))
	{
		// min
		GetToken( false );
		g_pose[i].min = verify_atof( token );
	}

	if( TokenAvailable( ))
	{
		// max
		GetToken( false );
		g_pose[i].max = verify_atof( token );
	}

	while( TokenAvailable( ))
	{
		GetToken( false );

		if( !Q_stricmp( token, "wrap" ))
		{
			g_pose[i].flags |= STUDIO_LOOPING;
			g_pose[i].loop = g_pose[i].max - g_pose[i].min;
		}
		else if( !Q_stricmp( token, "loop" ))
		{
			g_pose[i].flags |= STUDIO_LOOPING;
			GetToken( false );
			g_pose[i].loop = verify_atof( token );
		}
	}
}

/*
=================
Cmd_Origin
=================
*/
void Cmd_Origin( void )
{
	GetToken( false );
	g_defaultadjust.x = verify_atof (token);

	GetToken( false );
	g_defaultadjust.y = verify_atof (token);

	GetToken( false );
	g_defaultadjust.z = verify_atof (token);

	if( TokenAvailable( ))
	{
		GetToken( false );
		g_defaultrotation.z = DEG2RAD( verify_atof( token ) + 90.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the default root rotation so that the Y axis is up instead of the Z axis (for Maya)
//-----------------------------------------------------------------------------
void Cmd_UpAxis( void )
{
	// We want to create a rotation that rotates from the art space
	// (specified by the up direction) to a z up space
	// Note: x, -x, -y are untested
	GetToken( false );

	if( !Q_stricmp( token, "x" ))
	{
		// rotate 90 degrees around y to move x into z
		g_defaultrotation.x = 0.0f;
		g_defaultrotation.y = M_PI / 2.0f;
	}
	else if( !Q_stricmp( token, "-x" ))
	{
		// untested
		g_defaultrotation.x = 0.0f;
		g_defaultrotation.y = -M_PI / 2.0f;
	}
	else if( !Q_stricmp( token, "y" ))
	{
		// rotate 90 degrees around x to move y into z
		g_defaultrotation.x = M_PI / 2.0f;
		g_defaultrotation.y = 0.0f;
	}
	else if( !Q_stricmp( token, "-y" ))
	{
		// untested
		g_defaultrotation.x = -M_PI / 2.0f;
		g_defaultrotation.y = 0.0f;
	}
	else if( !Q_stricmp( token, "z" ))
	{
		// there's still a built in 90 degree Z rotation :(
		g_defaultrotation.x = 0.0f;
		g_defaultrotation.y = 0.0f;
	}
	else if( !Q_stricmp( token, "-z" ))
	{
		// there's still a built in 90 degree Z rotation :(
		g_defaultrotation.x = 0.0f;
		g_defaultrotation.y = 0.0f;
	}
	else
	{
		TokenError( "unknown $upaxis option: \"%s\"\n", token );
		return;
	}
}

void Cmd_ScaleUp( void )
{
	GetToken( false );
	g_defaultscale = verify_atof( token );
}

void Cmd_ScaleAxis( int axis )
{
	GetToken( false );
	g_defaultscale[axis] = verify_atof( token );
}

int Cmd_SequenceGroup( void )
{
	GetToken( false );
	Q_strncpy( g_sequencegroup[g_numseqgroups].label, token, sizeof( g_sequencegroup[0].label ));
	g_numseqgroups++;

	return 0;
}

int Cmd_SequenceGroupSize( void )
{
	GetToken( false );
	maxseqgroupsize = 1024 * verify_atoi( token );
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: parse order dependant s_animcmd_t token for $animations
//-----------------------------------------------------------------------------
int ParseCmdlistToken( int &numcmds, s_animcmd_t *cmds, int cmd_source )
{
	if( numcmds >= MAXSTUDIOCMDS )
		return false;

	s_animcmd_t *pcmd = &cmds[numcmds];

	pcmd->cmd_source = cmd_source;

	if( !Q_stricmp( "fixuploop", token ))
	{
		pcmd->cmd = CMD_FIXUP;

		GetToken( false );
		pcmd->fixuploop.start = verify_atoi( token );
		GetToken( false );
		pcmd->fixuploop.end = verify_atoi( token );
	}
	else if( !Q_strnicmp( "weightlist", token, 6 ))
	{
		GetToken( false );

		int i;
		for( i = 1; i < g_numweightlist; i++ )
		{
			if( !Q_stricmp( g_weightlist[i].name, token ))
				break;
		}

		if( i == g_numweightlist )
		{
			TokenError( "unknown weightlist '%s\'\n", token );
		}

		pcmd->cmd = CMD_WEIGHTS;
		pcmd->weightlist.index = i;
	}
	else if( !Q_stricmp( "subtract", token ))
	{
		pcmd->cmd = CMD_SUBTRACT;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown subtract animation '%s\'\n", token );
		}

		pcmd->subtract.ref = extanim;

		GetToken( false );
		pcmd->subtract.frame = verify_atoi( token );

		pcmd->subtract.flags |= STUDIO_POST;
	}
	else if( !Q_stricmp( "presubtract", token )) // FIXME: rename this to something better
	{
		pcmd->cmd = CMD_SUBTRACT;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown presubtract animation '%s\'\n", token );
		}

		pcmd->subtract.ref = extanim;

		GetToken( false );
		pcmd->subtract.frame = verify_atoi( token );
	}
	else if( !Q_stricmp( "alignto", token ))
	{
		pcmd->cmd = CMD_AO;

		pcmd->ao.pBonename = NULL;

		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}

		pcmd->ao.ref = extanim;
		pcmd->ao.motiontype = STUDIO_X | STUDIO_Y;
		pcmd->ao.srcframe = 0;
		pcmd->ao.destframe = 0;
	}
	else if( !Q_stricmp( "align", token ))
	{
		pcmd->cmd = CMD_AO;

		pcmd->ao.pBonename = NULL;

		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown align animation '%s\'\n", token );
		}

		pcmd->ao.ref = extanim;

		// motion type to match
		pcmd->ao.motiontype = 0;
		GetToken( false );
		int ctrl;

		while(( ctrl = LookupControl( token )) != -1 )
		{
			pcmd->ao.motiontype |= ctrl;
			GetToken( false );
		}

		if( pcmd->ao.motiontype == 0 )
		{
			TokenError( "missing controls on align\n" );
		}

		// frame of reference animation to match
		pcmd->ao.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->ao.destframe = verify_atoi( token );
	}
	else if( !Q_stricmp( "alignboneto", token ))
	{
		pcmd->cmd = CMD_AO;

		GetToken( false );
		pcmd->ao.pBonename = strdup( token );
		
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown alignboneto animation '%s\'\n", token );
		}

		pcmd->ao.ref = extanim;
		pcmd->ao.motiontype = STUDIO_X | STUDIO_Y;
		pcmd->ao.srcframe = 0;
		pcmd->ao.destframe = 0;
	}
	else if( !Q_stricmp( "alignbone", token ))
	{
		pcmd->cmd = CMD_AO;

		GetToken( false );
		pcmd->ao.pBonename = strdup( token );
		
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown alignboneto animation '%s\'\n", token );
		}

		pcmd->ao.ref = extanim;

		// motion type to match
		pcmd->ao.motiontype = 0;
		GetToken( false );
		int ctrl;

		while(( ctrl = LookupControl( token )) != -1 )
		{
			pcmd->ao.motiontype |= ctrl;
			GetToken( false );
		}

		if( pcmd->ao.motiontype == 0 )
		{
			TokenError( "missing controls on align\n" );
		}

		// frame of reference animation to match
		pcmd->ao.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->ao.destframe = verify_atoi( token );
	}
	else if( !Q_stricmp( "match", token ))
	{
		pcmd->cmd = CMD_MATCH;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown match animation '%s\'\n", token );
		}

		pcmd->match.ref = extanim;
	}
	else if( !Q_stricmp( "matchblend", token ))
	{
		pcmd->cmd = CMD_MATCHBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown match animation '%s\'\n", token );
		}

		pcmd->match.ref = extanim;

		// frame of reference animation to match
		GetToken( false );
		pcmd->match.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->match.destframe = verify_atoi( token );

		// backup and starting match in here
		GetToken( false );
		pcmd->match.destpre = verify_atoi( token );

		// continue blending match till here
		GetToken( false );
		pcmd->match.destpost = verify_atoi( token );

	}
	else if( !Q_stricmp( "worldspaceblend", token ))
	{
		pcmd->cmd = CMD_WORLDSPACEBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown worldspaceblend animation '%s\'\n", token );
		}

		pcmd->world.ref = extanim;
		pcmd->world.startframe = 0;
		pcmd->world.loops = false;
	}
	else if( !Q_stricmp( "worldspaceblendloop", token ))
	{
		pcmd->cmd = CMD_WORLDSPACEBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown worldspaceblend animation '%s\'\n", token );
		}

		pcmd->world.ref = extanim;

		GetToken( false );
		pcmd->world.startframe = atoi( token );

		pcmd->world.loops = true;
	}
	else if( !Q_stricmp( "rotateto", token ))
	{
		pcmd->cmd = CMD_ANGLE;

		GetToken( false );
		pcmd->angle.angle = verify_atof( token );
	}
	else if( !Q_stricmp( "ikrule", token ))
	{
		pcmd->cmd = CMD_IKRULE;

		pcmd->ikrule.pRule = (s_ikrule_t *)Mem_Alloc( sizeof( s_ikrule_t ));

		Option_IKRule( pcmd->ikrule.pRule );
	}
	else if( !Q_stricmp( "ikfixup", token ))
	{
		pcmd->cmd = CMD_IKFIXUP;

		pcmd->ikfixup.pRule = (s_ikrule_t *)Mem_Alloc( sizeof( s_ikrule_t ));

		Option_IKRule( pcmd->ikrule.pRule );
	}
	else if( !Q_stricmp( "walkframe", token ))
	{
		pcmd->cmd = CMD_MOTION;

		// frame
		GetToken( false );
		pcmd->motion.iEndFrame = verify_atoi( token );

		// motion type to match
		pcmd->motion.motiontype = 0;
		while( TokenAvailable( ))
		{
			GetToken( false );
			int ctrl = LookupControl( token );

			if( ctrl != -1 )
			{
				pcmd->motion.motiontype |= ctrl;
			}
			else
			{
				UnGetToken();
				break;
			}
		}
	}
	else if( !Q_stricmp( "walkalignto", token ))
	{
		pcmd->cmd = CMD_REFMOTION;

		GetToken( false );
		pcmd->motion.iEndFrame = verify_atoi( token );

		pcmd->motion.iSrcFrame = pcmd->motion.iEndFrame;

		GetToken( false ); // reference animation
		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}

		pcmd->motion.pRefAnim = extanim;
		pcmd->motion.iRefFrame = 0;

		// motion type to match
		pcmd->motion.motiontype = 0;

		while( TokenAvailable( ))
		{
			GetToken( false );
			int ctrl = LookupControl( token );

			if( ctrl != -1 )
			{
				pcmd->motion.motiontype |= ctrl;
			}
			else
			{
				UnGetToken();
				break;
			}
		}
	}
	else if( !Q_stricmp( "walkalign", token ))
	{
		pcmd->cmd = CMD_REFMOTION;

		// end frame to apply motion over
		GetToken( false );
		pcmd->motion.iEndFrame = verify_atoi( token );

		// reference animation
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if( extanim == NULL )
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}
		pcmd->motion.pRefAnim = extanim;

		// motion type to match
		pcmd->motion.motiontype = 0;
		while( TokenAvailable( ))
		{
			GetToken( false );
			int ctrl = LookupControl( token );
			if( ctrl != -1 )
			{
				pcmd->motion.motiontype |= ctrl;
			}
			else
			{
				break;
			}
		}

		if( pcmd->motion.motiontype == 0 )
		{
			TokenError( "missing controls on walkalign\n" );
		}

		// frame of reference animation to match
		pcmd->motion.iRefFrame = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->motion.iSrcFrame = verify_atoi( token );
	}
	else if( !Q_stricmp( "derivative", token ))
	{
		pcmd->cmd = CMD_DERIVATIVE;

		// get scale
		GetToken( false );
		pcmd->derivative.scale = verify_atof( token );
	}
	else if( !Q_stricmp( "noanimation", token ))
	{
		pcmd->cmd = CMD_NOANIMATION;
	}
	else if( !Q_stricmp( "lineardelta", token ))
	{
		pcmd->cmd = CMD_LINEARDELTA;
		pcmd->linear.flags |= STUDIO_AL_POST;
	}
	else if( !Q_stricmp( "splinedelta", token ))
	{
		pcmd->cmd = CMD_LINEARDELTA;
		pcmd->linear.flags |= STUDIO_AL_POST;
		pcmd->linear.flags |= STUDIO_AL_SPLINE;
	}
	else if( !Q_stricmp( "compress", token ))
	{
		pcmd->cmd = CMD_COMPRESS;

		// get frames to skip
		GetToken( false );
		pcmd->compress.frames = verify_atoi( token );
	}
	else if( !Q_stricmp( "numframes", token ))
	{
		pcmd->cmd = CMD_NUMFRAMES;

		// get frames to force
		GetToken( false );
		pcmd->compress.frames = verify_atoi( token );
	}
	else if( !Q_stricmp( "counterrotate", token ))
	{
		pcmd->cmd = CMD_COUNTERROTATE;

		// get bone name
		GetToken( false );
		pcmd->counterrotate.pBonename = strdup( token );
	}
	else if( !Q_stricmp( "counterrotateto", token ))
	{
		pcmd->cmd = CMD_COUNTERROTATE;

		pcmd->counterrotate.bHasTarget = true;

		// get pitch
		GetToken( false );
		pcmd->counterrotate.targetAngle[0] = verify_atof( token );

		// get yaw
		GetToken( false );
		pcmd->counterrotate.targetAngle[1] = verify_atof( token );

		// get roll
		GetToken( false );
		pcmd->counterrotate.targetAngle[2] = verify_atof( token );

		// get bone name
		GetToken( false );
		pcmd->counterrotate.pBonename = strdup( token );
	}
	else if( !Q_stricmp( "localhierarchy", token ))
	{
		pcmd->cmd = CMD_LOCALHIERARCHY;

		// get bone name
		GetToken( false );
		pcmd->localhierarchy.pBonename = strdup( token );

		// get parent name
		GetToken( false );
		pcmd->localhierarchy.pParentname = strdup( token );

		pcmd->localhierarchy.start = -1;
		pcmd->localhierarchy.peak = -1;
		pcmd->localhierarchy.tail = -1;
		pcmd->localhierarchy.end = -1;

		if( TokenAvailable( ))
		{
			GetToken( false );
			if( !Q_stricmp( token, "range" ))
			{
				GetToken( false );
				pcmd->localhierarchy.start = verify_atof_with_null( token );

				GetToken( false );
				pcmd->localhierarchy.peak = verify_atof_with_null( token );

				GetToken( false );
				pcmd->localhierarchy.tail = verify_atof_with_null( token );

				GetToken( false );
				pcmd->localhierarchy.end = verify_atof_with_null( token );
			}
			else
			{
				UnGetToken();
			}
		}

		MsgDev( D_WARN, "^1localhierarchy^7: command not supported\n" );
		return false;
	}
	else
	{
		return false;
	}

	numcmds++;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: create named list of boneweights
//-----------------------------------------------------------------------------
void Option_Weightlist( s_weightlist_t *pweightlist )
{
	int	i, depth = 0;

	pweightlist->numbones = 0;

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if (depth != 0) {
				TokenError("missing }\n");
			}
			return;
		}

		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}
		else if( !Q_stricmp( "posweight", token ))
		{
			i = pweightlist->numbones - 1;
			if (i < 0)
			{
				TokenError("Error with specifing bone Position weight \'%s\'\n", pweightlist->name);
				return;
			}

			GetToken( false );
			pweightlist->boneposweight[i] = verify_atof( token );
			if( pweightlist->boneweight[i] == 0 && pweightlist->boneposweight[i] > 0 )
			{
				TokenError( "Non-zero Position weight with zero Rotation weight not allowed \'%s:%s %f %f\'\n", 
				pweightlist->name, pweightlist->bonename[i], pweightlist->boneweight[i], pweightlist->boneposweight[i] );
			}
		}
		else
		{
			i = pweightlist->numbones++;
			if( i >= MAXWEIGHTSPERLIST )
			{
				TokenError( "Too many bones (%d) in weightlist '%s'\n", i, pweightlist->name );
				return;
			}

			pweightlist->bonename[i] = strdup( token );
			GetToken( false );
			pweightlist->boneweight[i] = verify_atof( token );
			pweightlist->boneposweight[i] = pweightlist->boneweight[i];
		}

		if( depth < 0 )
		{
			TokenError( "missing {\n" );
			return;
		}
	}
}

int ParseEmpty( void )
{
	int depth = 0;

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if( depth != 0 )
			{
				TokenError( "missing }\n" );
			}
			return 1;
		}
		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}

		if( depth < 0 )
		{
			TokenError( "missing {\n" );
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: parse order independant s_animation_t token for $animations
//-----------------------------------------------------------------------------
bool ParseAnimationToken( s_animation_t *panim )
{
	int	i;

	if( !Q_stricmp( "if", token ))
	{
		// fixme: add expression evaluation
		GetToken( false );

		if( atoi( token ) == 0 && Q_stricmp( token, "true" ))
		{
			GetToken( true );

			if( token[0] == '{' )
			{
				int depth = 1;
				while( TokenAvailable( ) && depth > 0 )
				{
					GetToken( true );
					if( !Q_stricmp( "{", token ))
					{
						depth++;
					}
					else if( !Q_stricmp( "}", token ))
					{
						depth--;
					}
				}
			}
		}

		return true;
	}

	if( !Q_stricmp( "fps", token ))
	{
		GetToken( false );
		panim->fps = verify_atof( token );
		if( panim->fps <= 0.0f )
		{
			TokenError( "ParseAnimationToken:  fps (%f from '%s') <= 0.0\n", panim->fps, token );
		}
		return true;
	}

	if( !Q_stricmp( "origin", token ))
	{
		GetToken( false );
		panim->adjust.x = verify_atof( token );

		GetToken( false );
		panim->adjust.y = verify_atof( token );

		GetToken( false );
		panim->adjust.z = verify_atof( token );
		return true;
	}
	
	if( !Q_stricmp( "rotate", token ))
	{
		GetToken( false );
		// FIXME: broken for Maya
		panim->rotation.z = DEG2RAD( verify_atof( token ) + 90.0f );
		return true;
	}
	
	if( !Q_stricmp( "angles", token ))
	{
		GetToken( false );
		panim->rotation.x = DEG2RAD( verify_atof( token ));
		GetToken( false );
		panim->rotation.y = DEG2RAD( verify_atof( token ));
		GetToken( false );
		panim->rotation.z = DEG2RAD( verify_atof( token ) + 90.0f );
		return true;
	}
	
	if( !Q_stricmp( "scale", token ))
	{
		GetToken( false );
		panim->scale = verify_atof( token );
		return true;
	}

	if( !Q_strnicmp( "loop", token, 4 ))
	{
		panim->flags |= STUDIO_LOOPING;
		return true;
	}

	if( !Q_strnicmp( "rootlight", token, 4 ))
	{
		panim->flags |= STUDIO_LIGHT_FROM_ROOT;
		return true;
	}
	
	if( !Q_strnicmp( "startloop", token, 5 ))
	{
		GetToken( false );
		panim->looprestart = verify_atoi( token );
		panim->flags |= STUDIO_LOOPING;
		return true;
	}
	
	if( !Q_stricmp( "fudgeloop", token ))
	{
		panim->fudgeloop = true;
		panim->flags |= STUDIO_LOOPING;
		return true;
	}
	
	if( !Q_strnicmp( "snap", token, 4 ))
	{
		panim->flags |= STUDIO_SNAP;
		return true;
	}
	
	if( !Q_strnicmp( "frame", token, 5 ))
	{
		GetToken( false );
		panim->startframe = verify_atoi( token );
		GetToken( false );
		panim->endframe = verify_atoi( token );

		if( panim->startframe < panim->source.startframe )
			panim->startframe = panim->source.startframe;

		if( panim->endframe > panim->source.endframe )
			panim->endframe = panim->source.endframe;

		if( panim->endframe < panim->startframe )
			TokenError( "end frame before start frame in %s\n", panim->name );

		panim->numframes = panim->endframe - panim->startframe + 1;
		return true;
	}

	if( !Q_stricmp( "blockname", token ))
	{
		GetToken( false );
		return true;
	}

	if( !Q_stricmp( "post", token ))
	{
		panim->flags |= STUDIO_POST;
		return true;
	}
	
	if( !Q_stricmp( "noautoik", token ))
	{
		panim->noAutoIK = true;
		return true;
	}
	
	if( !Q_stricmp( "autoik", token ))
	{
		panim->noAutoIK = false;
		return true;
	}
	
	if( ParseCmdlistToken( panim->numcmds, panim->cmds, CMDSRC_LOCAL ))
		return true;

	if( !Q_stricmp( "cmdlist", token ))
	{
		GetToken( false ); // A

		for( i = 0; i < g_numcmdlists; i++ )
		{
			if( !Q_stricmp( g_cmdlist[i].name, token ))
				break;
		}

		if (i == g_numcmdlists) {
			TokenError("unknown cmdlist %s\n", token);
			return false;
		}

		for( int j = 0; j < g_cmdlist[i].numcmds; j++ )
		{
			if (panim->numcmds >= MAXSTUDIOCMDS) {
				TokenError("Too many cmds in %s\n", panim->name);
				return false;
			}
			panim->cmds[panim->numcmds++] = g_cmdlist[i].cmds[j];
		}

		return true;
	}

	if( !Q_stricmp( "motionrollback", token ))
	{
		GetToken( false );
		panim->motionrollback = atof( token );
		return true;
	}

	if( !Q_stricmp( "noanimblock", token ))
		return true;

	if( !Q_stricmp( "noanimblockstall", token ))
		return true;

	if( LookupControl( token ) != -1 )
	{
		panim->motiontype |= LookupControl( token );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: create named order dependant s_animcmd_t blocks, used as replicated token list for $animations
//-----------------------------------------------------------------------------
void Cmd_Cmdlist( void )
{
	int	depth = 0;

	// name
	GetToken( false );
	strncpy( g_cmdlist[g_numcmdlists].name, token, sizeof( g_cmdlist[0].name ));

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if( depth != 0 )
				TokenError( "missing }\n" );
			return;
		}

		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}
		else if( !ParseCmdlistToken( g_cmdlist[g_numcmdlists].numcmds, g_cmdlist[g_numcmdlists].cmds, CMDSRC_GLOBAL ))
		{
			TokenError( "unknown command: %s\n", token );
		}

		if( depth < 0 )
		{
			TokenError( "missing {\n" );
		}
	}
	g_numcmdlists++;
}

void Cmd_Weightlist( void )
{
	int	i;

	if( !GetToken( false )) 
		return;

	if( g_numweightlist >= MAXWEIGHTLISTS )
		TokenError( "Too many weightlist commands (%d)\n", MAXWEIGHTLISTS );

	for( i = 1; i < g_numweightlist; i++ )
	{
		if( !Q_stricmp( g_weightlist[i].name, token ))
		{
			TokenError( "Duplicate weightlist '%s'\n", token );
		}
	}

	Q_strncpy( g_weightlist[i].name, token, sizeof( g_weightlist[i].name ));
	Option_Weightlist( &g_weightlist[g_numweightlist] );
	g_numweightlist++;
}

void Cmd_DefaultWeightlist( void )
{
	Option_Weightlist( &g_weightlist[0] );
}

//-----------------------------------------------------------------------------
// Purpose: wrapper for parsing $animation tokens
//-----------------------------------------------------------------------------
int ParseAnimation( s_animation_t *panim )
{
	int	depth = 0;

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if( depth != 0 )
				TokenError( "missing }\n" );
			return 1;
		}

		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}
		else if( !ParseAnimationToken( panim ))
		{
			TokenError( "Unknown animation option\'%s\'\n", token );
		}

		if( depth < 0 )
		{
			TokenError( "missing {\n" );
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: allocate an entry for $animation
//-----------------------------------------------------------------------------
void Cmd_Animation( void )
{
	// name
	GetToken( false );

	s_animation_t *panim = LookupAnimation( token );

	if( panim != NULL )
		TokenError( "Duplicate animation name \"%s\"\n", token );

	// allocate animation entry
	g_panimation[g_numani] = panim = (s_animation_t *)Mem_Alloc( sizeof( s_animation_t ));
	Q_strncpy( panim->name, token, sizeof( panim->name ));
	g_numani++;

	// filename
	GetToken( false );
	Q_strncpy( panim->filename, token, sizeof( panim->filename ));

	// grab animation frames
	Grab_Animation( panim->filename, panim );

	panim->startframe = panim->source.startframe;
	panim->endframe = panim->source.endframe;
	panim->adjust = g_defaultadjust;
	panim->rotation = g_defaultrotation;
	panim->scale = g_defaultscale;
	panim->motionrollback = 0.3f;
	panim->fps = 30.0f;

	ParseAnimation( panim );

	panim->numframes = panim->endframe - panim->startframe + 1;
	MsgDev( D_INFO, "grabbing: %s.smd\t\t[^2anim^7][%d frames]\n", panim->name, panim->numframes );

	// post-apply scale to the frames
	scale_animation( panim );
}

//-----------------------------------------------------------------------------
// Purpose: create a virtual $animation command from a $sequence reference
//-----------------------------------------------------------------------------
s_animation_t *Cmd_ImpliedAnimation( s_sequence_t *psequence, const char *filename )
{
	s_animation_t	*panim;

	// allocate animation entry
	g_panimation[g_numani] = panim = (s_animation_t *)Mem_Alloc( sizeof( s_animation_t ));
	Q_snprintf( panim->name, sizeof( panim->name ), "@%s", psequence->name );
	g_numani++;

	Q_strncpy( panim->filename, filename, sizeof( panim->filename ));

	panim->startframe = 0;
	panim->endframe = MAXSTUDIOANIMATIONS - 1;
	panim->isImplied = true;

	// grab animation frames
	Grab_Animation( panim->filename, panim );

	panim->adjust = g_defaultadjust;
	panim->rotation = g_defaultrotation;
	panim->scale = g_defaultscale;
	panim->motionrollback = 0.3f;
	panim->fps = 30.0f;

	if( panim->startframe < panim->source.startframe )
		panim->startframe = panim->source.startframe;
	
	if( panim->endframe > panim->source.endframe )
		panim->endframe = panim->source.endframe;

	if( panim->endframe < panim->startframe )
		TokenError( "end frame before start frame in %s", panim->name );

	panim->numframes = panim->endframe - panim->startframe + 1;
	MsgDev( D_INFO, "grabbing: %s.smd\t\t[^2anim^7][%d frames]\n", panim->name, panim->numframes );

	// post-apply scale to the frames
	scale_animation( panim );

	return panim;
}

//-----------------------------------------------------------------------------
// Purpose: copy globally reavent $animation options from one $animation to another
//-----------------------------------------------------------------------------
void CopyAnimationSettings( s_animation_t *pdest, s_animation_t *psrc )
{
	pdest->fps = psrc->fps;

	pdest->adjust = psrc->adjust;
	pdest->rotation = psrc->rotation;
	pdest->scale = psrc->scale;
	pdest->motiontype = psrc->motiontype;

	for (int i = 0; i < psrc->numcmds; i++)
	{
		if (pdest->numcmds >= MAXSTUDIOCMDS) {
			TokenError("Too many cmds in %s\n", pdest->name);
			return;
		}
		pdest->cmds[pdest->numcmds++] = psrc->cmds[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: allocate an entry for $sequence
//-----------------------------------------------------------------------------
s_sequence_t *ProcessCmdSequence( const char *pSequenceName )
{
	s_animation_t *panim = LookupAnimation( pSequenceName );

	// allocate sequence
	if( panim != NULL )
		TokenError( "Duplicate sequence name \"%s\"\n", pSequenceName );

	if( g_numseq >= MAXSTUDIOSEQUENCES )
		TokenError( "Too many sequences (%d max)\n", MAXSTUDIOSEQUENCES );

	s_sequence_t *pseq = &g_sequence[g_numseq];

	// initialize sequence
	Q_strncpy( pseq->name, pSequenceName, sizeof( pseq->name ));

	pseq->seqgroup = g_numseqgroups - 1;
	pseq->paramindex[0] = -1;
	pseq->paramindex[1] = -1;
	pseq->groupsize[0] = 0;
	pseq->groupsize[1] = 0;
	pseq->fadeintime = 0.2f;
	pseq->fadeouttime = 0.2f;
	pseq->fps = 30.0f;
	pseq->flags = 0;
	g_numseq++;

	return pseq;
}

//-----------------------------------------------------------------------------
// Performs processing on a sequence
//-----------------------------------------------------------------------------
void ProcessSequence( s_sequence_t *pseq, int numblends, s_animation_t **animations, bool isAppend )
{
	if( isAppend ) return;

	if( numblends == 0 )
		TokenError( "no animations found\n");

	if( pseq->groupsize[0] == 0 )
	{
		// GoldSrc 9-way blending never filled second group
		if( numblends < 4 || !FBitSet( pseq->flags, STUDIO_BLENDPOSE ))
		{
			pseq->groupsize[0] = numblends;
			pseq->groupsize[1] = 1;
		}
		else
		{
			int i = sqrt( (float)numblends );

			if(( i * i ) == numblends )
			{
				pseq->groupsize[0] = i;
				pseq->groupsize[1] = i;
			}
			else
			{
				TokenError( "non-square (%d) number of blends without \"blendwidth\" set\n", numblends );
			}
		}
	}
	else
	{
		pseq->groupsize[1] = numblends / pseq->groupsize[0];

		if( pseq->groupsize[0] * pseq->groupsize[1] != numblends )
			TokenError( "missing animation blends. Expected %d, found %d\n", pseq->groupsize[0] * pseq->groupsize[1], numblends );
	}

	if( numblends == 0 )
		COM_FatalError( "no animations found\n" );

	for( int i = 0; i < numblends; i++ )
	{
		int j = i % pseq->groupsize[0];
		int k = i / pseq->groupsize[0];
#if 0
		// remap 2D array into 1D array
		Msg( "i %i, group[%i x %i] = %i\n", i, j, k, j + pseq->groupsize[0] * k );
		pseq->panim[j + pseq->groupsize[0] * k] = animations[i];
#else
		pseq->panim[i] = animations[i];
#endif
		if( i > 0 && animations[i]->isImplied )
			CopyAnimationSettings( animations[i], animations[0] );
		animations[i]->isImplied = false; // don't copy any more commands

		pseq->motiontype |= animations[i]->motiontype;
		pseq->flags |= animations[i]->flags;
	}

	// g-cont. backward compatibility
	pseq->numframes = animations[0]->numframes;
	pseq->fps = animations[0]->fps;

	pseq->numblends = numblends;
}

//-----------------------------------------------------------------------------
// Purpose: parse options unique to $sequence
//-----------------------------------------------------------------------------
int ParseSequence( s_sequence_t *pseq, bool isAppend )
{
	s_animation_t	*animations[MAXSTUDIOBLENDS];
	int		i, j, n, depth = 0;
	int		numblends = 0;

	// initialize first anim
	if( isAppend ) animations[0] = pseq->panim[0];

	while( 1 )
	{
		if( depth > 0 )
		{
			if( !GetToken( true )) 
			{
				break;
			}
		}
		else
		{
			if( !TokenAvailable( )) 
			{
				break;
			}
			else 
			{
				GetToken( false );
			}
		}

		if( endofscript )
		{
			if( depth != 0 )
				COM_FatalError( "missing }\n" );
			return 1;
		}

		if( !Q_stricmp( "{", token ))
		{
			depth++;
		}
		else if( !Q_stricmp( "}", token ))
		{
			depth--;
		}
		else if( !Q_stricmp( "event", token ))
		{
			depth -= Option_Event( pseq );
		}
		else if( !Q_stricmp( "activity", token ))
		{
			Option_Activity( pseq );
		}
		else if( !Q_strnicmp( token, "ACT_", 4 ))
		{
			UnGetToken( );
			Option_Activity( pseq );
		}
		else if( !Q_stricmp( "snap", token ))
		{
			pseq->flags |= STUDIO_SNAP;
		}
		else if( !Q_stricmp( "blendwidth", token ))
		{
			GetToken( false );
			pseq->groupsize[0] = verify_atoi( token );
		}
		else if( !Q_stricmp( "blend", token ))
		{
			i = ( pseq->paramindex[0] != -1 ) ? 1 : 0;

			GetToken( false );

			// GoldSource style blending
			if(( j = LookupControl( token )) == -1 )
			{
				// Source-style blending (pose parameters)
				j = LookupPoseParameter( token );
				SetBits( pseq->flags, STUDIO_BLENDPOSE );
			}

			pseq->paramindex[i] = j;
			pseq->paramattachment[i] = -1;
			GetToken( false );
			pseq->paramstart[i] = verify_atof( token );
			GetToken( false );
			pseq->paramend[i] = verify_atof( token );

			g_pose[j].min = Q_min( g_pose[j].min, pseq->paramstart[i] );
			g_pose[j].min = Q_min( g_pose[j].min, pseq->paramend[i] );
			g_pose[j].max = Q_max( g_pose[j].max, pseq->paramstart[i] );
			g_pose[j].max = Q_max( g_pose[j].max, pseq->paramend[i] );
		}
		else if( !Q_stricmp( "calcblend", token ))
		{
			i = ( pseq->paramindex[0] != -1 ) ? 1 : 0;

			GetToken( false );
			j = LookupPoseParameter( token );
			pseq->paramindex[i] = j;

			GetToken( false );
			pseq->paramattachment[i] = LookupAttachment( token );
			if( pseq->paramattachment[i] == -1 )
			{
				TokenError( "Unknown calcblend attachment \"%s\"\n", token );
			}

			GetToken( false );
			pseq->paramcontrol[i] = LookupControl( token );
			SetBits( pseq->flags, STUDIO_BLENDPOSE );
		}
		else if( !Q_stricmp( "blendref", token ))
		{
			GetToken( false );
			pseq->paramanim = LookupAnimation( token );
			if( pseq->paramanim == NULL )
			{
				TokenError( "Unknown blendref animation \"%s\"\n", token );
			}
		}
		else if( !Q_stricmp( "blendcomp", token ))
		{
			GetToken( false );
			pseq->paramcompanim = LookupAnimation( token );
			if( pseq->paramcompanim == NULL )
			{
				TokenError( "Unknown blendcomp animation \"%s\"\n", token );
			}
		}
		else if( !Q_stricmp( "blendcenter", token ))
		{
			GetToken( false );
			pseq->paramcenter = LookupAnimation( token );
			if( pseq->paramcenter == NULL )
			{
				TokenError( "Unknown blendcenter animation \"%s\"\n", token );
			}
		}
		else if( !Q_strnicmp( "node", token, 4 ))
		{
			GetToken( false );
			pseq->entrynode = pseq->exitnode = LookupXNode( token );
		}
		else if( !Q_stricmp( "transition", token ))
		{
			GetToken( false );
			pseq->entrynode = LookupXNode( token );
			GetToken( false );
			pseq->exitnode = LookupXNode( token );
		}
		else if( !Q_stricmp( "rtransition", token ))
		{
			GetToken( false );
			pseq->entrynode = LookupXNode( token );
			GetToken( false );
			pseq->exitnode = LookupXNode( token );
			pseq->nodeflags |= 1;
		}
		else if( !Q_stricmp( "exitphase", token ))
		{
			GetToken( false );
		}
		else if( !Q_stricmp( "delta", token ))
		{
			pseq->flags |= STUDIO_DELTA;
			pseq->flags |= STUDIO_POST;
		}
		else if( !Q_stricmp( "worldspace", token ))
		{
			pseq->flags |= STUDIO_WORLD;
			pseq->flags |= STUDIO_POST;
		}
		else if( !Q_stricmp( "post", token )) // remove
		{
			pseq->flags |= STUDIO_POST; 
		}
		else if( !Q_stricmp( "predelta", token ))
		{
			pseq->flags |= STUDIO_DELTA;
		}
		else if( !Q_stricmp( "autoplay", token ))
		{
			pseq->flags |= STUDIO_AUTOPLAY;
		}
		else if( !Q_stricmp( "fadein", token ))
		{
			GetToken( false );
			pseq->fadeintime = verify_atof( token );
		}
		else if( !Q_stricmp( "fadeout", token ))
		{
			GetToken( false );
			pseq->fadeouttime = verify_atof( token );
		}
		else if( !Q_stricmp( "realtime", token ))
		{
			pseq->flags |= STUDIO_REALTIME;
		}
		else if( !Q_stricmp( "posecycle", token ))
		{
			pseq->flags |= STUDIO_CYCLEPOSE;

			GetToken( false );
			pseq->cycleposeindex = LookupPoseParameter( token );
		}
		else if( !Q_stricmp( "hidden", token ))
		{
			pseq->flags |= STUDIO_HIDDEN;
		}
		else if( !Q_stricmp( "addlayer", token ))
		{
			GetToken( false );
			Q_strncpy( pseq->autolayer[pseq->numautolayers].name, token, sizeof( pseq->autolayer[0].name ));
			pseq->numautolayers++;
		}
		else if( !Q_stricmp( "iklock", token ))
		{
			GetToken( false );
			Q_strncpy( pseq->iklock[pseq->numiklocks].name, token, sizeof( pseq->iklock[0].name ));

			GetToken( false );
			pseq->iklock[pseq->numiklocks].flPosWeight = verify_atof( token );

			GetToken( false );
			pseq->iklock[pseq->numiklocks].flLocalQWeight = verify_atof( token );
		
			pseq->numiklocks++;
		}
		else if( !Q_stricmp( "keyvalues", token ))
		{
			Option_KeyValues( &pseq->KeyValue );
		}
		else if( !Q_stricmp( "blendlayer", token ))
		{
			pseq->autolayer[pseq->numautolayers].flags = 0;

			GetToken( false );
			Q_strncpy( pseq->autolayer[pseq->numautolayers].name, token, sizeof( pseq->autolayer[0].name ));

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].start = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].peak = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].tail = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].end = verify_atof( token );

			while( TokenAvailable( ))
			{
				GetToken( false );
				if( !Q_stricmp( "xfade", token ))
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_XFADE;
				}
				else if( !Q_stricmp( "spline", token ))
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_SPLINE;
				}
				else if( !Q_stricmp( "noblend", token ))
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_NOBLEND;
				}
				else if( !Q_stricmp( "poseparameter", token ))
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_POSE;
					GetToken( false );
					pseq->autolayer[pseq->numautolayers].pose = LookupPoseParameter( token );
				}
				else if( !Q_stricmp( "local", token ))
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_LOCAL;
					pseq->flags |= STUDIO_LOCAL;
				}
				else
				{
					UnGetToken();
					break;
				}
			}

			pseq->numautolayers++;
		}
		else if(( numblends || isAppend ) && ParseAnimationToken( animations[0] ))
		{

		}
		else if( !isAppend )
		{
			// assume it's an animation reference
			// first look up an existing animation
			for( n = 0; n < g_numani; n++ )
			{
				if( !Q_stricmp( token, g_panimation[n]->name ))
				{
					animations[numblends++] = g_panimation[n];
					break;
				}
			}

			if( n >= g_numani )
			{
				// assume it's an implied animation
				animations[numblends++] = Cmd_ImpliedAnimation( pseq, token );
			}

			// hack to allow animation commands to refer to same sequence
			if( numblends == 1 )
			{
				pseq->panim[0] = animations[0];
			}

		}
		else
		{
			TokenError( "unknown command \"%s\"\n", token );
		}

		if( depth < 0 )
		{
			TokenError( "missing {\n" );
		}
	}

	ProcessSequence( pseq, numblends, animations, isAppend );

	return 0;
}

//-----------------------------------------------------------------------------
// Process the sequence command
//-----------------------------------------------------------------------------
void Cmd_Sequence( void )
{
	if( !GetToken( false )) 
		return;

	s_sequence_t *pseq = ProcessCmdSequence( token );
	if( pseq ) ParseSequence( pseq, false );
}

//-----------------------------------------------------------------------------
// Purpose: append commands to either a sequence or an animation
//-----------------------------------------------------------------------------
void Cmd_Append( void )
{
	GetToken( false );

	s_sequence_t *pseq = LookupSequence( token );

	if( pseq )
	{
		ParseSequence( pseq, true );
		return;
	}

	s_animation_t *panim = LookupAnimation( token );

	if( panim )
	{
		ParseAnimation( panim );
		return;
	}

	TokenError( "unknown append animation %s\n", token );
}

void Cmd_Prepend( void )
{
	GetToken( false );

	s_sequence_t *pseq = LookupSequence( token );
	s_animation_t *panim = NULL;
	int iRet =  false;
	int count = 0;

	if( pseq )
	{
		panim = pseq->panim[0];
		count = panim->numcmds;
		iRet = ParseSequence( pseq, true );
	}
	else
	{
		panim = LookupAnimation( token );
		if( panim )
		{
			count = panim->numcmds;
			iRet = ParseAnimation( panim );
		}
	}

	if( panim && count != panim->numcmds )
	{
		s_animcmd_t tmp;
		tmp = panim->cmds[panim->numcmds - 1];
		for( int i = panim->numcmds - 1; i > 0; i-- )
			panim->cmds[i] = panim->cmds[i-1];
		panim->cmds[0] = tmp;
		return;
	}

	TokenError( "unknown prepend animation \"%s\"\n", token );
}

void Cmd_Continue( void )
{
	GetToken( false );

	s_sequence_t *pseq = LookupSequence( token );

	if( pseq )
	{
		GetToken( true );
		UnGetToken();
		if( token[0] != '$' )
			ParseSequence( pseq, true );
		return;
	}

	s_animation_t *panim = LookupAnimation( token );

	if( panim )
	{
		GetToken( true );
		UnGetToken();
		if( token[0] != '$' )
			ParseAnimation( panim );
		return;
	}

	TokenError( "unknown continue animation %s\n", token );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_IKChain( void )
{
	if( !GetToken( false )) 
		return;

	int	i;

	for( i = 0; i < g_numikchains; i++ )
	{
		if( !Q_stricmp( token, g_ikchain[i].name ))
			break;
	}

	if( i < g_numikchains )
	{
		MsgDev( D_WARN, "duplicate ikchain \"%s\" ignored\n", token );
		while( TryToken( ));
		return;
	}

	Q_strncpy( g_ikchain[g_numikchains].name, token, sizeof( g_ikchain[0].name ));

	GetToken( false );
	Q_strncpy( g_ikchain[g_numikchains].bonename, token, sizeof( g_ikchain[0].bonename ));

	g_ikchain[g_numikchains].axis = STUDIO_Z;
	g_ikchain[g_numikchains].value = 0.0;
	g_ikchain[g_numikchains].height = 18.0;	// sv_stepheight
	g_ikchain[g_numikchains].floor = 0.0;
	g_ikchain[g_numikchains].radius = 0.0;

	while( TokenAvailable( ))
	{
		GetToken( false );

		if( LookupControl( token ) != -1 )
		{
			g_ikchain[g_numikchains].axis = LookupControl( token );
			GetToken( false );
			g_ikchain[g_numikchains].value = verify_atof( token );
		}
		else if( !Q_stricmp( "height", token ))
		{
			GetToken( false );
			g_ikchain[g_numikchains].height = verify_atof( token );
		}
		else if( !Q_stricmp( "pad", token ))
		{
			GetToken( false );
			g_ikchain[g_numikchains].radius = verify_atof( token ) / 2.0;
		}
		else if( !Q_stricmp( "floor", token ))
		{
			GetToken( false );
			g_ikchain[g_numikchains].floor = verify_atof( token );
		}
		else if( !Q_stricmp( "knee", token ))
		{
			GetToken( false );
			g_ikchain[g_numikchains].link[0].kneeDir.x = verify_atof( token );
			GetToken( false );
			g_ikchain[g_numikchains].link[0].kneeDir.y = verify_atof( token );
			GetToken( false );
			g_ikchain[g_numikchains].link[0].kneeDir.z = verify_atof( token );
		}
		else if( !Q_stricmp( "center", token ))
		{
			GetToken( false );
			g_ikchain[g_numikchains].center.x = verify_atof( token );
			GetToken( false );
			g_ikchain[g_numikchains].center.y = verify_atof( token );
			GetToken( false );
			g_ikchain[g_numikchains].center.z = verify_atof( token );
		}
	}
	g_numikchains++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_IKAutoplayLock( void )
{
	GetToken( false );
	Q_strncpy( g_ikautoplaylock[g_numikautoplaylocks].name, token, sizeof( g_ikautoplaylock[0].name ));

	GetToken( false );
	g_ikautoplaylock[g_numikautoplaylocks].flPosWeight = verify_atof( token );

	GetToken( false );
	g_ikautoplaylock[g_numikautoplaylocks].flLocalQWeight = verify_atof( token );
	
	g_numikautoplaylocks++;
}


int Cmd_Root( void )
{
	if( GetToken( false ))
	{
		Q_strncpy( rootname, token, sizeof( rootname ));
		return 0;
	}

	return 1;
}

int Cmd_Controller( void )
{
	if( !GetToken( false ))
		return 0;

	if( g_numbonecontrollers >= MAXSTUDIOCONTROLLERS )
		COM_FatalError( "too many bone controllers (max %d)\n", MAXSTUDIOCONTROLLERS );

	if( !Q_stricmp( "mouth", token ))
		g_bonecontroller[g_numbonecontrollers].index = STUDIO_MOUTH;
	else g_bonecontroller[g_numbonecontrollers].index = verify_atoi( token );

	if( g_bonecontroller[g_numbonecontrollers].index < 0 || g_bonecontroller[g_numbonecontrollers].index > STUDIO_MOUTH )
		COM_FatalError( "$controller: invalid input index %d (allowed in range 0-4)\n", g_bonecontroller[g_numbonecontrollers].index );

	if( GetToken( false ))
	{
		Q_strncpy( g_bonecontroller[g_numbonecontrollers].name, token, sizeof( g_bonecontroller[0].name ));
		GetToken( false );

		if(( g_bonecontroller[g_numbonecontrollers].type = LookupControl( token )) == -1 ) 
		{
			MsgDev( D_WARN, "unknown bonecontroller type '%s'\n", token );
			return 0;
		}

		GetToken( false );
		g_bonecontroller[g_numbonecontrollers].start = verify_atof( token );

		GetToken( false );
		g_bonecontroller[g_numbonecontrollers].end = verify_atof( token );

		float	start = g_bonecontroller[g_numbonecontrollers].start;
		float	end = g_bonecontroller[g_numbonecontrollers].end;

		if( g_bonecontroller[g_numbonecontrollers].type & ( STUDIO_XR|STUDIO_YR|STUDIO_ZR ))
		{
			if(((int)( start + 360 ) % 360 ) == ((int)( end + 360 ) % 360 ))
				g_bonecontroller[g_numbonecontrollers].type |= STUDIO_RLOOP;
		}
		g_numbonecontrollers++;
	}

	return 1;
}

void Cmd_ScreenAlign( void )
{
	if( GetToken( false ))
	{
		Q_strncpy( g_screenalignedbone[g_numscreenalignedbones].name, token, sizeof( g_screenalignedbone[0].name ));
		g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_SPHERE;

		if( GetToken( false ) )
		{
			if( !Q_stricmp( "sphere", token ))
			{
				g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_SPHERE;
			}
			else if( !stricmp( "cylinder", token ))
			{
				g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_CYLINDER;
			}
		}

		g_numscreenalignedbones++;

	}
	else
	{
		COM_FatalError( "$screenalign: expected bone name\n" );
	}
}

void Cmd_BBox( void )
{
	GetToken( false );
	bbox[0][0] = verify_atof( token );

	GetToken( false );
	bbox[0][1] = verify_atof( token );

	GetToken( false );
	bbox[0][2] = verify_atof( token );

	GetToken( false );
	bbox[1][0] = verify_atof( token );

	GetToken( false );
	bbox[1][1] = verify_atof( token );

	GetToken( false );
	bbox[1][2] = verify_atof( token );

	g_wrotebbox = true;
}

void Cmd_CBox( void )
{
	GetToken( false );
	cbox[0][0] = verify_atof( token );

	GetToken( false );
	cbox[0][1] = verify_atof( token );

	GetToken( false );
	cbox[0][2] = verify_atof( token );

	GetToken( false );
	cbox[1][0] = verify_atof( token );

	GetToken( false );
	cbox[1][1] = verify_atof( token );

	GetToken( false );
	cbox[1][2] = verify_atof( token );

	g_wrotecbox = true;
}

void Cmd_Gamma( void )
{
	GetToken( false );
	g_gamma = verify_atof( token );
}

int Cmd_TextureGroup( void )
{
	int	depth = 0;
	int	index = 0;
	int	group = 0;

	if( g_numtextures == 0 )
		COM_FatalError( "texturegroups must follow model loading\n" );

	if( !GetToken( false ))
		return 0;

	if( g_numskinref == 0 )
		g_numskinref = g_numtextures;

	while( 1 )
	{
		if( !GetToken( true )) 
			break;

		if( endofscript )
		{
			if( depth != 0 )
				COM_FatalError( "missing }\n" );
			return 1;
		}

		if( token[0] == '{' )
		{
			depth++;
		}
		else if( token[0] == '}' )
		{
			depth--;
			if( depth == 0 )
				break;
			group++;
			index = 0;
		}
		else if( depth == 2 )
		{
			int i = LookupTexture( token );
			g_texturegroup[g_numtexturegroups][group][index] = i;
			if( group != 0 ) g_texture[i].parent = g_texturegroup[g_numtexturegroups][0][index];
			index++;
			g_numtexturereps[g_numtexturegroups] = index;
			g_numtexturelayers[g_numtexturegroups] = group + 1;
		}
	}
	g_numtexturegroups++;

	return 0;
}

int Cmd_Hitgroup( void )
{
	GetToken( false );
	g_hitgroup[g_numhitgroups].group = verify_atoi( token );
	GetToken( false );
	Q_strncpy( g_hitgroup[g_numhitgroups].name, token, sizeof( g_hitgroup[0].name ));
	g_numhitgroups++;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_Hitbox( void )
{
	bool	autogenerated = false;

	if( g_hitboxsets.Size() == 0 )
	{
		g_hitboxsets.AddToTail();
		autogenerated = true;
	}

	// last one
	s_hitboxset_t *set = &g_hitboxsets[ g_hitboxsets.Size() - 1 ];

	if( autogenerated )
	{
		memset( set, 0, sizeof( *set ) );
		// fill in name if it wasn't specified in the .qc
		Q_strncpy( set->hitboxsetname, "default", sizeof( set->hitboxsetname ));
	}

	GetToken( false );
	set->hitbox[set->numhitboxes].group = verify_atoi( token );
	
	// grab the bone name:
	GetToken( false );
	Q_strncpy( set->hitbox[set->numhitboxes].name, token, sizeof( set->hitbox[0].name ));

	GetToken( false );
	set->hitbox[set->numhitboxes].bmin[0] = verify_atof( token );
	GetToken( false );
	set->hitbox[set->numhitboxes].bmin[1] = verify_atof( token );
	GetToken( false );
	set->hitbox[set->numhitboxes].bmin[2] = verify_atof( token );
	GetToken( false );
	set->hitbox[set->numhitboxes].bmax[0] = verify_atof( token );
	GetToken( false );
	set->hitbox[set->numhitboxes].bmax[1] = verify_atof( token );
	GetToken( false );
	set->hitbox[set->numhitboxes].bmax[2] = verify_atof( token );

	// scale hitboxes
	set->hitbox[set->numhitboxes].bmin *= g_defaultscale;
	set->hitbox[set->numhitboxes].bmax *= g_defaultscale;

	TryToken(); // skip the hit box name if present:

	set->numhitboxes++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_HitboxSet( void )
{
	// add a new hitboxset
	s_hitboxset_t *set = &g_hitboxsets[ g_hitboxsets.AddToTail() ];
	GetToken( false );
	memset( set, 0, sizeof( *set ) );
	Q_strncpy( set->hitboxsetname, token, sizeof( set->hitboxsetname ));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_BoneMerge( void )
{
	int nIndex = g_BoneMerge.AddToTail();

	// bone name
	GetToken( false );
	Q_strncpy( g_BoneMerge[nIndex].bonename, token, sizeof( g_BoneMerge[0].bonename ));
}


int Cmd_Attachment( void )
{
	Vector tmp;
	int i;

	if( g_numattachments >= MAXSTUDIOATTACHMENTS )
		COM_FatalError( "too many attachments (max %d)\n", MAXSTUDIOATTACHMENTS );

	// attachment name
	GetToken( false );
	Q_strncpy( g_attachment[g_numattachments].name, token, sizeof( g_attachment[0].name ));

	// bone name
	GetToken( false );
	Q_strncpy( g_attachment[g_numattachments].bonename, token, sizeof( g_attachment[0].bonename ));

	// position
	GetToken( false );
	tmp.x = verify_atof( token );
	GetToken( false );
	tmp.y = verify_atof( token );
	GetToken( false );
	tmp.z = verify_atof( token );
	tmp *= g_defaultscale;

	g_attachment[g_numattachments].local.Identity();

	while( TokenAvailable( ))
	{
		GetToken( false );

		if( !Q_stricmp( token, "absolute" ))
		{
			g_attachment[g_numattachments].local = matrix3x4( g_vecZero, g_defaultrotation ).Invert();
			g_attachment[g_numattachments].flags |= STUDIO_ATTACHMENT_LOCAL;
			g_attachment[g_numattachments].type |= IS_ABSOLUTE;
		}
		else if( !Q_stricmp( token, "rigid" ))
		{
			g_attachment[g_numattachments].flags |= STUDIO_ATTACHMENT_LOCAL;
			g_attachment[g_numattachments].type |= IS_RIGID;
		}
		else if( !Q_stricmp( token, "world_align" ))
		{
		}
		else if( !Q_stricmp( token, "rotate" ))
		{
			Vector angles = g_vecZero;

			for( i = 0; i < 3; i++ )
			{
				if( !TokenAvailable( ))
					break;
				GetToken( false );
				angles[i] = verify_atof( token );
			}

			g_attachment[g_numattachments].local = matrix3x4( g_vecZero, angles );
			g_attachment[g_numattachments].flags |= STUDIO_ATTACHMENT_LOCAL;
		}
		else if( !Q_stricmp( token, "x_and_z_axes" ))
		{
			Vector xaxis, yaxis, zaxis;

			for( i = 0; i < 3; i++ )
			{
				if( !TokenAvailable( ))
					break;

				GetToken( false );
				xaxis[i] = verify_atof( token );
			}

			for( i = 0; i < 3; i++ )
			{
				if (!TokenAvailable())
					break;

				GetToken( false );
				zaxis[i] = verify_atof( token );
			}

			xaxis = xaxis.Normalize();
			zaxis += xaxis * -DotProduct( zaxis, xaxis );
			zaxis = zaxis.Normalize();
			yaxis = CrossProduct( zaxis, xaxis );

			g_attachment[g_numattachments].flags |= STUDIO_ATTACHMENT_LOCAL;
			g_attachment[g_numattachments].local.SetForward( xaxis );
			g_attachment[g_numattachments].local.SetRight( yaxis );
			g_attachment[g_numattachments].local.SetUp( zaxis );
		}
		else
		{
			MsgDev( D_WARN, "unknown attachment (%s) option\n", g_attachment[g_numattachments].name );
			while( TryToken( ));
		}
	}

	g_attachment[g_numattachments].local.SetOrigin( tmp );
	g_numattachments++;

	return 0;
}

void Cmd_Renamebone( void )
{
	// from
	GetToken( false );
	Q_strncpy( g_renamedbone[g_numrenamedbones].from, token, sizeof( g_renamedbone[0].from ));

	// to
	GetToken( false );
	Q_strncpy( g_renamedbone[g_numrenamedbones].to, token, sizeof( g_renamedbone[0].to ));

	g_numrenamedbones++;
}

void Cmd_SkipTransition( void )
{
	int	nskips = 0;
	int	list[10];

	while( TokenAvailable( ))
	{
		GetToken( false );
		list[nskips++] = LookupXNode( token );
	}

	for( int i = 0; i < nskips; i++ )
	{
		for( int j = 0; j < nskips; j++ )
		{
			if( list[i] != list[j] )
			{
				g_xnodeskip[g_numxnodeskips][0] = list[i];
				g_xnodeskip[g_numxnodeskips][1] = list[j];
				g_numxnodeskips++;
			}
		}
	}
}

void Cmd_TexRenderMode( void )
{
	GetToken( false );
	int i = LookupTexture( token );

	GetToken( false );
	if( !Q_stricmp( token, "additive" ))
	{
		g_texture[i].flags |= STUDIO_NF_ADDITIVE;
	}
	else if( !Q_stricmp( token, "masked" ))
	{
		g_texture[i].flags |= STUDIO_NF_MASKED;
	}
	else if( !Q_stricmp( token, "masked_solid" ))
	{
		g_texture[i].flags |= (STUDIO_NF_MASKED|STUDIO_NF_ALPHASOLID);
	}
	else if( !Q_stricmp( token, "fullbright" ))
	{
		g_texture[i].flags |= STUDIO_NF_FULLBRIGHT;
	}
	else if( !Q_stricmp( token, "smooth" ))
	{
		g_texture[i].flags |= STUDIO_NF_SMOOTH;
	}
	else if( !Q_stricmp( token, "nosmooth" ))
	{
		g_texture[i].flags &= ~STUDIO_NF_SMOOTH;
	}
	else if( !Q_stricmp( token, "twoside" ))
	{
		g_texture[i].flags |= STUDIO_NF_TWOSIDE;
	}
	else MsgDev( D_WARN, "Texture '%s' has unknown render mode '%s'!\n", g_texture[i].name, token );
}

//-----------------------------------------------------------------------------
// Purpose: force a specific parent child relationship
//-----------------------------------------------------------------------------
void Cmd_ForcedHierarchy( void )
{
	// child name
	GetToken( false );
	Q_strncpy( g_forcedhierarchy[g_numforcedhierarchy].childname, token, sizeof( g_forcedhierarchy[0].childname ));

	// parent name
	GetToken( false );
	Q_strncpy( g_forcedhierarchy[g_numforcedhierarchy].parentname, token, sizeof( g_forcedhierarchy[0].parentname ));

	g_numforcedhierarchy++;
}

//-----------------------------------------------------------------------------
// Purpose: insert a virtual bone between a child and parent (currently unsupported)
//-----------------------------------------------------------------------------
void Cmd_InsertHierarchy( void )
{
	// child name
	GetToken( false );
	Q_strncpy( g_forcedhierarchy[g_numforcedhierarchy].childname, token, sizeof( g_forcedhierarchy[0].childname ));

	// subparent name
	GetToken( false );
	Q_strncpy( g_forcedhierarchy[g_numforcedhierarchy].subparentname, token, sizeof( g_forcedhierarchy[0].subparentname ));

	// parent name
	GetToken( false );
	Q_strncpy( g_forcedhierarchy[g_numforcedhierarchy].parentname, token, sizeof( g_forcedhierarchy[0].parentname ));

	g_numforcedhierarchy++;
}

//-----------------------------------------------------------------------------
// Purpose: rotate a specific bone
//-----------------------------------------------------------------------------
void Cmd_ForceRealign( void )
{
	// bone name
	GetToken( false );
	Q_strncpy( g_forcedrealign[g_numforcedrealign].name, token, sizeof( g_forcedrealign[0].name ));

	// skip
	GetToken( false );

	// X axis
	GetToken( false );
	g_forcedrealign[g_numforcedrealign].rot.x = DEG2RAD( verify_atof( token ));

	// Y axis
	GetToken( false );
	g_forcedrealign[g_numforcedrealign].rot.y = DEG2RAD( verify_atof( token ));

	// Z axis
	GetToken( false );
	g_forcedrealign[g_numforcedrealign].rot.z = DEG2RAD( verify_atof( token ));

	g_numforcedrealign++;
}


//-----------------------------------------------------------------------------
// Purpose: specify a bone to allow > 180 but < 360 rotation (forces a calculated "mid point" to rotation)
//-----------------------------------------------------------------------------
void Cmd_LimitRotation( )
{
	// bone name
	GetToken( false );
	Q_strncpy( g_limitrotation[g_numlimitrotation].name, token, sizeof( g_limitrotation[0].name ));

	while( TokenAvailable( ))
	{
		// sequence name
		GetToken( false );
		g_limitrotation[g_numlimitrotation].sequencename[g_limitrotation[g_numlimitrotation].numseq++] = strdup( token );
	}

	g_numlimitrotation++;
}

//-----------------------------------------------------------------------------
// Purpose: specify bones to store, even if nothing references them
//-----------------------------------------------------------------------------
void Cmd_DefineBone( void )
{
	// bone name
	GetToken( false );
	Q_strncpy( g_importbone[g_numimportbones].name, token, sizeof( g_importbone[0].name ));

	// parent name
	GetToken( false );
	Q_strncpy( g_importbone[g_numimportbones].parent, token, sizeof( g_importbone[0].parent ));

	Vector pos, ang;

	// default pos
	GetToken( false );
	pos.x = verify_atof( token );
	GetToken( false );
	pos.y = verify_atof( token );
	GetToken( false );
	pos.z = verify_atof( token );
	GetToken( false );
	ang.x = verify_atof( token );
	GetToken( false );
	ang.y = verify_atof( token );
	GetToken( false );
	ang.z = verify_atof( token );

	g_importbone[g_numimportbones].rawLocal = matrix3x4( pos, ang );

	if( TokenAvailable( ))
	{
		g_importbone[g_numimportbones].bPreAligned = true;
		// realign pos
		GetToken( false );
		pos.x = verify_atof( token );
		GetToken( false );
		pos.y = verify_atof( token );
		GetToken(false );
		pos.z = verify_atof( token );
		GetToken( false );
		ang.x = verify_atof( token );
		GetToken( false );
		ang.y = verify_atof( token );
		GetToken( false );
		ang.z = verify_atof( token );

		g_importbone[g_numimportbones].srcRealign = matrix3x4( pos, ang );
	}
	else
	{
		g_importbone[g_numimportbones].srcRealign.Identity();
	}

	g_numimportbones++;
}

//----------------------------------------------------------------------------------------------
float ParseJiggleStiffness( void )
{
	float stiffness;
	float minStiffness; // const float
	float maxStiffness; // const float

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting stiffness value\n", linecount, line );
		return 0.0f;
	}

	stiffness = verify_atof( token ); 
	
	minStiffness = 0.0f;
	maxStiffness = 1000.0f;

	return bound( minStiffness, stiffness, maxStiffness ); 

}

//----------------------------------------------------------------------------------------------
float ParseJiggleDamping( void )
{
	float damping;
	float minDamping; // const float 
	float maxDamping; // const float 

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting damping value\n", linecount, line );
		return 0.0f;
	}

	damping = verify_atof( token );

	minDamping = 0.0f;
	maxDamping = 10.0f;

	return bound( minDamping, damping, maxDamping );  
}


//----------------------------------------------------------------------------------------------
int ParseJiggleAngleConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_ANGLE_CONSTRAINT;

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting angle value\n", linecount, line );
		return false;
	}
	
	jiggleInfo->data.angleLimit = verify_atof( token ) * M_PI / 180.0f;
	
	return true;
}


//----------------------------------------------------------------------------------------------
int ParseJiggleYawConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_YAW_CONSTRAINT;
	
	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting minimum yaw value\n", linecount, line );
		return false;	
	}

	jiggleInfo->data.minYaw = verify_atof( token ) * M_PI / 180.0f;

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting maximum yaw value\n", linecount, line );
		return false;
	}

	jiggleInfo->data.maxYaw = verify_atof( token ) * M_PI / 180.0f;
	
	return true;
}

//----------------------------------------------------------------------------------------------
int ParseJigglePitchConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_PITCH_CONSTRAINT;

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting minimum pitch value\n", linecount, line );
		return false;	
	}

	jiggleInfo->data.minPitch = verify_atof( token ) * M_PI / 180.0f;

	if ( !GetToken( false ) )
	{
		COM_FatalError( "$jigglebone(%d): expecting maximum pitch value\n", linecount, line );
		return false;
	}

	jiggleInfo->data.maxPitch = verify_atof( token ) * M_PI / 180.0f;

	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse common parameters.
 * This assumes a token has already been read, and returns true if
 * the token is recognized and parsed.
 */
int ParseCommonJiggle( s_jigglebone_t *jiggleInfo )
{
	if (!stricmp( token, "tip_mass" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.tipMass = verify_atof( token );
	}
	else if (!stricmp( token, "length" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.length = verify_atof( token );
	}
	else if (!stricmp( token, "angle_constraint" ))
	{
		if (ParseJiggleAngleConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "yaw_constraint" ))
	{
		if (ParseJiggleYawConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "yaw_friction" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.yawFriction = verify_atof( token );
	}
	else if (!stricmp( token, "yaw_bounce" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.yawBounce = verify_atof( token );
	}
	else if (!stricmp( token, "pitch_constraint" ))
	{
		if (ParseJigglePitchConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "pitch_friction" ))
	{
		if ( !GetToken( false ) )
		{
			return false;		
		}

		jiggleInfo->data.pitchFriction = verify_atof( token );
	}
	else if (!stricmp( token, "pitch_bounce" ))
	{
		if ( !GetToken( false ) )
		{
			return false;		
		}

		jiggleInfo->data.pitchBounce = verify_atof( token );
	}
	else
	{
		// unknown token
		COM_FatalError( "$jigglebone: invalid syntax '%s'\n", token );
		return false;
	}
	
	return true;
}

//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for is_flexible subsection
 */
int ParseFlexibleJiggle( s_jigglebone_t *jiggleInfo )
{
	int gotOpenBracket = false;
	jiggleInfo->data.flags |= (JIGGLE_IS_FLEXIBLE | JIGGLE_HAS_LENGTH_CONSTRAINT);

	while (true)
	{
		if (GetToken( true ) == false)
		{
			COM_FatalError( "$jigglebone(%d): is_flexible: parse error\n", linecount, line );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			COM_FatalError( "$jigglebone(%d): is_flexible: missing '{'\n", linecount, line );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "yaw_stiffness" ))
		{
			jiggleInfo->data.yawStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "yaw_damping" ))
		{
			jiggleInfo->data.yawDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "pitch_stiffness" ))
		{
			jiggleInfo->data.pitchStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "pitch_damping" ))
		{
			jiggleInfo->data.pitchDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "along_stiffness" ))
		{
			jiggleInfo->data.alongStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "along_damping" ))
		{
			jiggleInfo->data.alongDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "allow_length_flex" ))
		{
			jiggleInfo->data.flags &= ~JIGGLE_HAS_LENGTH_CONSTRAINT;
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			COM_FatalError( "$jigglebone:is_flexible: invalid syntax '%s'\n", token );
			return false;
		}
	}
	
	return true;
}

//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for is_rigid subsection
 */
int ParseRigidJiggle( s_jigglebone_t *jiggleInfo )
{
	int gotOpenBracket = false;
	jiggleInfo->data.flags |= (JIGGLE_IS_RIGID | JIGGLE_HAS_LENGTH_CONSTRAINT);

	while (true)
	{
		if (GetToken( true ) == false)
		{
			COM_FatalError( "$jigglebone(%d):is_rigid: parse error\n", linecount, line );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			COM_FatalError( "$jigglebone(%d):is_rigid: missing '{'\n", linecount, line );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			COM_FatalError( "$jigglebone:is_rigid: invalid syntax '%s'\n", token );
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for has_base_spring subsection
 */
bool ParseBaseSpringJiggle( s_jigglebone_t *jiggleInfo )
{
	int gotOpenBracket = false;
	jiggleInfo->data.flags |= JIGGLE_HAS_BASE_SPRING;

	while (true)
	{
		if (GetToken( true ) == false)
		{
			COM_FatalError( "$jigglebone(%d):is_rigid: parse error\n", linecount, line );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			COM_FatalError( "$jigglebone(%d):is_rigid: missing '{'\n", linecount, line );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "stiffness" ))
		{
			jiggleInfo->data.baseStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "damping" ))
		{
			jiggleInfo->data.baseDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "left_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinLeft = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxLeft = verify_atof( token );
		}
		else if (!stricmp( token, "left_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseLeftFriction = verify_atof( token );
		}
		else if (!stricmp( token, "up_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinUp = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxUp = verify_atof( token );
		}
		else if (!stricmp( token, "up_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseUpFriction = verify_atof( token );
		}
		else if (!stricmp( token, "forward_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinForward = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxForward = verify_atof( token );
		}
		else if (!stricmp( token, "forward_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseForwardFriction = verify_atof( token );
		}
		else if (!stricmp( token, "base_mass" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMass = verify_atof( token );
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			COM_FatalError( "$jigglebone:has_base_spring: invalid syntax '%s'\n", token );
			return false;
		}
	}

	return true;
}

/*
=================
=================
*/
void Cmd_JiggleBone( void )
{
	s_jigglebone_t *jiggleInfo = &g_jigglebones[g_numjigglebones];
	int gotOpenBracket = false;

	// bone name
	GetToken( false );
	Q_strncpy( jiggleInfo->bonename, token, sizeof( jiggleInfo->bonename ));

	// default values
	memset( &jiggleInfo->data, 0, sizeof( mstudiojigglebone_t ) );
	jiggleInfo->data.length = 10.0f;
	jiggleInfo->data.yawStiffness = 100.0f;
	jiggleInfo->data.pitchStiffness = 100.0f;
	jiggleInfo->data.alongStiffness = 100.0f;
	jiggleInfo->data.baseStiffness = 100.0f;
	jiggleInfo->data.baseMinUp = -100.0f;
	jiggleInfo->data.baseMaxUp = 100.0f;
	jiggleInfo->data.baseMinLeft = -100.0f;
	jiggleInfo->data.baseMaxLeft = 100.0f;
	jiggleInfo->data.baseMinForward = -100.0f;
	jiggleInfo->data.baseMaxForward = 100.0f;

	while( 1 )
	{
		if( !GetToken( true ))
		{
			COM_FatalError( "$jigglebone(%d): parse error\n", linecount, line );
			return;
		}
		
		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			COM_FatalError( "$jigglebone(%d): missing '{'\n", linecount, line );
			return;				
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "is_flexible" ))
		{
			if (ParseFlexibleJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}
		else if (!stricmp( token, "is_rigid" ))
		{
			if (ParseRigidJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}
		else if (!stricmp( token, "has_base_spring" ))
		{
			if( ParseBaseSpringJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}	
		else
		{
			COM_FatalError( "$jigglebone: invalid syntax '%s'\n", token );
			return;				
		}
	}

	MsgDev( D_INFO, "Marking bone %s as a jiggle bone\n", jiggleInfo->bonename );
	g_numjigglebones++;
}

void Grab_AxisInterpBones( void )
{
	char		cmd[1024], tmp[1025];
	s_axisinterpbone_t	*pBone = &g_axisinterpbones[g_numaxisinterpbones];
	s_axisinterpbone_t	*pAxis = NULL;
	Vector		basepos;

	while( GetLineInput( ))
	{
		if( IsEnd( line )) 
			return;

		int	i = sscanf( line, "%1023s \"%[^\"]\" \"%[^\"]\" \"%[^\"]\" \"%[^\"]\" %d",
			cmd, pBone->bonename, tmp, pBone->controlname, tmp, &pBone->axis );

		if( i == 6 && !Q_stricmp( cmd, "bone" ))
		{
			pAxis = pBone;
			pBone->axis = pBone->axis - 1;	// MAX uses 1..3, engine 0..2
			g_numaxisinterpbones++;
			pBone = &g_axisinterpbones[g_numaxisinterpbones];
		}
		else if( !Q_stricmp( cmd, "display" ))
		{
			// skip all display info
		}
		else if( !Q_stricmp( cmd, "type" ))
		{
			// skip all type info
		}
		else if( !Q_stricmp( cmd, "basepos" ))
		{
			i = sscanf( line, "basepos %f %f %f", &basepos.x, &basepos.y, &basepos.z );
			// skip all type info
		}
		else if( !Q_stricmp( cmd, "axis" ))
		{
			Vector	pos, rot;
			int	j;

			i = sscanf( line, "axis %d %f %f %f %f %f %f", &j, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] );
			if( i == 7 )
			{
				pAxis->pos[j] = basepos + pos;
				AngleQuaternion( rot, pAxis->quat[j] );
			}
		}
	}
}

bool Grab_AimAtBones( void )
{
	s_aimatbone_t	*pAimAtBone = &g_aimatbones[g_numaimatbones];

	// already know it's <aimconstraint> in the first string, otherwise wouldn't be here
	if( sscanf( line, "%*s %127s %127s %127s", pAimAtBone->bonename, pAimAtBone->parentname, pAimAtBone->aimname ) == 3 )
	{
		g_numaimatbones++;

		char	cmd[1024];
		Vector	vector;

		while( GetLineInput( )) 
		{
			if( IsEnd( line )) 
				return false;

			if( sscanf( line, "%1023s %f %f %f", cmd, &vector[0], &vector[1], &vector[2] ) != 4 )
			{
				bool allSpace = true;

				// Allow blank lines to be skipped without error
				for( const char *pC = line; *pC != '\0' && pC < ( line + 4096 ); ++pC )
				{
					if( !isspace( *pC ))
					{
						allSpace = false;
						break;
					}
				}

				if( allSpace )
				{
					continue;
				}

				return true;
			}

			if( !Q_stricmp( cmd, "<aimvector>" ))
			{
				// make sure these are unit length on read
				pAimAtBone->aimvector = vector.Normalize();
			}
			else if( !Q_stricmp( cmd, "<upvector>" ))
			{
				// make sure these are unit length on read
				pAimAtBone->upvector = vector.Normalize();
			}
			else if( !Q_stricmp( cmd, "<basepos>" ))
			{
				pAimAtBone->basepos = vector;
			}
			else
			{
				return true;
			}
		}
	}

	// If we get here, we're at EOF
	return false;
}

void Grab_QuatInterpBones( void )
{
	char		cmd[1024];
	Radian		rotateaxis( 0.0f, 0.0f, 0.0f );
	Radian		jointorient( 0.0f, 0.0f, 0.0f );
	s_quatinterpbone_t	*pBone = &g_quatinterpbones[g_numquatinterpbones];
	s_quatinterpbone_t	*pAxis = NULL;
	Vector		basepos;

	while( GetLineInput( )) 
	{
		if( IsEnd( line )) 
			return;

		int i = sscanf( line, "%s %s %s %s %s", cmd, pBone->bonename, pBone->parentname, pBone->controlparentname, pBone->controlname );

		while( i == 4 && !Q_stricmp( cmd, "<aimconstraint>" ))
		{
			// if Grab_AimAtBones() returns false, there file is at EOF
			if( !Grab_AimAtBones( ))
			{
				return;
			}

			// Grab_AimAtBones will read input into line same as here until it gets a line it doesn't understand, at which point
			// it will exit leaving that line in line, so check for the end and scan the current buffer again and continue on with 
			// the normal QuatInterpBones process
			i = sscanf( line, "%s %s %s %s %s", cmd,
			pBone->bonename, pBone->parentname, pBone->controlparentname, pBone->controlname );
		}

		if( i == 5 && !Q_stricmp( cmd, "<helper>" ))
		{
			pAxis = pBone;
			g_numquatinterpbones++;
			pBone = &g_quatinterpbones[g_numquatinterpbones];
		}
		else if( i > 0 )
		{
			// There was a bug before which could cause the same command to be parsed twice
			// because if the sscanf above completely fails, it will return 0 and not 
			// change the contents of cmd, so i should be greater than 0 in order for
			// any of these checks to be valid... Still kind of buggy as these checks
			// do case insensitive stricmp but then the sscanf does case sensitive
			// matching afterwards... Should probably change those to
			// sscanf( line, "%*s %f ... ) etc...

			if( !Q_stricmp( cmd, "<display>" ))
			{
				// skip all display info
				Vector	size;
				float	distance;

				i = sscanf( line, "<display> %f %f %f %f", &size[0], &size[1], &size[2], &distance );

				if( i == 4 )
				{
					pAxis->percentage = distance / 100.0f;
					pAxis->size = size;
				}
				else
				{
					COM_FatalError( "Line %d: Unable to parse procedual <display> bone: %s", linecount, line );
				}
			}
			else if( !Q_stricmp( cmd, "<basepos>" ))
			{
				i = sscanf( line, "<basepos> %f %f %f", &basepos.x, &basepos.y, &basepos.z );
				// skip all type info
			}
			else if( !Q_stricmp( cmd, "<rotateaxis>" ))
			{
				i = sscanf( line, "%*s %f %f %f", &rotateaxis.x, &rotateaxis.y, &rotateaxis.z );
				rotateaxis.x = DEG2RAD( rotateaxis.x );
				rotateaxis.y = DEG2RAD( rotateaxis.y );
				rotateaxis.z = DEG2RAD( rotateaxis.z );
			}
			else if( !Q_stricmp( cmd, "<jointorient>" ))
			{
				i = sscanf( line, "%*s %f %f %f", &jointorient.x, &jointorient.y, &jointorient.z );
				jointorient.x = DEG2RAD( jointorient.x );
				jointorient.y = DEG2RAD( jointorient.y );
				jointorient.z = DEG2RAD( jointorient.z );
			}
			else if( !Q_stricmp( cmd, "<trigger>" ))
			{
				float	tolerance;
				Radian	trigger;
				Vector	pos, rot;
				Radian	ang;
				int	j;

				i = sscanf( line, "<trigger> %f %f %f %f %f %f %f %f %f %f", 
					&tolerance,
					&trigger.x, &trigger.y, &trigger.z,
					&ang.x, &ang.y, &ang.z,
					&pos.x, &pos.y, &pos.z );

				if( i == 10 )
				{
					trigger.x = DEG2RAD( trigger.x );
					trigger.y = DEG2RAD( trigger.y );
					trigger.z = DEG2RAD( trigger.z );
					ang.x = DEG2RAD( ang.x );
					ang.y = DEG2RAD( ang.y );
					ang.z = DEG2RAD( ang.z );

					Vector4D	q, q1, q2;

					AngleQuaternion( ang, q );

					if( rotateaxis.x != 0.0 || rotateaxis.y != 0.0 || rotateaxis.z != 0.0 )
					{
						AngleQuaternion( rotateaxis, q1 );
						QuaternionMult( q1, q, q2 );
						q = q2;
					}

					if( jointorient != g_vecZero )
					{
						AngleQuaternion( jointorient, q1 );
						QuaternionMult( q, q1, q2 );
						q = q2;
					}

					j = pAxis->numtriggers++;
					pAxis->tolerance[j] = DEG2RAD( tolerance );
					AngleQuaternion( trigger, pAxis->trigger[j] );
					pAxis->pos[j] = basepos + pos;
					pAxis->quat[j] = q;
				}
				else
				{
					COM_FatalError( "Line %d: Unable to parse procedual <trigger> bone: %s", linecount, line );
				}
			}
			else
			{
				COM_FatalError( "Line %d: Unable to parse procedual bone data: %s", linecount, line );
			}
		}
		else
		{
			// Allow blank lines to be skipped without error
			bool allSpace = true;

			for( const char *pC = line; *pC != '\0' && pC < ( line + 4096 ); ++pC )
			{
				if( !isspace( *pC ))
				{
					allSpace = false;
					break;
				}
			}

			if( !allSpace )
			{
				COM_FatalError( "Line %d: Unable to parse procedual bone data: %s", linecount, line );
			}
		}
	}
}

void Load_ProceduralBones( void )
{
	char	filename[256];
	char	shortname[64];
	char	cmd[1024];
	int	option;

	GetToken( false );
	Q_snprintf( filename, sizeof( filename ), "%s/%s", cddir[numdirs], token );
	COM_DefaultExtension( filename, ".vrd" );

	if( !COM_FileExists( filename ))
	{
		MsgDev( D_ERROR, "%s doesn't exist\n", filename );
		return;
	}

	if(( input = fopen( filename, "r" )) == 0 )
	{
		MsgDev( D_ERROR, "%s couldn't be open\n", filename );
		return;
	}

	COM_FileBase( filename, shortname );
	MsgDev( D_INFO, "grabbing: %s.%s\t\t[^3bones^7]\n", shortname, COM_FileExtension( filename ));
	linecount = 0;

	if( !Q_stricmp( COM_FileExtension( filename ), "vrd" ))
	{
		Grab_QuatInterpBones( );
	}
	else
	{
		while( GetLineInput( )) 
		{
			int args = sscanf(line, "%s %d", cmd, &option);
			if (args != 2) 
			{
				MsgDev(D_ERROR, "failed line parsing in %s\n", filename);
				return;
			}
			if( !Q_stricmp( cmd, "version" )) 
			{
				if( option != 1 ) 
					COM_FatalError( "%s version %i should be 1\n", filename, option );
			}
			else if( !Q_stricmp( cmd, "proceduralbones" )) 
			{
				Grab_AxisInterpBones( );
			}
		}
	}

	fclose( input );
}

void Cmd_KeyValues( void )
{
	Option_KeyValues( &g_KeyValueText );
}

void Cmd_AlwaysCollapse( void )
{
	GetToken( false );
	int nIndex = g_collapse.AddToTail();
	Q_strncpy( g_collapse[nIndex].bonename, token, sizeof( g_collapse[0].bonename ));
	g_collapse_bones = true;
}

void Cmd_Pushd( void )
{
	GetToken( false );

	Q_strncpy( cddir[numdirs+1], cddir[numdirs], sizeof( cddir[0] ));
	Q_strncat( cddir[numdirs+1], token, sizeof( cddir[0] ));
	Q_strncat( cddir[numdirs+1], "/", sizeof( cddir[0] ));
	numdirs++;
}

void Cmd_Popd( void )
{
	if( numdirs > 0 )
		numdirs--;
}

void Cmd_Lod( void )
{
	ParseEmpty();
}

void Cmd_ShadowLod( void )
{
	ParseEmpty();
}

/*
===============
ParseScript
===============
*/
void ParseScript( void )
{
	while( 1 )
	{
		do
		{	// look for a line starting with a $ command
			GetToken( true );
			if( endofscript )
				return;
			if( token[0] == '$' )
				break;
			while( TryToken( ));
		} while( 1 );
	
		if( !Q_stricmp( token, "$modelname" ))
		{
			Cmd_Modelname ();
		}
		else if( !Q_stricmp( token, "$cd" ))
		{
			if( cdset ) COM_FatalError ("Two $cd in one model");
			GetToken( false );
			Q_strncpy( cddir[0], COM_ExpandArg( token ), sizeof( cddir[0] ));
			cdset = true;
		}
		else if( !Q_stricmp( token, "$cdtexture" ) || !Q_stricmp( token, "$cdmaterials" ))
		{
			while( TokenAvailable( ))
			{
				GetToken( false );
				Q_strncpy( cdtexture[cdtextureset], COM_ExpandArg( token ), sizeof( cdtexture[0] ));
				cdtextureset++;
			}
		}
		else if( !Q_stricmp( token, "$scale" ))
		{
			Cmd_ScaleUp ();
		}
		else if( !Q_stricmp( token, "$scale_x" ))
		{
			Cmd_ScaleAxis( 1 );	// x&y swapped in studio
		}
		else if( !Q_stricmp( token, "$scale_y" ))
		{
			Cmd_ScaleAxis( 0 );	// x&y swapped in studio
		}
		else if( !Q_stricmp( token, "$scale_z" ))
		{
			Cmd_ScaleAxis( 2 );
		}
		else if( !Q_stricmp( token, "$root" ) || !Q_stricmp( token, "$rootbone" ))
		{
			Cmd_Root ();
		}
		else if( !Q_stricmp( token, "$pushd" ))
		{
			Cmd_Pushd();
		}
		else if( !Q_stricmp( token, "$popd" ))
		{
			Cmd_Popd();
		}
		else if( !Q_stricmp( token, "$alwayscollapse" ))
		{
			Cmd_AlwaysCollapse ();
		}
		else if( !Q_stricmp( token, "$controller" ))
		{
			Cmd_Controller ();
		}
		else if (!stricmp( token, "$screenalign" ))
		{
			Cmd_ScreenAlign( );
		}
		else if (!stricmp (token, "$model"))
		{
			Cmd_Model();
		}
		else if( !Q_stricmp( token, "$body" ))
		{
			Cmd_Body();
		}
		else if( !Q_stricmp( token, "$bodygroup" ))
		{
			Cmd_Bodygroup();
		}
		else if (!stricmp (token, "$animation" ))
		{
			Cmd_Animation();
		}
		else if( !Q_stricmp( token, "$cmdlist" ))
		{
			Cmd_Cmdlist ();
		}
		else if( !Q_stricmp( token, "$sequence" ))
		{
			Cmd_Sequence ();
		}
		else if( !Q_stricmp( token, "$append" ))
		{
			Cmd_Append ();
		}
		else if( !Q_stricmp( token, "$prepend" ))
		{
			Cmd_Append ();
		}
		else if( !Q_stricmp( token, "$continue" ))
		{
			Cmd_Append ();
		}
		else if( !Q_stricmp( token, "$sequencegroup" ))
		{
			Cmd_SequenceGroup ();
		}
		else if( !Q_stricmp( token, "$sequencegroupsize" ))
		{
			Cmd_SequenceGroupSize ();
		}
		else if( !Q_stricmp( token, "$weightlist" ))
		{
			Cmd_Weightlist ();
		}
		else if( !Q_stricmp( token, "$defaultweightlist" ))
		{
			Cmd_DefaultWeightlist ();
		}
		else if( !Q_stricmp( token, "$ikchain" ))
		{
			Cmd_IKChain ();
		}
		else if( !Q_stricmp( token, "$ikautoplaylock" ))
		{
			Cmd_IKAutoplayLock ();
		}
		else if( !Q_stricmp( token, "$eyeposition" ))
		{
			Cmd_Eyeposition ();
		}
		else if( !Q_stricmp( token, "$heirarchy" ))
		{
			Cmd_ForcedHierarchy ();
		}
		else if( !Q_stricmp( token, "$hierarchy" ))
		{
			Cmd_ForcedHierarchy ();
		}
		else if( !Q_stricmp( token, "$insertbone" ))
		{
			Cmd_InsertHierarchy ();
		}
		else if( !Q_stricmp( token, "$limitrotation" ))
		{
			Cmd_LimitRotation ();
		}
		else if( !Q_stricmp( token, "$forcerealign" ))
		{
			Cmd_ForceRealign ();
		}
		else if( !Q_stricmp( token, "$origin" ))
		{
			Cmd_Origin ();
		}
		else if (!stricmp (token, "$upaxis"))
		{
			Cmd_UpAxis( );
		}
		else if( !Q_stricmp( token, "$bbox" ))
		{
			Cmd_BBox ();
		}
		else if( !Q_stricmp( token, "$cbox" ))
		{
			Cmd_CBox ();
		}
		else if( !Q_stricmp( token, "$gamma" ))
		{
			Cmd_Gamma ();
		}
		else if( !Q_stricmp( token, "$flags" ))
		{
			Cmd_Flags ();
		}
		else if( !Q_stricmp( token, "$texturegroup" ))
		{
			Cmd_TextureGroup ();
		}
		else if( !Q_stricmp( token, "$skiptransition" ))
		{
			Cmd_SkipTransition ();
		}
		else if( !Q_stricmp( token, "$calctransitions" ))
		{
			g_multistagegraph = 1;
		}
		else if( !Q_stricmp( token, "$staticprop" ))
		{
			gflags |= STUDIO_STATIC_PROP;
			g_staticprop = true;
		}
		else if( !Q_stricmp( token, "$autocenter" ))
		{
			g_centerstaticprop = true;
		}
		else if( !Q_stricmp( token, "$hgroup" ))
		{
			Cmd_Hitgroup ();
		}
		else if( !Q_stricmp( token, "$hbox" ))
		{
			Cmd_Hitbox ();
		}
		else if( !Q_stricmp( token, "$hboxset" ))
		{
			Cmd_HitboxSet ();
		}
		else if( !Q_stricmp( token, "$attachment" ))
		{
			Cmd_Attachment ();
		}
		else if( !Q_stricmp( token, "$bonemerge" ))
		{
			Cmd_BoneMerge ();
		}
		else if( !Q_stricmp( token, "$lod" ))
		{
			Cmd_Lod ();
		}
		else if( !Q_stricmp( token, "$shadowlod" ))
		{
			Cmd_ShadowLod ();
		}
		else if( !Q_stricmp( token, "$externaltextures" ))
		{
			split_textures = 1;
		}
		else if( !Q_stricmp( token, "$cliptotextures" ))
		{
			clip_texcoords = 1;
		}
		else if( !Q_stricmp( token, "$mergecontrollers" ))
		{
			g_mergebonecontrollers =  1;
		}
		else if( !Q_stricmp( token, "$fixedcoords" ))
		{
			store_uv_coords = 1;
		}
		else if( !Q_stricmp( token, "$freecords" ))
		{
			allow_tileing =  1;
			store_uv_coords = 1;
		}
		else if( !Q_stricmp( token, "$freecoords" ))
		{
			allow_tileing =  1;
			store_uv_coords = 1;
		}
		else if( !Q_stricmp( token, "$boneweights" ))
		{
			allow_boneweights = 1;
		}
		else if( !Q_stricmp( token, "$lockbonelengths" ))
		{
			g_lockbonelengths = 1;
		}
		else if( !Q_stricmp( token, "$renamebone" ))
		{
			Cmd_Renamebone ();
		}
		else if( !Q_stricmp( token, "$definebone" ))
		{
			Cmd_DefineBone ();
		}
		else if( !Q_stricmp( token, "$realignbones" ))
		{
			g_realignbones = 1;
		}
		else if( !Q_stricmp( token, "$unlockdefinebones" ))
		{
			g_overridebones = 1;
		}
		else if( !Q_stricmp( token, "$texrendermode" ))
		{
			Cmd_TexRenderMode();
		}
		else if( !Q_stricmp( token, "$jigglebone" ))
		{
			Cmd_JiggleBone ();
		}
		else if( !Q_stricmp( token, "$proceduralbones" ))
		{
			Load_ProceduralBones( );
		}
		else if( !Q_stricmp( token, "$poseparameter" ))
		{
			Cmd_PoseParameter( );
		}
		else if( !Q_stricmp( token, "$keyvalues" ))
		{
			Cmd_KeyValues( );
		}
		else if( !Q_stricmp(token, "$keepfreebones" ))
		{
			g_keep_free_bones = true;
		}
		else if( !Q_stricmp( token, "$collapsebones" ))
		{
			g_collapse_bones = true;
		}
		else if( !Q_stricmp( token, "$collapsebonesaggressive" ))
		{
			g_collapse_bones = true;
			g_collapse_bones_aggressive = true;
		}
		else
		{
			if( token[0] == '$' )
				MsgDev( D_REPORT, "^2Warning:^7 unknown command %s\n", token );
			else MsgDev( D_WARN, "unknown command %s\n", token );
			while( TryToken( )); // skip at rest of the line
		}
	}
}

static void PrintTitle(bool printUsage = true)
{
	Msg("\n");
	Msg("  pxstudiomdl - advanced, crossplatform studiomodel compiler, based on p2studiomdl\n");
	Msg("  Version     : %s (^1%s ^7/ ^2%s ^7/ ^3%s ^7/ ^4%s^7)\n",
		APP_VERSION_STRING,
		BuildInfo::GetDate(),
		BuildInfo::GetCommitHash(),
		BuildInfo::GetArchitecture(),
		BuildInfo::GetPlatform()
	);
	Msg("  Website     : https://github.com/SNMetamorph/PrimeXT\n");

	if (printUsage) {
		Msg("  Usage       : pxstudiomdl <options> file.qc\n");
	}
	Msg("\n");
}

static void PrintOptionsList()
{
	Msg("  Options list:\n"
		"     ^5-t^7   : replace all model textures with specified\n"
		"     ^5-r^7   : tag reversed\n"
		"     ^5-n^7   : tag bad normals\n"
		"     ^5-f^7   : flip all triangles\n"
		"     ^5-a^7   : normal blend angle\n"
		"     ^5-h^7   : dump hitboxes\n"
		"     ^5-g^7   : dump transition graph\n"
		"     ^5-ath^7 : alpha threshold for transparency (0.0 - 1.0, default is 0.5)\n"
		"     ^5-dev^7 : set message verbose level (1-5, default is 3)\n"
		"\n"
	);
}

static void WaitForKey()
{
	Msg("Press any key to exit...\n");
	Sys_WaitForKeyInput();
}

int main( int argc, char **argv )
{
	int	i;
	static char path[1024];

	atexit( Sys_CloseLog );
	COM_InitCmdlib( argv, argc );
	Sys_SetupCrashHandler();

	// impicit path
	Q_strncpy( cddir[0], ".\\", sizeof( cddir[0] ));
	g_numweightlist = 1; // skip weightlist 0
	g_defaultrotation = Radian( 0.0f, 0.0f, M_PI / 2 );
	g_normal_blend = cos( DEG2RAD( 2.0 ));
	g_defaultscale = 1.0f;

	numrep = 0;
	tag_reversed = 0;
	tag_normals = 0;
	flip_triangles = 1;
	g_lockbonelengths = 0;
	g_overridebones = 0;
	maxseqgroupsize = 1024 * 1024;
	g_multistagegraph = 0;
	allow_boneweights = 0;
	has_boneweights = 0;
	g_gamma = 1.8f;
	g_alpha_threshold = 0.5f;

	if( argc == 1 )
	{
		PrintTitle();
		PrintOptionsList();
		WaitForKey();
		return 1;
	}

	Sys_InitLog("pxstudiomdl.log");
	PrintTitle(false);

	for (i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] == '-')
		{
			if (!Q_stricmp(argv[i], "-dev"))
			{
				i++;
				SetDeveloperLevel(verify_atoi(argv[i]));
			}
			else if (!Q_stricmp(argv[i], "-t"))
			{
				i++;
				Q_strncpy(defaulttexture[numrep], argv[i], sizeof(defaulttexture[0]));
				if (i < argc - 2 && argv[i + 1][0] != '-')
				{
					i++;
					Q_strncpy(sourcetexture[numrep], argv[i], sizeof(sourcetexture[0]));
					MsgDev(D_INFO, "Replacing %s with %s\n", sourcetexture[numrep], sizeof(defaulttexture[0]));
				}
				MsgDev(D_INFO, "Using default texture: %s\n", defaulttexture);
				numrep++;
			}
			else if (!Q_stricmp(argv[i], "-r"))
			{
				tag_reversed = 1;
			}
			else if (!Q_stricmp(argv[i], "-n"))
			{
				tag_normals = 1;
			}
			else if (!Q_stricmp(argv[i], "-f"))
			{
				flip_triangles = 0;
			}
			else if (!Q_stricmp(argv[i], "-a"))
			{
				i++;
				g_normal_blend = cos(DEG2RAD(verify_atof(argv[i])));
			}
			else if (!Q_stricmp(argv[i], "-h"))
			{
				g_dump_hboxes = true;
			}
			else if (!Q_stricmp(argv[i], "-g"))
			{
				g_dump_graph = true;
			}
			else if (!Q_stricmp(argv[i], "-ath"))
			{
				i++;
				g_alpha_threshold = bound(0.0f, verify_atof(argv[i]), 1.0f);
			}
		}
	}

	if( !argv[i] )
	{
		MsgDev( D_ERROR, "No inputfile specified\n" );
		return 1;
	}

	Q_strcpy( g_sequencegroup[g_numseqgroups].label, "default" );
	g_numseqgroups = 1;

	// load the script
	Q_strncpy( path, argv[i], sizeof( path ));
	Q_strncpy( outname, path, sizeof( outname ));
	COM_DefaultExtension( path, ".qc" );
	COM_StripExtension( outname );
	LoadScriptFile( path );

	// parse it
	ParseScript ();
	SetSkinValues ();
	SimplifyModel ();
	WriteFile ();
	ClearModel ();

	SetDeveloperLevel( D_REPORT );
	Mem_Check(); // report leaks
	Sys_RestoreCrashHandler();

	return 0;
}
