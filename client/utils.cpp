/*
utils.cpp - utility code
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "triangleapi.h"
#include "r_local.h"
#include <mathlib.h>
#include "r_efx.h"

/*
=============
pfnAlertMessage

=============
*/
void ALERT( ALERT_TYPE level, char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	if( developer_level <= DEV_NONE )
		return;

	if( level == at_aiconsole && developer_level < DEV_EXTENDED )
		return;

	va_start( args, szFmt );
	Q_vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	if( level == at_warning )
	{
		gEngfuncs.Con_Printf( va( "^3Warning:^7 %s", buffer ));
	}
	else if( level == at_error  )
	{
		gEngfuncs.Con_Printf( va( "^1Error:^7 %s", buffer ));
	} 
	else
	{
		gEngfuncs.Con_Printf( buffer );
	}
}

float g_hullcolor[8][3] = 
{
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 0.5f, 0.5f },
{ 0.5f, 1.0f, 0.5f },
{ 1.0f, 1.0f, 0.5f },
{ 0.5f, 0.5f, 1.0f },
{ 1.0f, 0.5f, 1.0f },
{ 0.5f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
};

int g_boxpnt[6][4] =
{
{ 0, 4, 6, 2 }, // +X
{ 0, 1, 5, 4 }, // +Y
{ 0, 2, 3, 1 }, // +Z
{ 7, 5, 1, 3 }, // -X
{ 7, 3, 2, 6 }, // -Y
{ 7, 6, 4, 5 }, // -Z
};	

void DrawQuad( float xmin, float ymin, float xmax, float ymax )
{
	// top left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( xmin, ymin, 0 ); 

	// bottom left
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( xmin, ymax, 0 );

	// bottom right
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( xmax, ymax, 0 );

	// top right
	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3f( xmax, ymin, 0 );
}

/*
=============
WorldToScreen

convert world coordinates (x,y,z) into screen (x, y)
=============
*/
int WorldToScreen( const Vector &world, Vector &screen )
{
	int retval = R_WorldToScreen( world, screen );

	screen[0] =  0.5f * screen[0] * (float)RI->viewport[2];
	screen[1] = -0.5f * screen[1] * (float)RI->viewport[3];
	screen[0] += 0.5f * (float)RI->viewport[2];
	screen[1] += 0.5f * (float)RI->viewport[3];

	return retval;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

HSPRITE LoadSprite( const char *pszName )
{
	char sz[256]; 
	int i;

	if( ScreenWidth < 640 )
		i = 320;
	else i = 640;

	Q_snprintf( sz, sizeof( sz ), pszName, i );

	return SPR_Load( sz );
}

//=================
//   UTIL_IsPlayer
//=================
bool UTIL_IsPlayer( int idx )
{
	if ( idx >= 1 && idx <= gEngfuncs.GetMaxClients() )
		return true;
	return false;
}


//=================
//     UTIL_IsLocal
//=================
bool UTIL_IsLocal( int idx )
{
	return gEngfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
}

/*
======================================================================

LEAF LISTING

NOTE: this code ripped out from Xash3D
======================================================================
*/
static void Mod_BoxLeafnums_r( leaflist_t *ll, mnode_t *node, model_t *worldmodel )
{
	int	s;

	while( 1 )
	{
		if( node->contents == CONTENTS_SOLID )
			return;

		if( node->contents < 0 )
		{
			mleaf_t	*leaf = (mleaf_t *)node;

			// it's a leaf!
			if( ll->count >= ll->maxcount )
			{
				ll->overflowed = true;
				return;
			}

			ll->list[ll->count++] = leaf->cluster;
			return;
		}
	
		s = BOX_ON_PLANE_SIDE( ll->mins, ll->maxs, node->plane );

		if( s == 1 )
		{
			node = node->children[0];
		}
		else if( s == 2 )
		{
			node = node->children[1];
		}
		else
		{
			// go down both
			if( ll->headnode == NULL )
				ll->headnode = node;
			Mod_BoxLeafnums_r( ll, node->children[0], worldmodel );
			node = node->children[1];
		}
	}
}

/*
==================
Mod_BoxLeafnums
==================
*/
int Mod_BoxLeafnums( const Vector &mins, const Vector &maxs, short *list, int listsize, mnode_t **headnode )
{
	leaflist_t ll;
	model_t	*worldmodel;

	worldmodel = gEngfuncs.GetEntityByIndex( 0 )->model;
	if( headnode ) *headnode = NULL;

	if( !worldmodel )
		return 0;

	ll.mins = mins;
	ll.maxs = maxs;
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.headnode = NULL;
	ll.overflowed = false;

	Mod_BoxLeafnums_r( &ll, worldmodel->nodes, worldmodel );

	if( headnode )
		*headnode = ll.headnode;
	return ll.count;
}

/*
=============
Mod_HeadnodeVisible
=============
*/
bool Mod_HeadnodeVisible( mnode_t *node, const byte *visbits )
{
	if( !node || node->contents == CONTENTS_SOLID )
		return false;

	if( node->contents < 0 )
	{
		if( CHECKVISBIT( visbits, ((mleaf_t *)node)->cluster ))
			return true;
		return false;
	}

	if( Mod_HeadnodeVisible( node->children[0], visbits ))
		return true;

	return Mod_HeadnodeVisible( node->children[1], visbits );
}

/*
=============
Mod_BoxVisible

Returns true if any leaf in boxspace
is potentially visible
=============
*/
bool Mod_BoxVisible( const Vector mins, const Vector maxs, const byte *visbits )
{
	short	leafList[2048];
	mnode_t	*headnode;
	int	i, count;

	if( !visbits || !mins || !maxs )
		return true;

	count = Mod_BoxLeafnums( mins, maxs, leafList, ARRAYSIZE( leafList ), &headnode );

	if( count < ARRAYSIZE( leafList ))
		headnode = NULL; // ignore headnode if we not overflowed

	for( i = 0; i < count; i++ )
	{
		if( CHECKVISBIT( visbits, leafList[i] ))
			return true;
	}

	if( Mod_HeadnodeVisible( headnode, visbits ))
		return true;

	return false;
}

/*
===================
Mod_DecompressVis
===================
*/

byte *Mod_DecompressVis( const byte *in, model_t *model )
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte		*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

	if( !in )
	{	// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
	
	return decompressed;
}

byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model )
{
	if( !model || !leaf || leaf == model->leafs || !model->visdata )
		return Mod_DecompressVis( NULL, model );

	return Mod_DecompressVis( leaf->compressed_vis, model );
}

