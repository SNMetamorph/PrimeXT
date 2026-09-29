/*
smd_source.cpp - SMD reference mesh/animation importer
Copyright (C) 2017 Uncle Mike
Copyright (C) 2026 SNMetamorph

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
#include "file_system.h"
#include "smd_source.h"
#include <cctype>

//-----------------------------------------------------------------------------
// Purpose: collapse duplicate bone weights and keep only the strongest ones
//-----------------------------------------------------------------------------
static int SortAndBalanceBones( int iCount, int iMaxCount, int bones[], float weights[] )
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

CSmdSource::CSmdSource( void )
	: m_input( nullptr, &fclose )
	, m_linecount( 0 )
{
	m_line[0] = '\0';
}

bool CSmdSource::GetLine( void )
{
	while( fgets( m_line.data(), m_line.size(), m_input.get()) != NULL )
	{
		m_linecount++;

		// skip comments
		if( m_line[0] == '/' && m_line[1] == '/' )
			continue;
		return true;
	}

	return false;
}

int CSmdSource::Grab_Nodes( s_node_t *pnodes )
{
	int	index, parent;
	int	numbones = 0;
	char	name[1024];

	for( index = 0; index < MAXSTUDIOSRCBONES; index++ )
		pnodes[index].parent = -1;

	while( GetLine( ))
	{
		if( sscanf( m_line.data(), "%d \"%[^\"]\" %d", &index, name, &parent ) == 3 )
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

	COM_FatalError( "Unexpected EOF at line %d\n", m_linecount );

	return 0;
}

void CSmdSource::Grab_Skeleton( s_model_t *pmodel )
{
	Vector	pos;
	Radian	rot;
	char	cmd[1024];
	int	index;

	while( GetLine( ))
	{
		if( sscanf( m_line.data(), "%d %f %f %f %f %f %f", &index, &pos.x, &pos.y, &pos.z, &rot.x, &rot.y, &rot.z ) == 7 )
		{
			pos *= pmodel->scale;
			pmodel->skeleton[index].pos = pos;
			pmodel->skeleton[index].rot = rot;
		}
		else if( sscanf( m_line.data(), "%s %d", cmd, &index ))
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

void CSmdSource::Grab_Triangles( s_model_t *pmodel )
{
	int	i, j, k;
	int	ncount = 0;
	float	vmin = 9999.0f;

	// load the base triangles
	while( 1 ) 
	{
		if( !GetLine( )) 
			break;

		// check for end
		if( IsEnd( m_line.data())) 
			break;

		char		texturename[64];
		s_mesh_t		*pmesh;
		s_trianglevert_t	*ptriv;
		int		bone;
		Vector		vert[3];
		Vector		norm[3];

		Q_strncpy( texturename, m_line.data(), sizeof( texturename ));

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
			GetLine();
			GetLine();
			GetLine();
			continue;
		}

		if( Q_stristr( texturename, "null.bmp" ) || Q_stristr( texturename, "null.tga" ))
		{
			// skip all faces with the null texture on them.
			GetLine();
			GetLine();
			GetLine();
			continue;
		}

		COM_DefaultExtension( texturename, ".tga" ); // Crowbar decompiler issues
		pmesh = LookupMesh( pmodel, texturename );

		for( j = 0; j < 3; j++ ) 
		{
			if( pmodel->flip_triangles )
				ptriv = LookupTriangle( pmesh, pmesh->numtris ) + 2 - j; // quake wants them in the reverse order
			else ptriv = LookupTriangle( pmesh, pmesh->numtris ) + j;

			if( !GetLine( )) 
			{
				COM_FatalError( "%s: error on line %d: %s", m_filename.c_str(), m_linecount, m_line.data());
			}

			pmodel->srcvert.AddToTail();
			s_srcvertex_t	*srcv = &pmodel->srcvert[pmodel->srcvert.Count() - 1];
			int		iCount = 0, bones[MAXSTUDIOSRCBONES];
			float		weights[MAXSTUDIOSRCBONES];
			s_boneweight_t	boneWeight;

			// clean memory before use to avoid bug
			memset(srcv, 0, sizeof(*srcv));

			// get support for Source bone weights description
			i = sscanf( m_line.data(), "%d %f %f %f %f %f %f %f %f %d %d %f %d %f %d %f %d %f",
			&bone, 
			&srcv->vert[0], &srcv->vert[1], &srcv->vert[2], 
			&srcv->norm[0], &srcv->norm[1], &srcv->norm[2], 
			&ptriv->u, &ptriv->v,
			&iCount,
			&bones[0], &weights[0], &bones[1], &weights[1], &bones[2], &weights[2], &bones[3], &weights[3] );

			if( i < 9 ) continue; 

			if( bone < 0 || bone >= pmodel->numbones ) 
			{
				COM_FatalError( "bogus bone index\n%d %s :\n%s", m_linecount, m_filename.c_str(), m_line.data());
			}

			// continue parsing more bones.
			if( iCount > MAXSTUDIOBONEWEIGHTS )
			{
				char	*token;
				int	ctr = 0;

				for( k = 0; k < 18; k++ )
				{
					while( m_line[ctr] == ' ' )
					{
						ctr++;
					}

					token = strtok( &m_line[ctr], " " );
					ctr += Q_strlen( token ) + 1;
				}

				for( k = 4; k < iCount && k < MAXSTUDIOSRCBONES; k++ )
				{
					while( m_line[ctr] == ' ' )
					{
						ctr++;
					}

					token = strtok( &m_line[ctr], " " );
					ctr += Q_strlen( token ) + 1;

					bones[k] = verify_atoi( token );

					token = strtok( &m_line[ctr], " " );
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

bool CSmdSource::Grab_AnimFrames( s_animation_t *panim )
{
	Vector	pos;
	Radian	rot;
	char	cmd[1024];
	int	index, size;
	int	t = -99999999;

	size = panim->numbones * sizeof( s_bone_t );
	panim->source.startframe = -1;
	panim->source.endframe = 0;

	while( GetLine( ))
	{
		if( sscanf( m_line.data(), "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] ) == 7 )
		{
			if( panim->source.startframe < 0 )
				COM_FatalError( "missing frame start(%d) : %s\n", m_linecount, m_line.data());

			panim->rawanim[t][index].pos = pos;
			panim->rawanim[t][index].rot = rot;
			continue;
		}

		if( sscanf( m_line.data(), "%1023s %d", cmd, &index ) == 0 )
		{
			COM_FatalError( "(%d) : %s", m_linecount, m_line.data());
			continue;
		}

		if( !Q_stricmp( cmd, "time" )) 
		{
			t = index;

			if( panim->source.startframe == -1 )
				panim->source.startframe = t;

			if( t < panim->source.startframe )
				COM_FatalError( "frame error(%d) : %s\n", m_linecount, m_line.data());

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

		COM_FatalError( "(%d) : %s", m_linecount, m_line.data());
	}

	COM_FatalError( "unexpected EOF: %s\n", panim->name );

	return true;
}

void CSmdSource::GrabStudio( s_model_t *pmodel )
{
	char	cmd[1024];
	int	option;
	char	path[1024];

	Q_snprintf( path, sizeof( path ), "%s/%s", cddir[numdirs], pmodel->name );
	COM_DefaultExtension( path, ".smd" );
	m_filename = path;

	if( !COM_FileExists( path ))
		COM_FatalError( "%s doesn't exist\n", path );

	m_input.reset( fopen( path, "r" ));
	if( !m_input )
		COM_FatalError( "%s couldn't be open\n", path );

	MsgDev( D_INFO, "grabbing: %s.smd\t\t[^1mesh^7]\n", pmodel->name );
	m_linecount = 0;

	while( GetLine( ))
	{
		int	numRead = sscanf( m_line.data(), "%s %d", cmd, &option );

		// blank line
		if(( numRead == EOF ) || ( numRead == 0 ))
			continue;

		if( !Q_strcmp( cmd, "version" ))
		{
			if( option != 1 )
				COM_FatalError( "%s version %i should be 1\n", path, option );
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
}

void CSmdSource::GrabAnimation( s_animation_t *panim )
{
	char	cmd[1024];
	int	option;
	char	path[1024];

	Q_snprintf( path, sizeof( path ), "%s/%s", cddir[numdirs], panim->filename );
	COM_DefaultExtension( path, ".smd" );
	m_filename = path;

	if( !COM_FileExists( path ))
		COM_FatalError ("%s doesn't exist\n", path );

	m_input.reset( fopen( path, "r" ));
	if( !m_input )
		COM_FatalError( "%s couldn't be open\n", path );
	m_linecount = 0;

	while( GetLine( ))
	{
		sscanf( m_line.data(), "%s %d", cmd, &option );
		if( !Q_strcmp( cmd, "version" ))
		{
			if( option != 1 )
				COM_FatalError( "%s version %i should be 1\n", path, option );
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

			while( GetLine( ))
			{
				if( IsEnd( m_line.data()))
					break;
			}
		}
	}
}
