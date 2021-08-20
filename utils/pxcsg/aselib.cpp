/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "csg.h"
#include "aselib.h"

#define MAX_ASE_MATERIALS		1024
#define MAX_ASE_OBJECTS		2048
#define MAX_ASE_ANIMATIONS		32
#define MAX_ASE_ANIMATION_FRAMES	32

typedef struct
{
	float	x, y, z;
	float	nx, ny, nz;
	float	s, t;
} aseVertex_t;

typedef struct
{
	float	s, t;
} aseTVertex_t;

typedef int	aseFace_t[3];

typedef struct
{
	int		numFaces;
	int		numVertexes;
	int		numTVertexes;
	int		timeValue;
	aseVertex_t	*vertexes;
	aseTVertex_t	*tvertexes;
	aseFace_t		*faces, *tfaces;

	int		currentFace;
	int		currentVertex;
} aseMesh_t;

typedef struct
{
	int		numFrames;
	aseMesh_t		frames[MAX_ASE_ANIMATION_FRAMES];
	int		currentFrame;
} aseMeshAnimation_t;

typedef struct
{
	char		name[128];
} aseMaterial_t;

/*
** contains the animate sequence of a single surface
** using a single material
*/
typedef struct
{
	char		name[128];
	int		materialRef;
	int		numAnimations;
	aseMeshAnimation_t	anim;
} aseGeomObject_t;

typedef struct
{
	int		numMaterials;
	aseMaterial_t	materials[MAX_ASE_MATERIALS];
	aseGeomObject_t	objects[MAX_ASE_OBJECTS];

	char		*buffer;
	char		*curpos;
	size_t		len;
	int		currentObject;
	bool		grabAnims;
} ase_t;

static char	s_token[1024];
static ase_t	ase;

static bool ASE_Process( void );
static void ASE_FreeGeomObject( int ndx );

/*
** ASE_Load
*/
bool ASE_Load( const char *filename, bool grabAnims )
{
	memset( &ase, 0, sizeof( ase ));

	ase.curpos = ase.buffer = (char *)FS_LoadFile( filename, &ase.len, false );
	if( !ase.buffer ) return false;

	ase.grabAnims = grabAnims;
	if( !ASE_Process( ))
		return false;

	return true;
}

/*
** ASE_Free
*/
void ASE_Free( void )
{
	for( int i = 0; i < ase.currentObject; i++ )
	{
		ASE_FreeGeomObject( i );
	}
}

/*
** ASE_GetNumSurfaces
*/
int ASE_GetNumSurfaces( void )
{
	return ase.currentObject;
}

/*
** ASE_GetSurfaceName
*/
const char *ASE_GetSurfaceName( int which )
{
	aseGeomObject_t *pObject = &ase.objects[which];

	if( !pObject->anim.numFrames )
		return NULL;

	return pObject->name;
}