/*
==================
Mod_PointInLeaf

==================
*/
mleaf_t *Mod_PointInLeaf( Vector p, mnode_t *node )
{
	while( 1 )
	{
		if( node->contents < 0 )
			return (mleaf_t *)node;
		node = node->children[PlaneDiff( p, node->plane ) < 0];
	}

	// never reached
	return NULL;
}

byte *Mod_GetCurrentVis( void )
{
	return RI->visbytes;
//	return Mod_LeafPVS( r_viewleaf, worldmodel );
}

bool Mod_CheckBoxVisible( const Vector &absmin, const Vector &absmax )
{
	return Mod_BoxVisible( absmin, absmax, Mod_GetCurrentVis( ));
}

bool Mod_CheckEntityPVS( cl_entity_t *ent )
{
	if( !ent ) return false; // bad entity?

	if( ent->curstate.messagenum != r_currentMessageNum )
		return false; // already culled by server

	Vector mins = ent->curstate.origin + ent->curstate.mins;
	Vector maxs = ent->curstate.origin + ent->curstate.maxs;

	return Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( ));
}

bool Mod_CheckEntityLeafPVS( const Vector &absmin, const Vector &absmax, mleaf_t *leaf )
{
	return Mod_BoxVisible( absmin, absmax, Mod_LeafPVS( leaf, worldmodel ));
}

bool Mod_CheckTempEntityPVS( TEMPENTITY *pTemp )
{
	if( !pTemp ) return false; // not exist on the client

	Vector mins = pTemp->entity.curstate.origin + pTemp->entity.curstate.mins;
	Vector maxs = pTemp->entity.curstate.origin + pTemp->entity.curstate.maxs;

	return Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( ));
}

int Mod_GetType( int modelIndex )
{
	model_t 	*m_pModel;

	m_pModel = IEngineStudio.GetModelByIndex( modelIndex );
	if( m_pModel == NULL )
		return mod_bad;

	return m_pModel->type;
}

/*
===================
Mod_GetFrames
===================
*/
void Mod_GetFrames( int modelIndex, int &numFrames )
{
	model_t 	*m_pModel;

	m_pModel = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pModel )
	{
		numFrames = 1;
		return;
	}

	numFrames = m_pModel->numframes;
	if( numFrames < 1 ) numFrames = 1;
}


#define MAX_POLYGON_POINTS	64
#define PLANESIDE_FRONT	1
#define PLANESIDE_BACK	2
#define PLANESIDE_ON	3

/*
==================
R_ClipPolygon
==================
*/
bool R_ClipPolygon( int numPoints, Vector *points, const mplane_t *plane, int *numClipped, Vector *clipped )
{
	float	dists[MAX_POLYGON_POINTS];
	int	sides[MAX_POLYGON_POINTS];
	bool	frontSide, backSide;
	float	frac;
	int	i;

	if( numPoints >= MAX_POLYGON_POINTS - 2 )
		HOST_ERROR( "R_ClipPolygon: MAX_POLYGON_POINTS hit\n" );

	*numClipped = 0;

	// Determine sides for each point
	frontSide = false;
	backSide = false;

	for( i = 0; i < numPoints; i++ )
	{
		dists[i] = PlaneDiff( points[i], plane );

		if( dists[i] > ON_EPSILON )
		{
			sides[i] = PLANESIDE_FRONT;
			frontSide = true;
			continue;
		}

		if( dists[i] < -ON_EPSILON )
		{
			sides[i] = PLANESIDE_BACK;
			backSide = true;
			continue;
		}

		sides[i] = PLANESIDE_ON;
	}

	if( !frontSide )
		return false;	// Not clipped

	if( !backSide )
	{
		*numClipped = numPoints;
		memcpy( clipped, points, numPoints * sizeof( Vector ));

		return true;
	}

	// xlip it
	points[i] = points[0];
	dists[i] = dists[0];
	sides[i] = sides[0];

	for( i = 0; i < numPoints; i++ )
	{
		if( sides[i] == PLANESIDE_ON )
		{
			clipped[(*numClipped)++] = points[i];
			continue;
		}

		if( sides[i] == PLANESIDE_FRONT )
			clipped[(*numClipped)++] = points[i];

		if( sides[i+1] == PLANESIDE_ON || sides[i+1] == sides[i] )
			continue;

		if( dists[i] == dists[i+1] )
		{
			clipped[(*numClipped)++] = points[i];
		}
		else
		{
			frac = dists[i] / (dists[i] - dists[i+1]);
			clipped[(*numClipped)++] = points[i] + (points[i+1] - points[i]) * frac;
		}
	}

	return true;
}

/*
==================
R_SplitPolygon
==================
*/
void R_SplitPolygon( int numPoints, Vector *points, const mplane_t *plane, int *numFront, Vector *front, int *numBack, Vector *back )
{
	float	dists[MAX_POLYGON_POINTS];
	int	sides[MAX_POLYGON_POINTS];
	bool	frontSide, backSide;
	Vector	mid;
	float	frac;
	int	i;

	if( numPoints >= MAX_POLYGON_POINTS - 2 )
		HOST_ERROR( "R_SplitPolygon: MAX_POLYGON_POINTS hit\n" );

	*numFront = 0;
	*numBack = 0;

	// Determine sides for each point
	frontSide = false;
	backSide = false;

	for( i = 0; i < numPoints; i++ )
	{
		dists[i] = PlaneDiff( points[i], plane );

		if( dists[i] > ON_EPSILON )
		{
			sides[i] = PLANESIDE_FRONT;
			frontSide = true;
			continue;
		}

		if( dists[i] < -ON_EPSILON )
		{
			sides[i] = PLANESIDE_BACK;
			backSide = true;
			continue;
		}

		sides[i] = PLANESIDE_ON;
	}

	if( !frontSide )
	{
		*numBack = numPoints;
		memcpy( back, points, numPoints * sizeof( Vector ));
		return;
	}

	if( !backSide )
	{
		*numFront = numPoints;
		memcpy( front, points, numPoints * sizeof( Vector ));
		return;
	}

	// split it
	points[i] = points[0];

	dists[i] = dists[0];
	sides[i] = sides[0];

	for( i = 0; i < numPoints; i++ )
	{
		if( sides[i] == PLANESIDE_ON )
		{
			front[(*numFront)++] = points[i];
			back[(*numBack)++] = points[i];
			continue;
		}

		if( sides[i] == PLANESIDE_FRONT )
			front[(*numFront)++] = points[i];

		if( sides[i] == PLANESIDE_BACK )
			back[(*numBack)++] = points[i];

		if( sides[i+1] == PLANESIDE_ON || sides[i+1] == sides[i] )
			continue;

		if( dists[i] == dists[i+1] )
		{
			front[(*numFront)++] = points[i];
			back[(*numBack)++] = points[i];
		}
		else
		{
			frac = dists[i] / (dists[i] - dists[i+1]);
			mid = points[i] + (points[i+1] - points[i]) * frac;
			front[(*numFront)++] = mid;
			back[(*numBack)++] = mid;
		}
	}
}