/*
** ASE_GetSurfaceAnimation
**
** Returns an animation (sequence of polysets)
*/
polyset_t *ASE_GetSurfaceAnimation( int which, int *pNumFrames, int skipFrameStart, int skipFrameEnd, int maxFrames )
{
	aseGeomObject_t	*pObject = &ase.objects[which];
	int		numFramesInAnimation;
	int		numFramesToKeep;
	polyset_t		*psets;

	if( !pObject->anim.numFrames )
		return 0;

	if ( pObject->anim.numFrames > maxFrames && maxFrames != -1 )
	{
		numFramesInAnimation = maxFrames;
	}
	else 
	{
		numFramesInAnimation = pObject->anim.numFrames;
		if( maxFrames != -1 )
			MsgDev( D_WARN, "ASE_GetSurfaceAnimation maxFrames > numFramesInAnimation\n" );
	}

	if( skipFrameEnd != -1 )
		numFramesToKeep = numFramesInAnimation - ( skipFrameEnd - skipFrameStart + 1 );
	else numFramesToKeep = numFramesInAnimation;

	*pNumFrames = numFramesToKeep;

	psets = (polyset_t *)Mem_Alloc( sizeof( polyset_t ) * numFramesToKeep );

	for( int f = 0, i = 0; i < numFramesInAnimation; i++ )
	{
		aseMesh_t	*pMesh = &pObject->anim.frames[i];

		if( skipFrameStart != -1 )
		{
			if( i >= skipFrameStart && i <= skipFrameEnd )
				continue;
		}

		Q_strcpy( psets[f].name, pObject->name );
		char *start = Q_stristr( ase.materials[pObject->materialRef].name, "textures\\" );
		if( !start ) start = ase.materials[pObject->materialRef].name;
		else start += Q_strlen( "textures\\" );
		Q_strcpy( psets[f].materialname, start );

		psets[f].triangles = (poly_t *)Mem_Alloc( sizeof( poly_t ) * pObject->anim.frames[i].numFaces );
		psets[f].numtriangles = pObject->anim.frames[i].numFaces;

		for( int t = 0; t < pObject->anim.frames[i].numFaces; t++ )
		{
			for( int k = 0; k < 3; k++ )
			{
				psets[f].triangles[t].verts[k].point[0] = pMesh->vertexes[pMesh->faces[t][k]].x;
				psets[f].triangles[t].verts[k].point[1] = pMesh->vertexes[pMesh->faces[t][k]].y;
				psets[f].triangles[t].verts[k].point[2] = pMesh->vertexes[pMesh->faces[t][k]].z;

				if( pMesh->tvertexes && pMesh->tfaces )
				{
					psets[f].triangles[t].verts[k].coord[0] = pMesh->tvertexes[pMesh->tfaces[t][k]].s;
					psets[f].triangles[t].verts[k].coord[1] = pMesh->tvertexes[pMesh->tfaces[t][k]].t;
				}

			}
		}
		f++;
	}

	return psets;
}

static void ASE_FreeGeomObject( int ndx )
{
	aseGeomObject_t	*pObject;

	pObject = &ase.objects[ndx];

	for( int i = 0; i < pObject->anim.numFrames; i++ )
	{
		if( pObject->anim.frames[i].vertexes )
		{
			Mem_Free( pObject->anim.frames[i].vertexes );
		}
		if ( pObject->anim.frames[i].tvertexes )
		{
			Mem_Free( pObject->anim.frames[i].tvertexes );
		}
		if ( pObject->anim.frames[i].faces )
		{
			Mem_Free( pObject->anim.frames[i].faces );
		}
		if ( pObject->anim.frames[i].tfaces )
		{
			Mem_Free( pObject->anim.frames[i].tfaces );
		}
	}

	memset( pObject, 0, sizeof( *pObject ) );
}

static aseMesh_t *ASE_GetCurrentMesh( void )
{
	aseGeomObject_t	*pObject;

	if( ase.currentObject >= MAX_ASE_OBJECTS )
	{
		COM_FatalError( "Too many GEOMOBJECTs\n" );
		return 0; // never called
	}

	pObject = &ase.objects[ase.currentObject];

	if( pObject->anim.currentFrame >= MAX_ASE_ANIMATION_FRAMES )
	{
		COM_FatalError( "Too many MESHes\n" );
		return 0;
	}

	return &pObject->anim.frames[pObject->anim.currentFrame];
}

static int CharIsTokenDelimiter( int ch )
{
	if ( ch <= 32 )
		return 1;
	return 0;
}

static int ASE_GetToken( bool restOfLine )
{
	int i = 0;

	if( ase.buffer == 0 )
		return 0;

	if(( ase.curpos - ase.buffer ) == ase.len )
		return 0;

	// skip over crap
	while((( ase.curpos - ase.buffer ) < ase.len ) && ( *ase.curpos <= 32 ))
	{
		ase.curpos++;
	}

	while(( ase.curpos - ase.buffer ) < ase.len )
	{
		s_token[i] = *ase.curpos;

		ase.curpos++;
		i++;

		if(( CharIsTokenDelimiter( s_token[i-1] ) && !restOfLine ) || (( s_token[i-1] == '\n' ) || ( s_token[i-1] == '\r' )))
		{
			s_token[i-1] = 0;
			break;
		}
	}

	s_token[i] = 0;

	return 1;
}