/*
==================
R_TransformWorldToDevice
==================
*/
void R_TransformWorldToDevice( const Vector &world, Vector &ndc )
{
	Vector4D	clip;
	float	scale;

	clip[0] = world[0] * RI->worldviewProjectionMatrix[0][0] + world[1] * RI->worldviewProjectionMatrix[1][0] + world[2] * RI->worldviewProjectionMatrix[2][0] + RI->worldviewProjectionMatrix[3][0];
	clip[1] = world[0] * RI->worldviewProjectionMatrix[0][1] + world[1] * RI->worldviewProjectionMatrix[1][1] + world[2] * RI->worldviewProjectionMatrix[2][1] + RI->worldviewProjectionMatrix[3][1];
	clip[2] = world[0] * RI->worldviewProjectionMatrix[0][2] + world[1] * RI->worldviewProjectionMatrix[1][2] + world[2] * RI->worldviewProjectionMatrix[2][2] + RI->worldviewProjectionMatrix[3][2];
	clip[3] = world[0] * RI->worldviewProjectionMatrix[0][3] + world[1] * RI->worldviewProjectionMatrix[1][3] + world[2] * RI->worldviewProjectionMatrix[2][3] + RI->worldviewProjectionMatrix[3][3];

	if( clip[3] == 0.0f )
	{
		ndc[0] = clip[0];
		ndc[1] = clip[1];
		ndc[2] = -1.0f;
	}
	else
	{
		scale = 1.0f / clip[3];
		ndc[0] = clip[0] * scale;
		ndc[1] = clip[1] * scale;
		ndc[2] = clip[2] * scale;
	}
}

/*
==================
R_TransformDeviceToScreen
==================
*/
void R_TransformDeviceToScreen( const Vector &ndc, Vector &screen )
{
	screen[0] = (ndc[0] * 0.5f + 0.5f) * (RI->viewport[2] - RI->viewport[0]) + RI->viewport[0];
	screen[1] = (ndc[1] * 0.5f + 0.5f) * (RI->viewport[3] - RI->viewport[1]) + RI->viewport[1];
	screen[2] = (ndc[2] * 0.5f + 0.5f);
}