static void ASE_ParseBracedBlock( void (*parser)( const char *token ))
{
	int indent = 0;

	while( ASE_GetToken( false ))
	{
		if( !Q_strcmp( s_token, "{" ))
		{
			indent++;
		}
		else if( !Q_strcmp( s_token, "}" ))
		{
			indent--;
			if( indent == 0 )
				break;
			else if( indent < 0 )
				COM_FatalError( "Unexpected '}'\n" );
		}
		else
		{
			if( parser )
				parser( s_token );
		}
	}
}

static void ASE_SkipEnclosingBraces( void )
{
	int indent = 0;

	while( ASE_GetToken( false ))
	{
		if( !Q_strcmp( s_token, "{" ))
		{
			indent++;
		}
		else if( !Q_strcmp( s_token, "}" ))
		{
			indent--;
			if( indent == 0 )
				break;
			else if ( indent < 0 )
				COM_FatalError( "Unexpected '}'\n" );
		}
	}
}

static void ASE_SkipRestOfLine( void )
{
	ASE_GetToken( true );
}

static void ASE_KeyMAP_DIFFUSE( const char *token )
{
	char	buffer[1024];

	if( !Q_strcmp( token, "*BITMAP" ))
	{
		ASE_GetToken( false );

		Q_strcpy( buffer, s_token + 1 );
		if( Q_strchr( buffer, '"' ))
			*Q_strchr( buffer, '"' ) = 0;
		Q_strcpy( ase.materials[ase.numMaterials].name, buffer );
	}
}

static void ASE_KeyMATERIAL( const char *token )
{
	if( !Q_strcmp( token, "*MAP_DIFFUSE" ) )
	{
		ASE_ParseBracedBlock( ASE_KeyMAP_DIFFUSE );
	}
}

static void ASE_KeyMATERIAL_LIST( const char *token )
{
	if( !Q_strcmp( token, "*MATERIAL_COUNT" ))
	{
		ASE_GetToken( false );
		MsgDev( D_REPORT, "..num materials: %s\n", s_token );
		if( atoi( s_token ) > MAX_ASE_MATERIALS )
		{
			COM_FatalError( "Too many materials!\n" );
		}
		ase.numMaterials = 0;
	}
	else if( !Q_strcmp( token, "*MATERIAL" ))
	{
		MsgDev( D_REPORT, "..material %d ", ase.numMaterials );
		ASE_ParseBracedBlock( ASE_KeyMATERIAL );
		ase.numMaterials++;
	}
}

static void ASE_KeyMESH_VERTEX_LIST( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if( !Q_strcmp( token, "*MESH_VERTEX" ))
	{
		ASE_GetToken( false );		// skip number

		ASE_GetToken( false );
		pMesh->vertexes[pMesh->currentVertex].y = atof( s_token );

		ASE_GetToken( false );
		pMesh->vertexes[pMesh->currentVertex].x = -atof( s_token );

		ASE_GetToken( false );
		pMesh->vertexes[pMesh->currentVertex].z = atof( s_token );

		pMesh->currentVertex++;

		if( pMesh->currentVertex > pMesh->numVertexes )
		{
			COM_FatalError( "pMesh->currentVertex >= pMesh->numVertexes\n" );
		}
	}
	else
	{
		COM_FatalError( "Unknown token '%s' while parsing MESH_VERTEX_LIST\n", token );
	}
}

static void ASE_KeyMESH_FACE_LIST( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if( !Q_strcmp( token, "*MESH_FACE" ) )
	{
		ASE_GetToken( false );	// skip face number

		ASE_GetToken( false );	// skip label
		ASE_GetToken( false );	// first vertex
		pMesh->faces[pMesh->currentFace][0] = atoi( s_token );

		ASE_GetToken( false );	// skip label
		ASE_GetToken( false );	// second vertex
		pMesh->faces[pMesh->currentFace][2] = atoi( s_token );

		ASE_GetToken( false );	// skip label
		ASE_GetToken( false );	// third vertex
		pMesh->faces[pMesh->currentFace][1] = atoi( s_token );

		ASE_GetToken( true );
		pMesh->currentFace++;
	}
	else
	{
		COM_FatalError( "Unknown token '%s' while parsing MESH_FACE_LIST\n", token );
	}
}

static void ASE_KeyTFACE_LIST( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if( !Q_strcmp( token, "*MESH_TFACE" ))
	{
		int a, b, c;

		ASE_GetToken( false );

		ASE_GetToken( false );
		a = atoi( s_token );
		ASE_GetToken( false );
		c = atoi( s_token );
		ASE_GetToken( false );
		b = atoi( s_token );

		pMesh->tfaces[pMesh->currentFace][0] = a;
		pMesh->tfaces[pMesh->currentFace][1] = b;
		pMesh->tfaces[pMesh->currentFace][2] = c;

		pMesh->currentFace++;
	}
	else
	{
		COM_FatalError( "Unknown token '%s' in MESH_TFACE\n", token );
	}
}

static void ASE_KeyMESH_TVERTLIST( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if( !Q_strcmp( token, "*MESH_TVERT" ))
	{
		char u[80], v[80], w[80];

		ASE_GetToken( false );

		ASE_GetToken( false );
		strcpy( u, s_token );

		ASE_GetToken( false );
		strcpy( v, s_token );

		ASE_GetToken( false );
		strcpy( w, s_token );

		pMesh->tvertexes[pMesh->currentVertex].s = atof( u );
		pMesh->tvertexes[pMesh->currentVertex].t = 1.0f - atof( v );

		pMesh->currentVertex++;

		if( pMesh->currentVertex > pMesh->numTVertexes )
		{
			COM_FatalError( "pMesh->currentVertex > pMesh->numTVertexes\n" );
		}
	}
	else
	{
		COM_FatalError( "Unknown token '%s' while parsing MESH_TVERTLIST\n" );
	}
}

static void ASE_KeyMESH( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if( !Q_strcmp( token, "*TIMEVALUE" ))
	{
		ASE_GetToken( false );

		pMesh->timeValue = atoi( s_token );
		MsgDev( D_REPORT, ".....timevalue: %d\n", pMesh->timeValue );
	}
	else if( !Q_strcmp( token, "*MESH_NUMVERTEX" ))
	{
		ASE_GetToken( false );

		pMesh->numVertexes = atoi( s_token );
		MsgDev( D_REPORT, ".....TIMEVALUE: %d\n", pMesh->timeValue );
		MsgDev( D_REPORT, ".....num vertexes: %d\n", pMesh->numVertexes );
	}
	else if( !Q_strcmp( token, "*MESH_NUMFACES" ))
	{
		ASE_GetToken( false );

		pMesh->numFaces = atoi( s_token );
		MsgDev( D_REPORT, ".....num faces: %d\n", pMesh->numFaces );
	}
	else if( !Q_strcmp( token, "*MESH_NUMTVFACES" ))
	{
		ASE_GetToken( false );

		if( atoi( s_token ) != pMesh->numFaces )
		{
			COM_FatalError( "MESH_NUMTVFACES != MESH_NUMFACES\n" );
		}
	}
	else if( !Q_strcmp( token, "*MESH_NUMTVERTEX" ))
	{
		ASE_GetToken( false );

		pMesh->numTVertexes = atoi( s_token );
		MsgDev( D_REPORT, ".....num tvertexes: %d\n", pMesh->numTVertexes );
	}
	else if( !Q_strcmp( token, "*MESH_VERTEX_LIST" ))
	{
		pMesh->vertexes = (aseVertex_t *)Mem_Alloc( sizeof( aseVertex_t ) * pMesh->numVertexes );
		pMesh->currentVertex = 0;
		MsgDev( D_REPORT, ".....parsing MESH_VERTEX_LIST\n" );
		ASE_ParseBracedBlock( ASE_KeyMESH_VERTEX_LIST );
	}
	else if( !Q_strcmp( token, "*MESH_TVERTLIST" ))
	{
		pMesh->currentVertex = 0;
		pMesh->tvertexes = (aseTVertex_t *)Mem_Alloc( sizeof( aseTVertex_t ) * pMesh->numTVertexes );
		MsgDev( D_REPORT, ".....parsing MESH_TVERTLIST\n" );
		ASE_ParseBracedBlock( ASE_KeyMESH_TVERTLIST );
	}
	else if( !Q_strcmp( token, "*MESH_FACE_LIST" ))
	{
		pMesh->faces = (aseFace_t *)Mem_Alloc( sizeof( aseFace_t ) * pMesh->numFaces );
		pMesh->currentFace = 0;
		MsgDev( D_REPORT, ".....parsing MESH_FACE_LIST\n" );
		ASE_ParseBracedBlock( ASE_KeyMESH_FACE_LIST );
	}
	else if( !Q_strcmp( token, "*MESH_TFACELIST" ))
	{
		pMesh->tfaces = (aseFace_t *)Mem_Alloc( sizeof( aseFace_t ) * pMesh->numFaces );
		pMesh->currentFace = 0;
		MsgDev( D_REPORT, ".....parsing MESH_TFACE_LIST\n" );
		ASE_ParseBracedBlock( ASE_KeyTFACE_LIST );
	}
	else if( !Q_strcmp( token, "*MESH_NORMALS" ))
	{
		ASE_ParseBracedBlock( 0 );
	}
}