/*
==================
R_ScissorForBounds
==================
*/
static bool R_ScissorForBounds( const Vector bbox[8], float *x, float *y, float *w, float *h )
{
	static int	cornerIndices[6][4] = {{3, 2, 6, 7}, {0, 1, 5, 4}, {2, 3, 1, 0}, {4, 5, 7, 6}, {1, 3, 7, 5}, {2, 0, 4, 6}};
	Vector		points[2][MAX_POLYGON_POINTS];
	float		ix1, iy1, ix2, iy2;
	float		x1, y1, x2, y2;
	int		numPoints;
	Vector		bounds[2];
	Vector		ndc;
	int		pingPong = 0;
	int		i, j;

	// Clip the light volume to the view frustum
	ClearBounds( bounds[0], bounds[1] );

	for( i = 0; i < 6; i++ )
	{
		numPoints = 4;

		points[pingPong][0] = bbox[cornerIndices[i][0]];
		points[pingPong][1] = bbox[cornerIndices[i][1]];
		points[pingPong][2] = bbox[cornerIndices[i][2]];
		points[pingPong][3] = bbox[cornerIndices[i][3]];

		for( j = 0; j < FRUSTUM_PLANES; j++ )
		{
			if( !FBitSet( RI->frustum.GetClipFlags(), BIT( j )))
				continue;

			if( !R_ClipPolygon( numPoints, points[pingPong], RI->frustum.GetPlane( j ), &numPoints, points[!pingPong] ))
				break;

			pingPong ^= 1;
		}

		if( j != FRUSTUM_PLANES )
			continue;

		// Add the clipped points
		for( j = 0; j < numPoints; j++ )
		{
			// Transform into normalized device coordinates
			R_TransformWorldToDevice( points[pingPong][j], ndc );

			// Add it
			AddPointToBounds( ndc, bounds[0], bounds[1] );
		}
	}

	// If completely clipped away, clear the scissor
	if( BoundsIsCleared( bounds[0], bounds[1] ))
		return false;

	// Transform into screen space
	R_TransformDeviceToScreen( bounds[0], bounds[0] );
	R_TransformDeviceToScreen( bounds[1], bounds[1] );

	x1 = bounds[0].x - 1.0f;
	y1 = bounds[0].y - 1.0f;
	x2 = bounds[1].x + 1.0f;
	y2 = bounds[1].y + 1.0f;

	ix1 = Q_max( x1, RI->viewport[0] );
	ix2 = Q_min( x2, RI->viewport[2] );
	iy1 = Q_max( y1, RI->viewport[1] );
	iy2 = Q_min( y2, RI->viewport[3] );

	*x = ix1 + 1.0f;
	*y = RI->viewport[3] - iy2; // OpenGL specifics
	*w = ix2 - ix1 - 1.0f;
	*h = iy2 - iy1 - 1.0f;

	// headshield issues
	if( ix1 >= ix2 || iy1 >= iy2 )
		return false;
	return true;
}

void R_BoundsForAABB( const Vector &absmin, const Vector &absmax, Vector bbox[8] )
{
	ASSERT( bbox != NULL );

	// compute a full bounding box
	for( int i = 0; i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? absmin[0] : absmax[0];
  		bbox[i][1] = ( i & 2 ) ? absmin[1] : absmax[1];
  		bbox[i][2] = ( i & 4 ) ? absmin[2] : absmax[2];
	}
}

bool R_ScissorForAABB( const Vector &absmin, const Vector &absmax, float *x, float *y, float *w, float *h )
{
	Vector bbox[8];

	R_BoundsForAABB( absmin, absmax, bbox );

	return R_ScissorForBounds( bbox, x, y, w, h );
}

bool R_ScissorForCorners( const Vector bbox[8], float *x, float *y, float *w, float *h )
{
	return R_ScissorForBounds( bbox, x, y, w, h );
}

bool R_AABBToScreen( const Vector &absmin, const Vector &absmax, Vector2D &scrmin, Vector2D &scrmax, wrect_t *rect )
{
	float x, y, w, h;
	Vector bbox[8];

	R_BoundsForAABB( absmin, absmax, bbox );
	ClearBounds( scrmin, scrmax );

	if( !R_ScissorForBounds( bbox, &x, &y, &w, &h ))
	{
		if( rect ) memset( rect, 0, sizeof( *rect ));
		return false;
          }

	// copy rectangle
	if( rect )
	{
		rect->left = x;
		rect->right = RI->viewport[3] - h - y; // left bottom corner
		rect->top = w;
		rect->bottom = h;
	}

	// compute a bounding box
	AddPointToBounds( Vector2D( x, y ), scrmin, scrmax );
	AddPointToBounds( Vector2D( x + w, y ), scrmin, scrmax );
	AddPointToBounds( Vector2D( x, y + h ), scrmin, scrmax );
	AddPointToBounds( Vector2D( x + w, y + h ), scrmin, scrmax );

	return true;
}

/*
====================
V_CalcFov
====================
*/
float V_CalcFov( float &fov_x, float width, float height )
{
	float x, half_fov_y;

	if( fov_x < 1 || fov_x > 170 )
	{
		ALERT( at_error, "V_CalcFov: bad fov %g!\n", fov_x );
		fov_x = 90;
	}

	x = width / tan( DEG2RAD( fov_x ) * 0.5f );
	half_fov_y = atan( height / x );

	return RAD2DEG( half_fov_y ) * 2;
}