static void ASE_KeyMESH_ANIMATION( const char *token )
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	// loads a single animation frame
	if( !Q_strcmp( token, "*MESH" ) )
	{
		MsgDev( D_REPORT, "...found MESH\n" );
		assert( pMesh->faces == 0 );
		assert( pMesh->vertexes == 0 );
		assert( pMesh->tvertexes == 0 );
		memset( pMesh, 0, sizeof( *pMesh ) );

		ASE_ParseBracedBlock( ASE_KeyMESH );

		if( ++ase.objects[ase.currentObject].anim.currentFrame == MAX_ASE_ANIMATION_FRAMES )
		{
			COM_FatalError( "Too many animation frames\n" );
		}
	}
	else
	{
		COM_FatalError( "Unknown token '%s' while parsing MESH_ANIMATION\n", token );
	}
}

static void ASE_KeyGEOMOBJECT( const char *token )
{
	if ( !strcmp( token, "*NODE_NAME" ) )
	{
		char *name = ase.objects[ase.currentObject].name;

		ASE_GetToken( true );
		MsgDev( D_REPORT, " %s\n", s_token );
		Q_strcpy( ase.objects[ase.currentObject].name, s_token + 1 );
		if( Q_strchr( ase.objects[ase.currentObject].name, '"' ))
			*Q_strchr( ase.objects[ase.currentObject].name, '"' ) = 0;

		if( Q_strstr( name, "tag" ) == name )
		{
			while( Q_strchr( name, '_' ) != Q_strrchr( name, '_' ))
			{
				*Q_strrchr( name, '_' ) = 0;
			}
			while( Q_strrchr( name, ' ' ))
			{
				*Q_strrchr( name, ' ' ) = 0;
			}
		}
	}
	else if( !Q_strcmp( token, "*NODE_PARENT" ))
	{
		ASE_SkipRestOfLine();
	}
	// ignore unused data blocks
	else if( !Q_strcmp( token, "*NODE_TM" ) || !Q_strcmp( token, "*TM_ANIMATION" ))
	{
		ASE_ParseBracedBlock( 0 );
	}
	// ignore regular meshes that aren't part of animation
	else if( !Q_strcmp( token, "*MESH" ) && !ase.grabAnims )
	{
		ASE_ParseBracedBlock( ASE_KeyMESH );

		if( ++ase.objects[ase.currentObject].anim.currentFrame == MAX_ASE_ANIMATION_FRAMES )
		{
			COM_FatalError( "Too many animation frames\n" );
		}

		ase.objects[ase.currentObject].anim.numFrames = ase.objects[ase.currentObject].anim.currentFrame;
		ase.objects[ase.currentObject].numAnimations++;
	}
	// according to spec these are obsolete
	else if( !Q_strcmp( token, "*MATERIAL_REF" ))
	{
		ASE_GetToken( false );

		ase.objects[ase.currentObject].materialRef = atoi( s_token );
	}
	// loads a sequence of animation frames
	else if( !Q_strcmp( token, "*MESH_ANIMATION" ))
	{
		if( ase.grabAnims )
		{
			MsgDev( D_REPORT, "..found MESH_ANIMATION\n" );

			if( ase.objects[ase.currentObject].numAnimations )
			{
				COM_FatalError( "Multiple MESH_ANIMATIONS within a single GEOM_OBJECT\n" );
			}
			ASE_ParseBracedBlock( ASE_KeyMESH_ANIMATION );
			ase.objects[ase.currentObject].anim.numFrames = ase.objects[ase.currentObject].anim.currentFrame;
			ase.objects[ase.currentObject].numAnimations++;
		}
		else
		{
			ASE_SkipEnclosingBraces();
		}
	}
	// skip unused info
	else if( !Q_strcmp( token, "*PROP_MOTIONBLUR" ) || !Q_strcmp( token, "*PROP_CASTSHADOW" ) || !Q_strcmp( token, "*PROP_RECVSHADOW" ))
	{
		ASE_SkipRestOfLine();
	}
}

static void ConcatenateObjects( aseGeomObject_t *pObjA, aseGeomObject_t *pObjB )
{
}

static void CollapseObjects( void )
{
	int numObjects = ase.currentObject;

	for( int i = 0; i < numObjects; i++ )
	{
		int j;

		// skip tags
		if( Q_strstr( ase.objects[i].name, "tag" ) == ase.objects[i].name )
		{
			continue;
		}

		if( !ase.objects[i].numAnimations )
		{
			continue;
		}

		for( j = i + 1; j < numObjects; j++ )
		{
			if( Q_strstr( ase.objects[j].name, "tag" ) == ase.objects[j].name )
			{
				continue;
			}

			if( ase.objects[i].materialRef == ase.objects[j].materialRef )
			{
				if( ase.objects[j].numAnimations )
				{
					ConcatenateObjects( &ase.objects[i], &ase.objects[j] );
				}
			}
		}
	}
}

/*
** ASE_Process
*/
static bool ASE_Process( void )
{
	while( ASE_GetToken( false ))
	{
		if( !Q_strcmp( s_token, "*3DSMAX_ASCIIEXPORT" ) || !Q_strcmp( s_token, "*COMMENT" ))
		{
			ASE_SkipRestOfLine();
		}
		else if( !Q_strcmp( s_token, "*SCENE" ))
			ASE_SkipEnclosingBraces();
		else if( !Q_strcmp( s_token, "*MATERIAL_LIST" ))
		{
			MsgDev( D_REPORT, "MATERIAL_LIST\n" );

			ASE_ParseBracedBlock( ASE_KeyMATERIAL_LIST );
		}
		else if( !Q_strcmp( s_token, "*GEOMOBJECT" ))
		{
			MsgDev( D_REPORT, "GEOMOBJECT" );

			ASE_ParseBracedBlock( ASE_KeyGEOMOBJECT );

			if( Q_strstr( ase.objects[ase.currentObject].name, "Bip" ) || Q_strstr( ase.objects[ase.currentObject].name, "ignore_" ))
			{
				ASE_FreeGeomObject( ase.currentObject );
				MsgDev( D_REPORT, "(discarding BIP/ignore object)\n" );
			}
			else if(( Q_strstr( ase.objects[ase.currentObject].name, "h_" ) != ase.objects[ase.currentObject].name ) &&
				( Q_strstr( ase.objects[ase.currentObject].name, "l_" ) != ase.objects[ase.currentObject].name ) &&
				( Q_strstr( ase.objects[ase.currentObject].name, "u_" ) != ase.objects[ase.currentObject].name ) &&
				( Q_strstr( ase.objects[ase.currentObject].name, "tag" ) != ase.objects[ase.currentObject].name ) &&
					  ase.grabAnims )
			{
				MsgDev( D_REPORT, "(ignoring improperly labeled object '%s')\n", ase.objects[ase.currentObject].name );
				ASE_FreeGeomObject( ase.currentObject );
			}
			else
			{
				if( ++ase.currentObject == MAX_ASE_OBJECTS )
				{
					COM_FatalError( "Too many GEOMOBJECTs\n" );
				}
			}
		}
		else if( s_token[0] )
		{
			MsgDev( D_WARN, "Unknown token '%s'\n", s_token );
		}
	}
	Mem_Free( ase.buffer, C_FILESYSTEM );

	if( !ase.currentObject )
		return false;

	CollapseObjects();

	return true;
}