/*
====================
V_AdjustFov
====================
*/
void V_AdjustFov( float &fov_x, float &fov_y, float width, float height, bool lock_x )
{
	float x, y;

	if(( width * 3 ) == ( 4 * height ) || ( width * 4 ) == ( height * 5 ))
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if( lock_x )
	{
		fov_y = 2 * atan(( width * 3 ) / ( height * 4 ) * tan( fov_y * M_PI / 360.0 * 0.5 )) * 360 / M_PI;
		return;
	}

	y = V_CalcFov( fov_x, 640, 480 );
	x = fov_x;

	fov_x = V_CalcFov( y, height, width );

	if( fov_x < x )
		fov_x = x;
	else fov_y = y;
}

/*
====================
SPRITE_GetList

====================
*/
char *ParseHudSprite( char *pfile, char *psz, client_sprite_t *result )
{
	client_sprite_t tempSprite;
	int x = 0, y = 0, width = 0, height = 0;
	int section = 0;
	char token[256];
	
	memset( &tempSprite, 0, sizeof( tempSprite ));
	memset( result, 0, sizeof( *result ));
          
	while( pfile )
	{
		pfile = COM_ParseFile( pfile, token );	

		if( !Q_stricmp( token, psz ))
		{
			pfile = COM_ParseFile( pfile, token );	
			if( !Q_stricmp( token, "{" )) section = 1;
		}

		if( section )
		{
			if( !Q_stricmp( token, "}" ))
				break; // end section
			
			if( !Q_stricmp( token, "file" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				Q_strcpy( tempSprite.szSprite, token );
				void *testSprite;

				if(( testSprite = gEngfuncs.COM_LoadFile( tempSprite.szSprite, 5, NULL )) != NULL )
				{
					gEngfuncs.COM_FreeFile( testSprite );

					// fill structure at default
					HSPRITE m_hSprite = SPR_Load( tempSprite.szSprite );

					width = SPR_Width( m_hSprite, 0 );
					height = SPR_Height( m_hSprite, 0 );
					x = y = 0;
				}
				else
				{
					return pfile;
				}
			}
			else if( !Q_stricmp( token, "name" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				Q_strcpy( tempSprite.szName, token );
			}
			else if( !Q_stricmp( token, "x" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				x = Q_atoi( token );
			}
			else if( !Q_stricmp( token, "y" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				y = Q_atoi( token );
			}
			else if( !Q_stricmp( token, "width" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				width = Q_atoi( token );
			}
			else if( !Q_stricmp( token, "height" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				height = Q_atoi( token );
			}
		}
	}

	if( !section )
		return pfile; // data not found

	// calculate sprite position
	tempSprite.rc.left = x;
	tempSprite.rc.right = x + width; 
	tempSprite.rc.top = y;
	tempSprite.rc.bottom = y + height;

	// write resolution for backward compatibility
	if( ScreenWidth < 640 )
		tempSprite.iRes = 320;
	else tempSprite.iRes = 640;

	*result = tempSprite;

	return pfile;
}

/* =================================
	GetSpriteList

Finds and returns the matching 
sprite name 'psz' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount )
{
	if( !pList )
		return NULL;

	client_sprite_t *p = pList;
	int i = iCount;

	while( i-- )
	{
		if(( !Q_strcmp( psz, p->szName )) && ( p->iRes == iRes ))
			return p;
		p++;
	}
	return NULL;
}

/*
====================
Sys LoadGameDLL

====================
*/
bool Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunc_t *fcts )
{
	if( !handle ) return false;

	const dllfunc_t *gamefunc;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->func = NULL;

	char dllpath[128];

	// is direct path used ?
	if( dllname[0] == '*' ) Q_strncpy( dllpath, dllname + 1, sizeof( dllpath ));
	else Q_snprintf( dllpath, sizeof( dllpath ), "%s/bin/%s", gEngfuncs.pfnGetGameDirectory(), dllname );

	dllhandle_t dllhandle = LoadLibrary( dllpath );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if( !( *gamefunc->func = (void *)Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_FreeLibrary( &dllhandle );
			return false;
		}
	}          

	ALERT( at_aiconsole, "%s loaded succesfully!\n", (dllname[0] == '*') ? (dllname+1) : (dllname));
	*handle = dllhandle;

	return true;
}

void *Sys_GetProcAddress( dllhandle_t handle, const char *name )
{
	return (void *)GetProcAddress( handle, name );
}

void Sys_FreeLibrary( dllhandle_t *handle )
{
	if( !handle || !*handle )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}
