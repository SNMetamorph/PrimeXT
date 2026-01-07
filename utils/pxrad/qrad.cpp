/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// qrad.c

#include "qrad.h"
#include "app_info.h"
#include "crashhandler.h"
#include "build_info.h"

/*
NOTES
-----
every surface must be divided into at least two patches each axis
*/

patch_t		*g_face_patches[MAX_MAP_FACES];
entity_t	*g_face_entity[MAX_MAP_FACES];

int			g_numskynormals[SKYLEVELMAX+1];
vec3_t		*g_skynormals[SKYLEVELMAX+1];
patch_t		*g_patches;
uint		g_num_patches;
static vec3_t	(*emitlight)[MAXLIGHTMAPS];
static vec3_t	(*addlight)[MAXLIGHTMAPS];
static byte	(*newstyles)[MAXLIGHTMAPS];
#ifdef HLRAD_DELUXEMAPPING
static vec3_t	(*emitlight_dir)[MAXLIGHTMAPS];
static vec3_t	(*addlight_dir)[MAXLIGHTMAPS];
#endif
vec3_t		g_face_offset[MAX_MAP_FACES];		// for rotating bmodels
dplane_t	g_backplanes[MAX_MAP_PLANES];		// equal to MAX_MAP_FACES, there is no errors
int			g_overflowed_styles_onface[MAX_THREADS];
int			g_overflowed_styles_onpatch[MAX_THREADS];
int			g_direct_luxels[MAX_THREADS];
int			g_lighted_luxels[MAX_THREADS];

bool		g_texture_init[MAX_MAP_TEXTURES];
static winding_t	*g_windingArray[MAX_SUBDIVIDE];
static uint	g_numwindings = 0;
static size_t	g_transfer_data_bytes;
size_t		g_transfer_data_size[MAX_THREADS];
uint		g_numbounce = DEFAULT_BOUNCE;		// originally this was 8
vec_t		g_chop = DEFAULT_CHOP;
vec_t		g_texchop = DEFAULT_TEXCHOP;
vec_t		g_smoothvalue = DEFAULT_SMOOTHVALUE;
float		g_totalarea;
bool		g_fastmode = DEFAULT_FASTMODE;
vec_t		g_direct_scale = DEFAULT_DLIGHT_SCALE;
vec_t		g_indirect_scale = DEFAULT_LIGHT_SCALE;
vec_t		g_blur = DEFAULT_BLUR;
vec_t		g_gamma = DEFAULT_GAMMA;

vec3_t		g_reflectivity[MAX_MAP_TEXTURES];
vec_t		g_texgamma = DEFAULT_TEXREFLECTGAMMA;
bool		g_hdrcompresslog = false;
bool		g_tonemap = false;
bool		g_accurate_trans = false;
bool		g_fastsky = false;
bool		g_nolerp = false;
bool		g_envsky = false;
bool		g_solidsky = false;
bool		g_aa = false;
bool		g_delambert = false;
bool		g_worldspace = false;
bool		g_studiolegacy = false;
vec_t		g_scale = DEFAULT_GLOBAL_SCALE;
rgbdata_t	*g_skytextures[6];
vec_t		g_lightprobeepsilon = DEFAULT_LIGHTPROBE_EPSILON;
int			g_lightprobesamples = 256;
bool		g_vertexblur = false;
uint		g_numstudiobounce = DEFAULT_STUDIO_BOUNCE;
int			g_studiogipasscounter = 0;
bool		g_noemissive = false;
int			g_skystyle = -1;
bool		g_perpixelsky = false;
bool		g_usingpatches = true;
bool		g_patchaa = false;



bool		g_drawsample = false;
vec3_t		g_ambient = { 0, 0, 0 };
float		g_maxlight = DEFAULT_LIGHTCLIP;	// originally this was 196
float		g_indirect_sun = DEFAULT_INDIRECT_SUN;
bool		g_dirtmapping = DEFAULT_DIRTMAPPING;
bool		g_lerp_enabled = DEFAULT_LERP_ENABLED;
bool		g_extra = DEFAULT_EXTRAMODE;
bool		g_nomodelshadow = false;
bool		g_lightbalance = false;
bool		g_onlylights = false;
float		g_smoothing_threshold;		// cosine of smoothing angle(in radians)
char		source[MAX_PATH] = "";
uint		g_gammamode = DEFAULT_GAMMAMODE;
size_t		g_compatibility_mode = DEFAULT_COMPAT_MODE;

static char	global_lights[MAX_PATH] = "";
static char	level_lights[MAX_PATH] = "";

// pre-quantized table normals from Quake1
vec_t g_anorms[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

vec3_t	g_skynormals_random[SKYNORMALS_RANDOM] =
{
#include "anorms_rnd_16k.h"
};

/*
===================================================================

MISC

===================================================================
*/
const dplane_t *GetPlaneFromFace( const dface_t *face )
{
	ASSERT( face != NULL );

	if( face->side )
		return &g_backplanes[face->planenum];
	return &g_dplanes[face->planenum];
}

const dplane_t *GetPlaneFromFace( const uint facenum )
{
	ASSERT( facenum < MAX_MAP_FACES );

	dface_t	*face = &g_dfaces[facenum];

	if( face->side )
		return &g_backplanes[face->planenum];
	return &g_dplanes[face->planenum];
}

dleaf_t *PointInLeaf( const vec3_t point )
{
	int	nodenum = 0;

	while( nodenum >= 0 )
	{
		dnode_t	*node = &g_dnodes[nodenum];

		if( PlaneDiff( point, &g_dplanes[node->planenum] ) > 0 )
			nodenum = node->children[0];
		else nodenum = node->children[1];
	}

	return &g_dleafs[-nodenum - 1];
}

dleaf_t *PointInLeaf_r( int nodenum, const vec3_t point )
{
	dplane_t	*plane;
	dnode_t	*node;
	vec_t	dist;

	while( nodenum >= 0 )
	{
		node = &g_dnodes[nodenum];
		plane = &g_dplanes[node->planenum];
		dist = DotProduct( point, plane->normal ) - plane->dist;

		if( dist > HUNT_WALL_EPSILON )
		{
			nodenum = node->children[0];
		}
		else if( dist < -HUNT_WALL_EPSILON )
		{
			nodenum = node->children[1];
		}
		else
		{
			dleaf_t	*result[2];

			result[0] = PointInLeaf_r( node->children[0], point );
			result[1] = PointInLeaf_r( node->children[1], point );

			if( result[0] == g_dleafs || result[0]->contents == CONTENTS_SOLID )
				return result[0];

			if( result[1] == g_dleafs || result[1]->contents == CONTENTS_SOLID )
				return result[1];

			if( result[0]->contents == CONTENTS_SKY )
				return result[0];

			if( result[1]->contents == CONTENTS_SKY )
				return result[1];

			return result[0];
		}
	}

	return &g_dleafs[-nodenum - 1];
}

dleaf_t *PointInLeaf2( const vec3_t point )
{
	return PointInLeaf_r( 0, point );
}

/*
==============
GetVisCache

decompress visibility string
==============
*/
int GetVisCache( int lastoffset, int offset, byte *pvs )
{
	// get the PVS for the pos to limit the number of checks
	if( !g_visdatasize )
	{
		if( offset != lastoffset )
		{
			memset( pvs, 255, (g_numvisleafs + 7) / 8 );
			lastoffset = -1;
		}
	}
	else if( offset != lastoffset )
	{
		if( offset == -1 ) memset( pvs, 0, (g_numvisleafs + 7) / 8 );
		else DecompressVis( &g_dvisdata[offset], pvs );
		lastoffset = offset;
	}

	return lastoffset;
}

/*
==============
SetDLightVis

Init dlight visibility
==============
*/
void SetDLightVis( directlight_t *dl, int leafnum )
{
	if( !dl->pvs )
		dl->pvs = (byte *)Mem_Alloc( (g_numvisleafs + 7) / 8 );
	GetVisCache( -2, g_dleafs[leafnum].visofs, dl->pvs );
}

/*
==============
MergeDLightVis

Merge dlight vis with another leaf
==============
*/
void MergeDLightVis( directlight_t *dl, int leafnum )
{
	if( !dl->pvs )
	{
		SetDLightVis( dl, leafnum );
	}
	else
	{
		byte	pvs[(MAX_MAP_LEAFS + 7) / 8];
	
		GetVisCache( -2, g_dleafs[leafnum].visofs, pvs );

		// merge both vis graphs
		for( int i = 0; i < (g_numvisleafs + 7) / 8; i++ )
			dl->pvs[i] |= pvs[i];
	}
}

/*
==============
PatchPlaneDist

Fixes up patch planes for brush models with an origin brush
==============
*/
vec_t PatchPlaneDist( patch_t *patch )
{
	const dplane_t	*plane = GetPlaneFromFace( patch->faceNumber );

	return DotProduct( g_face_offset[patch->faceNumber], plane->normal ) + plane->dist;
}

bool PvsForOrigin( const vec3_t org, byte *pvs )
{
	dleaf_t	*leaf;

	if( !g_visdatasize )
	{
		memset( pvs, 255, (g_numvisleafs + 7) / 8 );
		return true;
	}

	leaf = PointInLeaf( org );
	if( leaf->visofs == -1 )
		COM_FatalError( "leaf->visofs == -1\n" );

	if( leaf->contents == CONTENTS_SOLID )
		return false;

	DecompressVis( &g_dvisdata[leaf->visofs], pvs );
	return true;
}

/*
=============
HuntForWorld

never return CONTENTS_SKY or CONTENTS_SOLID leafs
=============
*/
dleaf_t *HuntForWorld( vec3_t point, const vec3_t plane_offset, const dplane_t *plane, int hunt_size, vec_t hunt_scale, vec_t hunt_offset )
{
	vec3_t	current_point;
	vec3_t	original_point;
	dleaf_t	*best_leaf = NULL;
	vec_t	best_dist = 99999999.0;
	dplane_t	new_plane = *plane;
	int	a, x, y, z;
	vec3_t	best_point;
	vec3_t	scales;
	dleaf_t	*leaf;

	scales[0] = 0.0;
	scales[1] = -hunt_scale;
	scales[2] = hunt_scale;

	VectorCopy( point, best_point );
	VectorCopy( point, original_point );

	new_plane.dist += DotProduct( plane->normal, plane_offset );

	for( a = 0; a < hunt_size; a++ )
	{
		for( x = 0; x < 3; x++ )
		{
			current_point[0] = original_point[0] + (scales[x % 3] * a);

			for( y = 0; y < 3; y++ )
			{
				current_point[1] = original_point[1] + (scales[y % 3] * a);

				for( z = 0; z < 3; z++ )
				{
					if( a == 0 && ( x || y || z ))
					{
						continue;
					}

					vec3_t	delta;
					vec_t	dist;

					current_point[2] = original_point[2] + (scales[z % 3] * a);

					dist = DotProduct( current_point, new_plane.normal ) - new_plane.dist - hunt_offset;
					VectorMA( current_point, -dist, new_plane.normal, current_point );
					VectorSubtract( current_point, original_point, delta );
					dist = DotProduct( delta, delta );

					if( dist < best_dist )
					{
						if(( leaf = PointInLeaf2( current_point )) != g_dleafs )
						{
							if(( leaf->contents != CONTENTS_SKY ) && ( leaf->contents != CONTENTS_SOLID ))
							{
								if( x || y || z )
								{
									VectorCopy( current_point, best_point );
									best_dist = dist;
									best_leaf = leaf;
									continue;
								}
								else
								{
									VectorCopy( current_point, point );
									return leaf;
								}
							}
						}
					}
				}
			}
		}

		if( best_leaf )
			break;
	}

	VectorCopy( best_point, point );
	return best_leaf;
}

dworldlight_t *AllocWorldLight( word *lightnum )
{
	if( g_numworldlights == MAX_MAP_WORLDLIGHTS )
		COM_FatalError( "MAX_MAP_WORLDLIGHTS limit exceeded\n" );

	if( lightnum ) *lightnum = g_numworldlights;

	return &g_dworldlights[g_numworldlights++];
}

void InitWorldLightFromDlight( directlight_t *dl, int leafnum )
{
	dworldlight_t	*wl;

	if( dl->type != emit_skylight && leafnum == 0 )
		return; // light that be removed

	wl = AllocWorldLight( &dl->lightnum );

	wl->emittype = dl->type;
	wl->style = dl->style;
	VectorCopy( dl->origin, wl->origin );
	VectorCopy( dl->intensity, wl->intensity );
	VectorCopy( dl->normal, wl->normal );
	wl->stopdot = dl->stopdot;
	wl->stopdot2 = dl->stopdot2;
	wl->fade = dl->fade;
	wl->falloff = dl->falloff;
	wl->leafnum = leafnum;	// leaf where light is located
	wl->radius = dl->radius;
	wl->facenum = dl->facenum;
	wl->modelnumber = dl->modelnum;
}

void InitWorldLightFromPatch( dworldlight_t *wl, patch_t *p )
{
	dleaf_t	*leaf;

	leaf = PointInLeaf( p->origin );

	wl->emittype = emit_surface;
	wl->style = p->emitstyle;
	VectorCopy( p->origin, wl->origin );

	VectorCopy( p->baselight, wl->intensity );
	VectorScale( wl->intensity, p->area, wl->intensity );
	VectorScale( wl->intensity, p->exposure, wl->intensity );

	if( !VectorIsNull( p->reflectivity ))
		VectorMultiply( wl->intensity, p->reflectivity, wl->intensity );

	VectorCopy( GetPlaneFromFace( p->faceNumber )->normal, wl->normal );
	wl->stopdot = wl->stopdot2 = 0.0f;	// not used
	wl->fade = p->fade;			// constant
	wl->falloff = falloff_valve;		// inverse square falloff
	wl->leafnum = leaf - g_dleafs;	// leaf where light is located
	wl->radius = VectorLength2( wl->intensity ) * 0.5f;
	wl->facenum = p->faceNumber;
	wl->modelnumber = p->modelnum;
}

vec_t IntegrateEdge( const vec3_t v1, const vec3_t v2 )
{
	vec_t	cosTheta;
    vec_t	theta;    
    vec_t	res;
	vec3_t	vtemp;

	cosTheta = DotProduct( v1, v2 );
	theta = AcosFast(cosTheta);
	CrossProduct( v1, v2, vtemp );
	res = vtemp[2];
	if( theta > 0.001f )
		res *= theta / sin( theta );
	return res;
}

vec_t CalcPatchLambert( const vec3_t receiver_origin, const vec3_t t,const vec3_t b, const vec3_t n, const winding_t *emitter_winding )
{
	int		numedges = emitter_winding->numpoints;
	int		x;
	vec3_t	vcur, vprev, vtemp, vcutstart, vcutend;
	bool	cut_horizon = false;
	vec_t	mix;
	vec_t	sum = 0.0f;	

	VectorSubtract( emitter_winding->p[0], receiver_origin, vcur );
	WorldToTangent( vcur, t, b, n, vtemp );
	VectorCopy( vtemp, vcur );
	VectorNormalize( vcur );

	for( x = 0; x < numedges; x++ )
	{
		VectorCopy( vcur, vprev );

		VectorSubtract( emitter_winding->p[(x + 1) % numedges], receiver_origin, vcur );
		WorldToTangent( vcur, t, b, n, vtemp );
		VectorCopy( vtemp, vcur );

		if( (vprev[2] < 0.0f) && (vcur[2] < 0.0f) )	//both verts below the horizon
			continue;			

		if( vcur[2] < 0.0f )
		{
			mix = vcur[2] / (vcur[2] - vprev[2]);
			vcutstart[0] = vprev[0] * mix + vcur[0] * ( 1.0f - mix );
			vcutstart[1] = vprev[1] * mix + vcur[1] * ( 1.0f - mix );
			vcutstart[2] = 0.0f;
			//VectorNormalize( vprev );
			VectorNormalize( vcutstart );
			sum += IntegrateEdge( vprev, vcutstart );
			cut_horizon = true;
			continue;
		}
		else if( vprev[2] < 0.0f )
		{
			mix = vcur[2] / (vcur[2] - vprev[2]);
			vcutend[0] = vprev[0] * mix + vcur[0] * ( 1.0f - mix );
			vcutend[1] = vprev[1] * mix + vcur[1] * ( 1.0f - mix );
			vcutend[2] = 0.0f;			
			VectorNormalize( vcur );
			VectorNormalize( vcutend );
			sum += IntegrateEdge( vcutend, vcur );
			cut_horizon = true;
			continue;			
		}	
		//VectorNormalize( vprev );
		VectorNormalize( vcur );

		sum += IntegrateEdge( vprev, vcur );
	}

	if( cut_horizon )
		sum += IntegrateEdge( vcutstart, vcutend );

	sum *= 0.5f;	//why?
	sum = Q_max( sum, 0.0f );

    return sum;
}

vec_t CalcFaceSolidAngle( dface_t *f, const vec3_t origin ) 
{
	int	i, se, v;
	dvertex_t	*dv;
	vec3_t r1, r2, r3, cross23;
	vec_t r1l, r2l, r3l;
	vec_t nom, denom;	
	vec_t omega = 0.0f;

	for( i = 0; i < f->numedges; i++ )
	{
		se = g_dsurfedges[f->firstedge + i];
		if( se < 0 ) v = g_dedges[-se].v[1];
		else v = g_dedges[se].v[0];
		dv = &g_dvertexes[v];	

		VectorCopy( dv->point, r3 );
		VectorSubtract( r3, origin, r3 );
		r3l = VectorLength( r3 );

		if( i == 0 )
		{
			VectorCopy( r3, r1 );
			r1l = r3l;
			continue;
		}

		if( i > 1 )
		{
			CrossProduct( r2, r3, cross23 );
			nom = fabsf( DotProduct( r1, cross23 ) );
			denom = r1l * r2l * r3l;
			denom += DotProduct( r1, r2 ) * r3l;
			denom += DotProduct( r2, r3 ) * r1l;
			denom += DotProduct( r3, r1 ) * r2l;

			omega += 2.0f * Atan2Fast( nom , denom );
		}
		
		VectorCopy( r3, r2 );
		r2l = r3l;
	}

	return omega;
}



// =====================================================================================
//  CalcSightArea
// =====================================================================================
vec_t CalcSightArea( const vec3_t receiver_origin, const vec3_t receiver_normal, const winding_t *emitter_winding, int skylevel )
{
	// maybe there are faster ways in calculating the weighted area, but at least this way is not bad.
	int	numedges = emitter_winding->numpoints;
	vec3_t	*edges = (vec3_t *)Mem_Alloc( numedges * sizeof( vec3_t ));
	vec3_t	*pnormal, *pedge;
	vec_t	dot;
	vec_t	area = 0.0f;
	int	i, j, x;

	for( x = 0; x < numedges; x++ )
	{
		vec3_t	v1, v2, normal;

		VectorSubtract( emitter_winding->p[x], receiver_origin, v1 );
		VectorSubtract( emitter_winding->p[(x + 1) % numedges], receiver_origin, v2 );
		CrossProduct( v1, v2, normal ); // pointing inward

		if( !VectorNormalize( normal ))
			break;
		VectorCopy( normal, edges[x] );
	}

	if( x != numedges )
	{
		Mem_Free( edges );
		return area;
	}

	for( i = 0, pnormal = g_skynormals[skylevel]; i < g_numskynormals[skylevel]; i++, pnormal++ )
	{
		dot = DotProduct( *pnormal, receiver_normal );
		if( dot <= 0 ) continue;

		for( j = 0, pedge = edges; j < numedges; j++, pedge++ )
		{
			if( DotProduct( *pnormal, *pedge ) <= 0 )
				break;
		}

		if( j < numedges )
			continue;
		area += dot;
	}

	area /= (float)i;

	area = area * 4.0 * M_PI; // convert to absolute sphere area
	Mem_Free( edges );

	return area;
}

// =====================================================================================
//  GetAlternateOrigin
// =====================================================================================
void GetAlternateOrigin( const vec3_t pos, const vec3_t normal, const patch_t *patch, vec3_t origin )
{
	const dplane_t	*faceplane;
	const vec_t	*faceplaneoffset;
	const vec_t	*facenormal;
	dplane_t		clipplane;
	winding_t		*w;

	faceplane = GetPlaneFromFace( patch->faceNumber );
	faceplaneoffset = g_face_offset[patch->faceNumber];
	facenormal = faceplane->normal;
	VectorCopy( normal, clipplane.normal );
	clipplane.dist = DotProduct( pos, clipplane.normal );

	if( WindingOnPlaneSide( patch->winding, clipplane.normal, clipplane.dist, ON_EPSILON ) != SIDE_CROSS )
	{
		VectorCopy( patch->origin, origin );
	}
	else
	{
		w = CopyWinding( patch->winding );

		ChopWindingInPlace( &w, clipplane.normal, clipplane.dist, ON_EPSILON, false );

		if( w == NULL )
		{
			VectorCopy( patch->origin, origin );
		}
		else
		{
			vec3_t	v, point, bestpoint;
			vec_t	dist, bestdist = -1.0;
			bool	found = false;
			vec3_t	center;

			WindingCenter( w, center );

			VectorMA( center, DEFAULT_HUNT_OFFSET, facenormal, point );

			if( HuntForWorld( point, faceplaneoffset, faceplane, 2, 1.0, DEFAULT_HUNT_OFFSET ))
			{
				VectorSubtract( point, center, v );
				dist = VectorLength( v );

				if( !found || dist < bestdist )
				{
					VectorCopy( point, bestpoint );
					bestdist = dist;
					found = true;
				}
			}

			if( !found )
			{
				for( int i = 0; i < (int)w->numpoints; i++ )
				{
					const vec_t	*p1 = w->p[i];
					const vec_t	*p2 = w->p[(i + 1) % w->numpoints];

					VectorAdd( p1, p2, point );
					VectorAdd( point, center, point );
					VectorScale( point, 1.0 / 3.0, point );
					VectorMA( point, DEFAULT_HUNT_OFFSET, facenormal, point );

					if( HuntForWorld( point, faceplaneoffset, faceplane, 1, 0.0, DEFAULT_HUNT_OFFSET ))
					{
						VectorSubtract( point, center, v );
						dist = VectorLength( v );

						if( !found || dist < bestdist )
						{
							VectorCopy( point, bestpoint );
							bestdist = dist;
							found = true;
						}
					}
				}
			}

			if( found ) VectorCopy( bestpoint, origin );
			else VectorCopy( patch->origin, origin );

			FreeWinding( w );
		}
	}
}

/*
=============
MakeBackplanes
=============
*/
void MakeBackplanes( void )
{
	for( int i = 0; i < g_numplanes; i++ )
	{
		VectorNegate( g_dplanes[i].normal, g_backplanes[i].normal );
		g_backplanes[i].dist = -g_dplanes[i].dist;
	}
}

/*
===================================================================

  TEXTURE LIGHT VALUES

===================================================================
*/
static texlight_t	g_texlights[MAX_TEXLIGHTS];
static int	g_num_texlights;

int ParseInfoTexlights( entity_t *mapent )
{
	int	numtexlights = 0;
	float	r, g, b, i;
	int	j, values;
	epair_t	*ep;

	if( Q_strcmp( ValueForKey( mapent, "classname" ), "info_texlights" ))
		return 0;

	MsgDev( D_REPORT, "Reading texlights from info_texlights map entity\n" );

	for( ep = mapent->epairs; ep; ep = ep->next )
	{
		if( !Q_strcmp( ep->key, "classname" ) || !Q_strcmp( ep->key, "origin" ))
			continue; // we dont care about these keyvalues

		values = sscanf( ep->value, "%f %f %f %f", &r, &g, &b, &i );

		r = powf( r / 255.0f, 2.2f );
		g = powf( g / 255.0f, 2.2f );
		b = powf( b / 255.0f, 2.2f );

		if( values == 1 )
		{  
			g = b = r;
		}
		else if( values == 4 ) // use brightness value.
		{
			r *= i / 255.0f;
			g *= i / 255.0f;
			b *= i / 255.0f;
		}
		else if( values != 3 )
		{
			MsgDev( D_WARN, "ignoring bad texlight '%s' in info_texlights entity", ep->key );
			continue;
		}

		for( j = 0; j < g_num_texlights; j++ )
		{
			texlight_t	*tl = &g_texlights[j];

			if( !Q_strcmp( tl->name, ep->key ))
			{
				if( !Q_strcmp( tl->filename, "info_texlights" ))
				{
					MsgDev( D_REPORT, "duplication of '%s' in info_texlights map entity!\n", tl->name );
				} 
				else if( tl->value[0] != r || tl->value[1] != g || tl->value[2] != b )
				{
					MsgDev( D_REPORT, "overriding '%s' from '%s' with info_texlights map entity!\n",
					tl->name, tl->filename );
				}
				else
				{
					MsgDev( D_WARN, "redundant '%s' def in '%s' AND info_texlights map entity!\n",
					tl->name, tl->filename );
				}
				break;
			}
		}

		Q_strncpy( g_texlights[j].name, ep->key, sizeof( g_texlights[0].name ));
		g_texlights[j].filename = "info_texlights";
		VectorSet( g_texlights[j].value, r, g, b );
		g_texlights[j].fade = 1.0f;
		numtexlights++;

		g_num_texlights = Q_max( g_num_texlights, j + 1 );

		if( g_num_texlights == MAX_TEXLIGHTS )
			COM_FatalError( "MAX_TEXLIGHTS limit exceeded\n" );
	}

	return numtexlights;
}

int ParseSurfaceLights( entity_t *mapent )
{
	char	name[64];
	char	*pLight = NULL;
	char	*pColor = NULL;
	vec3_t	intensity;
	int	j, argCnt;

	if( Q_strcmp( ValueForKey( mapent, "classname" ), "light" ))
		return 0;

	if( !CheckKey( mapent, "_surface" ))
		return 0;

	Q_strncpy( name, ValueForKey( mapent, "_surface" ), sizeof( name ));
	if( CheckKey( mapent, "_light" ))
		pLight = ValueForKey( mapent, "_light" );
	else pLight = ValueForKey( mapent, "light" );
	argCnt = ParseLightIntensity( pLight, intensity );
	if( name[0] == '*' ) name[0] = '!';

	// quake light
	if( argCnt <= 1 )
	{
		double	r, g, b, scaler;

		scaler = intensity[0];
		r = g = b = 0;

		if( CheckKey( mapent, "_color" ))
			pColor = ValueForKey( mapent, "_color" );
		else pColor = ValueForKey( mapent, "color" );

		if( pColor[0] && sscanf( pColor, "%lf %lf %lf", &r, &g, &b ) == 3 )
		{
			intensity[0] = r * (float)scaler;
			intensity[1] = g * (float)scaler;
			intensity[2] = b * (float)scaler;
		}
	}

	for( j = 0; j < g_num_texlights; j++ )
	{
		texlight_t	*tl = &g_texlights[j];

		if( !Q_strcmp( tl->name, name ))
		{
			if( !Q_strcmp( tl->filename, "info_texlights" ))
			{
				MsgDev( D_REPORT, "duplication of '%s' in info_texlights map entity!\n", tl->name );
			} 
			else if( !VectorCompareEpsilon( tl->value, intensity, ON_EPSILON ))
			{
				MsgDev( D_REPORT, "overriding '%s' from '%s' with info_texlights map entity!\n", tl->name, tl->filename );
			}
			else
			{
				MsgDev( D_WARN, "redundant '%s' def in '%s' AND info_texlights map entity!\n", tl->name, tl->filename );
			}
			break;
		}
	}

	Q_strncpy( g_texlights[j].name, name, sizeof( g_texlights[0].name ));
	VectorCopy( intensity, g_texlights[j].value );
	g_texlights[j].filename = "light_surface";
	g_texlights[j].fade = FloatForKey( mapent, "wait" );

	// to prevent division by zero
	if( g_texlights[j].fade <= 0.0 ) g_texlights[j].fade = 1.0;

	g_num_texlights = Q_max( g_num_texlights, j + 1 );
	if( g_num_texlights == MAX_TEXLIGHTS )
		COM_FatalError( "MAX_TEXLIGHTS limit exceeded\n" );

	return 1;
}

// =====================================================================================
//  ReadInfoTexlights
//      try and parse texlight info from the info_texlights entity 
// =====================================================================================
void ReadInfoTexlights( void )
{
	int	numtexlights = 0;
	entity_t	*mapent;

	for( int k = 0; k < g_numentities; k++ )
	{
		mapent = &g_entities[k];

		// all the vertex cache ID's should be removed now
		RemoveKey( mapent, "_vlight_id" );

		numtexlights += ParseInfoTexlights( mapent );

		numtexlights += ParseSurfaceLights( mapent );
	}

	MsgDev( D_REPORT, "[%i texlights parsed from info_texlights map entity]\n\n", numtexlights );
}

/*
============
ReadLightFile
============
*/
void ReadLightFile( const char *filename, bool use_direct_path )
{
	int	file_texlights = 0;
	int	result = 0;
	char	scan[128];
	int	j, argCnt;
	file_t	*f;

	FS_AllowDirectPaths(use_direct_path);
	f = FS_Open(filename, "r", false);
	FS_AllowDirectPaths(false);

	if (!f) 
	{
		MsgDev(D_INFO, "ReadLightFile: failed to parse texlights from '%s'\n", filename);
		return;
	}

	while( result != EOF )
	{
		char	szTexlight[256];
		vec_t	r, g, b, i = 1;
		char	*comment = scan;

		result = FS_Gets( f, (byte *)&scan, sizeof( scan ));

		// skip the comments
		if( comment[0] == '/' && comment[1] == '/' )
			continue;

		argCnt = sscanf( scan, "%s %f %f %f %f", szTexlight, &r, &g, &b, &i );

		r = powf( r / 255.0f, 2.2f );
		g = powf( g / 255.0f, 2.2f );
		b = powf( b / 255.0f, 2.2f );

		//moved here from CreateDirectLights and InitWorldLightFromPatch
		r *= 0.5f / M_PI;
		g *= 0.5f / M_PI;
		b *= 0.5f / M_PI;		

		if( argCnt == 2 )
		{
			// eith 1+1 args, the R,G,B values are all equal to the first value
			g = b = r;
		}
		else if( argCnt == 5 )
		{
			// With 1 + 4 args, the R,G,B values are "scaled" by the fourth numeric value i;
			r *= i / 255.0;
			g *= i / 255.0;
			b *= i / 255.0;
		}
		else if( argCnt != 4 )
		{
			if( Q_strlen( scan ) > 4 )
				MsgDev( D_WARN, "ignoring bad texlight '%s' in %s", scan, filename );
			continue;
		}

		for( j = 0; j < g_num_texlights; j++ )
		{
			texlight_t	*tl = &g_texlights[j];

			if( !Q_strcmp( tl->name, szTexlight ))
			{
				if( !Q_strcmp( tl->filename, filename ))
				{
					MsgDev( D_REPORT, "duplication of '%s' in file '%s'!\n", tl->name, tl->filename );
				} 
				else if( tl->value[0] != r || tl->value[1] != g || tl->value[2] != b )
				{
					MsgDev( D_REPORT, "overriding '%s' from '%s' with '%s'!\n", tl->name, tl->filename, filename );
				}
				else
				{
					MsgDev( D_WARN, "redundant '%s' def in '%s' AND '%s'!\n", tl->name, tl->filename, filename );
				}
				break;
			}
		}

		Q_strncpy( g_texlights[j].name, szTexlight, sizeof( g_texlights[0].name ));
		VectorSet( g_texlights[j].value, r, g, b );
		g_texlights[j].filename = filename;
		g_texlights[j].fade = 1.0f;
		file_texlights++;

		g_num_texlights = Q_max( g_num_texlights, j + 1 );

		if( g_num_texlights == MAX_TEXLIGHTS )
			COM_FatalError( "MAX_TEXLIGHTS limit exceeded\n" );
	}		

	MsgDev( D_INFO, "[%i texlights parsed from '%s']\n\n", file_texlights, filename );
	FS_Close( f );
}

/*
============
LightForTexture
============
*/
vec_t LightForTexture( const char *name, vec3_t result )
{
	VectorClear( result );

	for( int i = 0; i < g_num_texlights; i++ )
	{
		if( !Q_stricmp( name, g_texlights[i].name ))
		{
			VectorCopy( g_texlights[i].value, result );
			MsgDev( D_REPORT, "Texture '%s': baselight is (%f,%f,%f).\n", name, result[0], result[1], result[2] );
			return g_texlights[i].fade;
		}
	}

	return 1.0f;
}

/*
=======================================================================

MAKE FACES

=======================================================================
*/
/*
=============
WindingFromFace
=============
*/
winding_t	*WindingFromFace( const dface_t *f )
{
	int	i, se, v;
	dvertex_t	*dv;
	winding_t	*w;

	w = AllocWinding( f->numedges );
	w->numpoints = f->numedges;

	for( i = 0; i < f->numedges; i++ )
	{
		se = g_dsurfedges[f->firstedge + i];
		if( se < 0 ) v = g_dedges[-se].v[1];
		else v = g_dedges[se].v[0];
		dv = &g_dvertexes[v];
		VectorCopy( dv->point, w->p[i] );
	}

	RemoveColinearPointsEpsilon( w, ON_EPSILON );

	return w;
}

/*
=============
BaseLightForFace
=============
*/
vec_t BaseLightForFace( dface_t *f, vec3_t light, vec3_t reflectivity )
{
	int	miptex = g_texinfo[f->texinfo].miptex;
	vec_t	fade;
	miptex_t	*mt;

	// check for light emited by texture
	mt = GetTextureByMiptex( miptex );
	if( !mt ) return 1.0f;

	fade = LightForTexture( mt->name, light );
	VectorClear( reflectivity );

#ifdef HLRAD_REFLECTIVITY
	int	samples = mt->width * mt->height;
	byte	*pal = ((byte *)mt) + mt->offsets[0] + (((mt->width * mt->height) * 85) >> 6);
	byte	*buf = ((byte *)mt) + mt->offsets[0];
	vec3_t	total;
	int		samples_total = 0;

	// check for cache
	if( g_texture_init[miptex] )
	{
		VectorCopy( g_reflectivity[miptex], reflectivity );
		return fade;
	}

	pal += sizeof( short ); // skip colorsize 
	VectorClear( total );

	for( int i = 0; i < samples; i++ )
	{
		vec3_t	reflectivity;

		if( mt->name[0] == '{' && buf[i] == 0xFF )
		{
			continue;
		}
		else
		{
			int	texel = buf[i];
			reflectivity[0] = pow( pal[texel*3+0] * (1.0f / 255.0f), g_texgamma );
			reflectivity[1] = pow( pal[texel*3+1] * (1.0f / 255.0f), g_texgamma );
			reflectivity[2] = pow( pal[texel*3+2] * (1.0f / 255.0f), g_texgamma );	

			VectorAdd( total, reflectivity, total );
			samples_total++;

		}
	}

	VectorScale( total, DEFAULT_TEXREFLECTSCALE, total );
	VectorScale( total, 1.0 / (double)(samples_total), g_reflectivity[miptex] );
	VectorCopy( g_reflectivity[miptex], reflectivity );
	MsgDev( D_REPORT, "Texture '%s': reflectivity is (%f,%f,%f).\n", mt->name, reflectivity[0], reflectivity[1], reflectivity[2] );
	g_texture_init[miptex] = true;
#endif
	return fade;
}

static void UpdateEmitterInfo( patch_t *patch )
{
	const vec_t	*origin = patch->origin;
	const winding_t	*winding = patch->winding;
	vec_t		radius = ON_EPSILON;

	for( int x = 0; x < winding->numpoints; x++ )
	{
		vec3_t	delta;
		vec_t	dist;
		VectorSubtract( winding->p[x], origin, delta );
		dist = VectorLength( delta );
		radius = Q_max( dist, radius );
	}

	int	skylevel = ACCURATEBOUNCE_DEFAULT_SKYLEVEL;
	vec_t	area = WindingArea( winding );
	vec_t	size = 0.8f;

	if( area < size * radius * radius ) // the shape is too thin
	{
		skylevel++;
		size *= 0.25f;

		if( area < size * radius * radius )
		{
			skylevel++;
			size *= 0.25f;

			if( area < size * radius * radius )
			{
				// stop here
				radius = sqrt( area / size );
				// just decrease the range to limit the use of the new method.
				// because when the area is small, the new method becomes randomized and unstable.
			}
		}
	}

	patch->emitter_range = ACCURATEBOUNCE_THRESHOLD * radius;
	patch->emitter_skylevel = skylevel;
}

// =====================================================================================
//  PlacePatchInside
// =====================================================================================
static bool PlacePatchInside( patch_t *patch )
{
	const vec_t	*face_offset = g_face_offset[patch->faceNumber];
	vec_t		offset = DEFAULT_HUNT_OFFSET;
	vec3_t		v, center, point, bestpoint;
	vec_t		pointsfound, pointstested;
	vec_t		dist, bestdist = -1.0;
	const dplane_t	*plane;
	bool		found;

	plane = GetPlaneFromFace( patch->faceNumber );
	WindingCenter( patch->winding, center );
	pointsfound = pointstested = 0;
	found = false;
	
	VectorMA( center, offset, plane->normal, point );
	pointstested++;

	if( HuntForWorld( point, face_offset, plane, 4, 0.2, offset ) || HuntForWorld( point, face_offset, plane, 4, 0.8, offset ))
	{
		VectorSubtract( point, center, v );
		dist = VectorLength( v );
		pointsfound++;

		if( !found || dist < bestdist )
		{
			VectorCopy( point, bestpoint );
			bestdist = dist;
			found = true;
		}
	}

	for( int i = 0; i < patch->winding->numpoints; i++ )
	{
		const vec_t *p1 = patch->winding->p[i];
		const vec_t *p2 = patch->winding->p[(i+1) % patch->winding->numpoints];

		VectorAdd( p1, p2, point );
		VectorAdd( point, center, point );
		VectorScale( point, 1.0 / 3.0, point );
		VectorMA( point, offset, plane->normal, point );
		pointstested++;

		if( HuntForWorld( point, face_offset, plane, 4, 0.2, offset ) || HuntForWorld( point, face_offset, plane, 4, 0.8, offset ))
		{
			VectorSubtract( point, center, v );
			dist = VectorLength( v );
			pointsfound++;

			if( !found || dist < bestdist )
			{
				VectorCopy( point, bestpoint );
				bestdist = dist;
				found = true;
			}
		}
	}

	patch->exposure = pointsfound / pointstested;

	if( found )
		VectorCopy( bestpoint, patch->origin );
	else
	{
		VectorMA( center, offset, plane->normal, patch->origin );
		SetBits( patch->flags, PATCH_OUTSIDE );
	}

	//multiple trace origins for patch antialising
	const vec3_t	*a, *b, *c;	//triangle vertices	
	vec_t	sample_area = patch->area / (float)PATCH_MAX_TRACE_ORIGINS;
	vec_t	area_counter = 0.0f;
	int		counter = 1;
	vec3_t	*trace_origin = &patch->trace_origins[counter];
	
	WindingCenter( patch->winding, center );
	VectorCopy( patch->origin, patch->trace_origins[0]);

	a = &center;

	for( int j = 0; j < patch->winding->numpoints; j++ )
	{
		b = &patch->winding->p[j];
		c = &patch->winding->p[(j + 1) % patch->winding->numpoints];

		area_counter += TriangleArea( *a, *b, *c );
		while( ((counter * sample_area) < area_counter) && ( counter < PATCH_MAX_TRACE_ORIGINS ) )
		{
			vec_t	sqrt_r1, r2;
			sqrt_r1 = sqrtf( Halton( 2, counter ) );
			r2 = Halton( 3, counter );

			VectorClear( *trace_origin );
			VectorMA( *trace_origin, 1.0f - sqrt_r1, *a, *trace_origin );
			VectorMA( *trace_origin, sqrt_r1 * ( 1.0f - r2 ), *b, *trace_origin );
			VectorMA( *trace_origin, sqrt_r1 * r2, *c, *trace_origin );

			VectorMA( patch->trace_origins[counter], 0.5f, plane->normal, *trace_origin );
		
			dleaf_t	*leaf;
			leaf = PointInLeaf2( *trace_origin );
			if(( leaf->contents == CONTENTS_SKY ) || ( leaf->contents == CONTENTS_SOLID ))
				VectorCopy( patch->origin, *trace_origin );

			counter++;
			trace_origin++;
		}
	}

	return found;
}

// =====================================================================================
//
//    SUBDIVIDE PATCHES
//
// =====================================================================================
// =====================================================================================
//  CutWindingWithGrid
//      Caller must free this returned value at some point
// =====================================================================================
static void CutWindingWithGrid( patch_t *patch, const dplane_t *plA, const dplane_t *plB )
{
	// patch->winding->m_NumPoints must > 0
	// plA->dist and plB->dist will not be used
	winding_t		*winding = NULL;
	vec_t		chop, epsilon;
	const int		max_gridsize = 64;
	vec_t		gridstartA, minA, maxA;
	vec_t		gridstartB, minB, maxB;
	int		gridsizeA;
	int		gridsizeB;
	vec_t		gridchopA;
	vec_t		gridchopB;
	int		i, numstrips;
	
	winding = CopyWinding( patch->winding ); // perform all the operations on the copy
	chop = Q_max( 1.0, patch->chop );
	epsilon = 0.6;

	// optimize the grid
	minA = minB = BOGUS_RANGE;
	maxA = maxB = -BOGUS_RANGE;

	for( int x = 0; x < winding->numpoints; x++ )
	{
		vec_t dotA, dotB;
		vec_t *point;

		point = winding->p[x];
		dotA = DotProduct( point, plA->normal );
		minA = Q_min( minA, dotA );
		maxA = Q_max( maxA, dotA );
		dotB = DotProduct( point, plB->normal );
		minB = Q_min( minB, dotB );
		maxB = Q_max( maxB, dotB );
	}

	gridchopA = chop;
	gridsizeA = (int)ceil(( maxA - minA - 2 * epsilon ) / gridchopA );
	gridsizeA = Q_max( 1, gridsizeA );

	if( gridsizeA > max_gridsize )
	{
		gridsizeA = max_gridsize;
		gridchopA = (maxA - minA) / (vec_t)gridsizeA;
	}
	gridstartA = (minA + maxA) / 2.0 - (gridsizeA / 2.0) * gridchopA;

	gridchopB = chop;
	gridsizeB = (int)ceil(( maxB - minB - 2 * epsilon ) / gridchopB);
	gridsizeB = Q_max( 1, gridsizeB );

	if( gridsizeB > max_gridsize )
	{
		gridsizeB = max_gridsize;
		gridchopB = (maxB - minB) / (vec_t)gridsizeB;
	}
	gridstartB = (minB + maxB) / 2.0 - (gridsizeB / 2.0) * gridchopB;
	
	// cut the winding by the direction of plane A and save into windingArray
	g_numwindings = 0;

	for( i = 1; i < gridsizeA; i++ )
	{
		winding_t	*front = NULL;
		winding_t	*back = NULL;
		vec_t	dist;

		dist = gridstartA + i * gridchopA;
		ClipWindingEpsilon( winding, plA->normal, dist, ON_EPSILON, &front, &back );

		if( !front || WindingOnPlaneSide( front, plA->normal, dist, epsilon ) == SIDE_ON ) // ended
		{
			if( front )
			{
				FreeWinding( front );
				front = NULL;
			}
			if( back )
			{
				FreeWinding( back );
				back = NULL;
			}
			break;
		}

		if( !back || WindingOnPlaneSide( back, plA->normal, dist, epsilon ) == SIDE_ON ) // didn't begin
		{
			if( front )
			{
				FreeWinding( front );
				front = NULL;
			}
			if( back )
			{
				FreeWinding( back );
				back = NULL;
			}
			continue;
		}

		FreeWinding( winding );
		winding = NULL;

		g_windingArray[g_numwindings] = back;
		g_numwindings++;
		back = NULL;

		winding = front;
		front = NULL;
	}

	g_windingArray[g_numwindings] = winding;
	g_numwindings++;
	winding = NULL;

	// cut by the direction of plane B
	numstrips = g_numwindings;

	for( i = 0; i < numstrips; i++ )
	{
		winding_t	*strip = g_windingArray[i];

		g_windingArray[i] = NULL;

		for( int j = 1; j < gridsizeB; j++ )
		{
			winding_t	*front = NULL;
			winding_t	*back = NULL;
			vec_t	dist;

			dist = gridstartB + j * gridchopB;
			ClipWindingEpsilon( strip, plB->normal, dist, ON_EPSILON, &front, &back );

			if( !front || WindingOnPlaneSide( front, plB->normal, dist, epsilon ) == SIDE_ON ) // ended
			{
				if( front )
				{
					FreeWinding( front );
					front = NULL;
				}
				if( back )
				{
					FreeWinding( back );
					back = NULL;
				}
				break;
			}
			if( !back || WindingOnPlaneSide( back, plB->normal, dist, epsilon ) == SIDE_ON ) // didn't begin
			{
				if( front )
				{
					FreeWinding( front );
					front = NULL;
				}
				if( back )
				{
					FreeWinding( back );
					back = NULL;
				}
				continue;
			}

			FreeWinding( strip );
			strip = NULL;

			g_windingArray[g_numwindings] = back;
			g_numwindings++;
			back = NULL;

			strip = front;
			front = NULL;
		}

		g_windingArray[g_numwindings] = strip;
		g_numwindings++;
		strip = NULL;
	}

	FreeWinding( patch->winding );
	patch->winding = NULL;
}

// =====================================================================================
//  GetGridPlanes
//      From patch, determine perpindicular grid planes to subdivide with (returned in planeA and planeB)
//      assume S and T is perpindicular (they SHOULD be in worldcraft 3.3 but aren't always...)
// =====================================================================================
static void GetGridPlanes( const patch_t *p, dplane_t *pl )
{
	const dface_t	*f = g_dfaces + p->faceNumber;
	dtexinfo_t	*tx = &g_texinfo[f->texinfo];
	const dplane_t	*faceplane = GetPlaneFromFace( p->faceNumber );
	dplane_t		*plane = pl;
	float		lmvecs[2][4];

	LightMatrixFromTexMatrix( tx, lmvecs );

	for( int x = 0; x < 2; x++, plane++ )
	{
		// cut the patch along texel grid planes
		vec_t val = DotProduct( faceplane->normal, lmvecs[!x] );
		VectorMA( lmvecs[!x], -val, faceplane->normal, plane->normal );
		VectorNormalize( plane->normal );
		plane->dist = DotProduct( plane->normal, p->origin );
	}
}

// =====================================================================================
//  SubdividePatch
// =====================================================================================
static void SubdividePatch( patch_t *patch )
{
	dplane_t		planes[2];
	dplane_t		*plA = &planes[0];
	dplane_t		*plB = &planes[1];
	patch_t		*new_patch;
	winding_t		**winding;
	int		x;

	memset( g_windingArray, 0, sizeof( g_windingArray ));
	g_numwindings = 0;

	GetGridPlanes( patch, planes );
	CutWindingWithGrid( patch, plA, plB );

	x = 0;
	patch->next = NULL;
	winding = g_windingArray;

	while( *winding == NULL )
	{
		winding++;
		x++;
	}

	patch->winding = *winding;
	winding++;
	x++;

	patch->area = WindingAreaAndBalancePoint( patch->winding, patch->origin );
	SetBits( patch->flags, PATCH_SOURCE );
	PlacePatchInside( patch );
	UpdateEmitterInfo( patch );

	new_patch = &g_patches[g_num_patches];

	for( ; x < g_numwindings; x++, winding++ )
	{
		if( *winding )
		{
			memcpy( new_patch, patch, sizeof( patch_t ));
			ClearBits( new_patch->flags, PATCH_SOURCE );

			new_patch->winding = *winding;
			new_patch->area = WindingAreaAndBalancePoint( new_patch->winding, new_patch->origin );
			SetBits( new_patch->flags, PATCH_CHILD );
			PlacePatchInside( new_patch );
			UpdateEmitterInfo( new_patch );
			g_num_patches++;
			new_patch++;

			if( g_num_patches == MAX_PATCHES )
				COM_FatalError( "MAX_PATCHES limit exceeded\n" );
		}
	}
	// ATTENTION: We let SortPatches relink all the ->next correctly! instead of doing it here too which is somewhat complicated
}

/*
============
getScale

compute scale for patch
============
*/
static vec_t getScale( const patch_t *patch )
{
	dface_t		*f = &g_dfaces[patch->faceNumber];
	dtexinfo_t	*tx = &g_texinfo[f->texinfo];
	const dplane_t	*faceplane = GetPlaneFromFace( f );
	vec3_t		outvecs[2];
	vec_t		scale[2];
	vec_t		dot;

	// snap texture "vecs" to faceplane without affecting texture alignment
	for( int x = 0; x < 2; x++ )
	{
		dot = DotProduct( faceplane->normal, tx->vecs[x] );
		VectorMA( tx->vecs[x], -dot, faceplane->normal, outvecs[x] );
	}
		
	scale[0] = 1.0 / Q_max( NORMAL_EPSILON, VectorLength( outvecs[0] ));
	scale[1] = 1.0 / Q_max( NORMAL_EPSILON, VectorLength( outvecs[1] ));

	// don't care about the angle between vecs[0] and vecs[1]
	// (given the length of "vecs", smaller angle = larger texel area),
	// because gridplanes will have the same angle (also smaller angle = larger patch area)
	return sqrt( scale[0] * scale[1] );
}

/*
============
getEmitMode

some pathches are emit_surfaces
============
*/
static bool getEmitMode( const patch_t *patch )
{
	bool	emitmode = false;
	vec_t	value;

	if( !VectorIsNull( patch->reflectivity )) 
		value = DotProduct( patch->baselight, patch->reflectivity ) / 3.0;
	else value = VectorAvg( patch->baselight );

	// this patch is emitting surface
	if( value > DLIGHT_THRESHOLD )
		emitmode = true;

	return emitmode;
}

/*
============
getChop

compute chop value for patch
============
*/
static vec_t getChop( const patch_t *patch )
{
	vec_t	rval;

	if( FBitSet( patch->flags, PATCH_EMITLIGHT ))
		rval = g_texchop * getScale( patch );
	else rval = g_chop * getScale( patch );

	return rval;
}

/*
=============
IsSpecial
=============
*/
static bool IsSpecial( const dface_t *f )
{
	return FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL );
}

/*
=============
MakePatchForFace
=============
*/
bool MakePatchForFace( int modelnum, int fn, winding_t *w, int style )
{
	dface_t		*f = g_dfaces + fn;
	vec3_t		centroid = { 0, 0, 0 };
	vec3_t		mins, maxs, delta;
	patch_t		*patch;
	vec_t		fade;
	dworldlight_t	*wl;
	int		i;

	// No patches at all for the sky!
	if( IsSpecial( f )) return false;

	if( w->numpoints < 3 ) // g-cont. how this possible?
		return false;

	patch = &g_patches[g_num_patches];
	if( g_num_patches == MAX_PATCHES )
		COM_FatalError( "MAX_PATCHES limit exceeded\n" );
	memset( patch, 0, sizeof( patch_t ));

	patch->area = WindingAreaAndBalancePoint( w, patch->origin );
	patch->modelnum = modelnum;
	patch->faceNumber = fn;
	patch->winding = w;

	// update debug info
	g_totalarea += patch->area;

 	fade = BaseLightForFace( f, patch->baselight, patch->reflectivity );

	for( i = 0; i < MAXLIGHTMAPS; i++ )
		patch->totalstyle[i] = 255;

	if( getEmitMode( patch ))
	{
		SetBits( patch->flags, PATCH_EMITLIGHT );

		if( style )
		{
			patch->totalstyle[0] = style;
			patch->emitstyle = style;
		}
	}

	SetBits( patch->flags, PATCH_SOURCE );
	patch->scale = getScale( patch );
	patch->chop = getChop( patch );
	PlacePatchInside( patch );
	UpdateEmitterInfo( patch );
	patch->fade = fade;

	// NOTE: we alloc worldlights only once per face.
	// all child patches after subdivision has the same lightnum 
	if( FBitSet( patch->flags, PATCH_EMITLIGHT ))
	{
		wl = AllocWorldLight( &patch->lightnum );
		InitWorldLightFromPatch( wl, patch );
	}

	g_face_patches[fn] = patch;
	g_num_patches++;

	// per-face data
	for( i = 0; i < f->numedges; i++ )
	{
		int	edge = g_dsurfedges[f->firstedge + i];

		if( edge > 0 )
		{
			VectorAdd( g_dvertexes[g_dedges[edge].v[0]].point, centroid, centroid );
			VectorAdd( g_dvertexes[g_dedges[edge].v[1]].point, centroid, centroid );
		}
		else
		{
			VectorAdd( g_dvertexes[g_dedges[-edge].v[1]].point, centroid, centroid );
			VectorAdd( g_dvertexes[g_dedges[-edge].v[0]].point, centroid, centroid );
		}
	}

	VectorScale( centroid, 1.0 / (f->numedges * 2), centroid );
	VectorAdd( centroid, g_face_offset[fn], g_face_centroids[fn] );	// save them for generating the patch normals later.

	WindingBounds( patch->winding, mins, maxs );
	VectorSubtract( maxs, mins, delta );

	if( VectorLength( delta ) > patch->chop )
	{
		if( patch->area < 1.0 )
			MsgDev( D_WARN, "patch at face %d tiny area (%4.3f) not subdividing\n", patch->faceNumber, patch->area );
                    else SubdividePatch( patch );
	}

	return true;
}

/*
=============
MakePatches
=============
*/
void MakePatches( void )
{
	int		fn, style = 0;
	vec3_t		light_origin;
	vec3_t		model_center;
	bool		b_light_origin;
	bool		b_model_center;
	int		i, j, k;
	vec3_t		origin;
	entity_t		*ent;
	dmodel_t		*mod;
	dface_t		*f;
	winding_t		*w;
	char		*s;

	g_patches = (patch_t *)Mem_Alloc( sizeof( patch_t ) * MAX_PATCHES );
	g_numworldlights = 0;

	for( i = 0; i < g_nummodels; i++ )
	{
		mod = &g_dmodels[i];
		ent = EntityForModel( i );
		VectorClear( origin );
		b_light_origin = false;
		b_model_center = false;

		// bmodels with origin brushes need to be offset
		// into their in-use position
		if( *( s = ValueForKey( ent, "origin" )))
		{
			double	v1, v2, v3;

			if( sscanf( s, "%lf %lf %lf", &v1, &v2, &v3 ) == 3 )
				VectorSet( origin, v1, v2, v3 );
		}

		// allow models to be lit in an alternate location (pt1)
		if( *( s = ValueForKey( ent, "light_origin" )))
		{
			entity_t	*e = FindTargetEntity( s );

			if( e )
			{
				if( *( s = ValueForKey( e, "origin" )))
				{
					double	v1, v2, v3;

					if( sscanf( s, "%lf %lf %lf", &v1, &v2, &v3 ) == 3 )
					{
						VectorSet( light_origin, v1, v2, v3 );
						b_light_origin = true;
					}
				}
			}
		}

		// allow models to be lit in an alternate location (pt2)
		if( *( s = ValueForKey( ent, "model_center" )))
		{
			double	v1, v2, v3;

			if( sscanf( s, "%lf %lf %lf", &v1, &v2, &v3 ) == 3 )
			{
				VectorSet( model_center, v1, v2, v3 );
				b_model_center = true;
			}
		}
#ifndef HLRAD_PARANOIA_BUMP
		if( CheckKey( ent, "style" ))
			style = abs( IntForKey( ent, "style" ));
		else style = 0;
#endif
		// allow models to be lit in an alternate location (pt3)
		if( b_light_origin && b_model_center )
		{
			VectorSubtract( light_origin, model_center, origin );
		}

		for( j = 0; j < mod->numfaces; j++ )
		{
			fn = mod->firstface + j;
			g_face_entity[fn] = ent;
			VectorCopy( origin, g_face_offset[fn] );
			f = &g_dfaces[fn];
			w = WindingFromFace( f );

			// adjust model origins
			for( k = 0; k < w->numpoints; k++ )
				VectorAdd( w->p[k], origin, w->p[k] );

			// some faces won't create patch on them
			if( !MakePatchForFace( i, fn, w, style ))
				FreeWinding( w );
		}
	}

	MsgDev( D_REPORT, "%i square feet [%.2f square inches]\n", (int)(g_totalarea / 144), g_totalarea );
	MsgDev( D_INFO, "%i base patches, required %s\n", g_num_patches, Q_memprint( sizeof( patch_t ) * g_num_patches ));
	MsgDev( D_NOTE, "patch_t = %s\n", Q_memprint( sizeof( patch_t )));
	MsgDev( D_NOTE, "sample_t = %s\n", Q_memprint( sizeof( sample_t )));
}

// =====================================================================================
//  patch_sorter
// =====================================================================================
static int patch_sorter( const void *p1, const void *p2 )
{
	patch_t	*patch1 = (patch_t *)p1;
	patch_t	*patch2 = (patch_t *)p2;

	if( patch1->faceNumber < patch2->faceNumber )
		return -1;
	else if( patch1->faceNumber > patch2->faceNumber )
		return 1;

	return 0;
}

/*
=============
SortPatches
=============
*/
static void SortPatches( void )
{
	patch_t	*old_patches = g_patches;
	g_patches = (patch_t *)Mem_Alloc( sizeof( patch_t ) * ( g_num_patches + 1 ));
	memcpy( g_patches, old_patches, g_num_patches * sizeof( patch_t ));
	Mem_Free( old_patches );

	qsort((void *)g_patches, (size_t)g_num_patches, sizeof( patch_t ), patch_sorter );

	// fixup g_face_patches & fFixup patch->next
	memset( g_face_patches, 0, sizeof( g_face_patches ));

	patch_t	*patch = g_patches + 1;
	patch_t	*prev = g_patches;
	int	x;

	g_face_patches[prev->faceNumber] = prev;

	for( x = 1; x < g_num_patches; x++, patch++ )
	{
		if( patch->faceNumber != prev->faceNumber )
		{
			prev->next = NULL;
			g_face_patches[patch->faceNumber] = patch;
		}
		else
		{
			prev->next = patch;
		}
		prev = patch;
	}

	for( x = 0; x < g_num_patches; x++ )
	{
		patch_t *patch = &g_patches[x];
		patch->leafnum = PointInLeaf( patch->origin ) - g_dleafs;
	}

	// validate chains (debug thing)
	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		for( patch = g_face_patches[facenum]; patch != NULL; patch = patch->next )
		{
			ASSERT( patch->faceNumber == facenum );
		}
	}

	if( g_num_patches <= 0 ) g_usingpatches = false;
}

/*
=============
FreePatches
=============
*/
static void FreePatches( void )
{
	patch_t	*patch = g_patches;

	for( int i = 0; i < g_num_patches; i++, patch++ )
		FreeWinding( patch->winding );

	memset( g_patches, 0, sizeof( patch_t ) * g_num_patches );
	Mem_Free( g_patches );
	g_patches = NULL;
}

/*
=============
FreeTransfers
=============
*/
static void FreeTransfers( void )
{
	patch_t	*patch = g_patches;

	for( int i = 0; i < g_num_patches; i++, patch++ )
	{
		if( patch->tData )
		{
			Mem_Free( patch->tData );
			patch->tData = NULL;
		}

		if( patch->tIndex )
		{
			Mem_Free( patch->tIndex );
			patch->tIndex = NULL;
		}
	}
}

//=====================================================================
#ifdef HLRAD_COMPRESS_TRANSFERS
static uint GetLengthOfRun( const uint *raw, const uint *end )
{
	uint	run_size = 0;

	while( raw < end )
	{
		if((( *raw ) + 1 ) == (*( raw + 1 )))
		{
			run_size++;
			raw++;

			if( run_size >= MAX_COMPRESSED_TRANSFER_INDEX )
				return run_size;
		}
		else
		{
			return run_size;
		}
	}

	return run_size;
}

static transfer_index_t *CompressTransferIndicies( const uint *tRaw, const uint rawSize, uint *iSize, int threadnum )
{
	uint	*end = (uint *)tRaw + rawSize - 1;
	uint	compressed_count_1 = 0;
	uint	compressed_count = 0;
	uint	*raw = (uint *)tRaw;
	uint	x, size = rawSize;

	if( !size ) return NULL;

	for( x = 0; x < rawSize; x++ )
	{
		x += GetLengthOfRun( tRaw + x, end );
		compressed_count_1++;
	}

	if( !compressed_count_1 )
		return NULL;

	transfer_index_t	*CompressedArray = (transfer_index_t *)Mem_Alloc( sizeof( transfer_index_t ) * compressed_count_1 );
	transfer_index_t	*compressed = CompressedArray;

	for( x = 0; x < size; x++, raw++, compressed++ )
	{
		compressed->index = (*raw);
		// zero based (count 0 still implies 1 item in the list, so 256 max entries result)
		compressed->size = GetLengthOfRun( raw, end );
		raw += compressed->size;
		x += compressed->size;
		compressed_count++;	// number of entries in compressed table
	}

	*iSize = compressed_count;

	if( compressed_count != compressed_count_1 )
		COM_FatalError( "CompressTransferIndicies: internal error\n" );

	g_transfer_data_size[threadnum] += sizeof( transfer_index_t ) * compressed_count;

	return CompressedArray;
}
#else
static transfer_index_t *CompressTransferIndicies( const uint *tRaw, const uint rawSize, uint *iSize, int threadnum )
{
	uint	*end = (uint *)tRaw + rawSize;
	uint	compressed_count = 0;
	uint	*raw = (uint *)tRaw;
	uint	x, size = rawSize;

	if( !size ) return NULL;

	transfer_index_t	*CompressedArray = (transfer_index_t *)Mem_Alloc( sizeof( transfer_index_t ) * size );
	transfer_index_t	*compressed = CompressedArray;

	for( x = 0; x < size; x++, raw++, compressed++ )
	{
		compressed->index = (*raw);
		compressed->size = 0;
		compressed_count++; // number of entries in compressed table
	}

	*iSize = compressed_count;

	g_transfer_data_size[threadnum] += sizeof( transfer_index_t ) * compressed_count;

	return CompressedArray;
}
#endif

/*
===========
MakeTransfers

This is run by multiple threads
===========
*/
static void MakeTransfers( int threadnum )
{
	transfer_data_t	*tData_All = (transfer_data_t *)Mem_Alloc( sizeof( transfer_data_t ) * ( g_num_patches + 1 ));
	uint		*tIndex_All = (uint *)Mem_Alloc( sizeof( transfer_index_t ) * ( g_num_patches + 1 ));
	patch_t		**vispatches = (patch_t **)Mem_Alloc(( g_num_patches + 1 ) * sizeof( patch_t* ));
	byte		pvs[(MAX_MAP_LEAFS+7)/8];
	const vec_t	*normal1, *normal2;
	float		trans, total, dist;
	bool		check_vis = true;
	patch_t		*patch1, *patch2;
	int			i, j, lastoffset;
	int			count = 0;
	uint		*tIndex;
	transfer_data_t	*tData;
	vec3_t		delta;
	vec_t		trans_sum;
	byte		*patch_shadowing = (byte *)Mem_Alloc( g_num_patches + 1 );

	while( 1 )
	{
		if(( i = GetThreadWork( )) == -1 )
			break;

		patch1 = g_patches + i;

		// calculate visibility for the patch
		if( !g_visdatasize )
		{
			if( check_vis )
			{
				memset( pvs, 255, (g_numvisleafs + 7) / 8 );
				check_vis = false;
			}
		}
		else
		{
			dleaf_t	*leaf = &g_dleafs[patch1->leafnum];
			int	thisoffset = leaf->visofs;

			if( check_vis || thisoffset != lastoffset )
			{
				if( thisoffset == -1 )
				{
					memset( pvs, 0, (g_numvisleafs + 7) / 8 );
				}
				else
				{
					DecompressVis( &g_dvisdata[leaf->visofs], pvs );
				}
				check_vis = false;
			}
			lastoffset = thisoffset;
		}

		const dplane_t *plane1 = GetPlaneFromFace( patch1->faceNumber );
		count = 0;

		// find out which patch2's will collect light from patch
		for( j = 0, patch2 = g_patches; j < g_num_patches; j++, patch2++ )
		{
			if( i == j ) continue;

			if( !patch2->leafnum || !CHECKVISBIT( pvs, patch2->leafnum - 1 ))
				continue;

			const dplane_t *plane2 = GetPlaneFromFace( patch2->faceNumber );

			if( DotProduct( patch1->origin, plane2->normal ) <= PatchPlaneDist( patch2 ) + ON_EPSILON - patch1->emitter_range )
				continue;

			// we need to do a real test
			vec3_t	origin1, origin2, delta;
			vec_t	dist;

			VectorSubtract( patch1->origin, patch2->origin, delta );
			dist = VectorLength( delta );

			if( dist < patch2->emitter_range - ON_EPSILON )
				GetAlternateOrigin( patch1->origin, plane1->normal, patch2, origin2 );
			else VectorCopy( patch2->origin, origin2 );

			if( DotProduct( origin2, plane1->normal ) <= PatchPlaneDist( patch1 ) + MINIMUM_PATCH_DISTANCE )
				continue;

			if( dist < patch1->emitter_range - ON_EPSILON )
				GetAlternateOrigin( patch2->origin, plane2->normal, patch1, origin1 );
			else VectorCopy( patch1->origin, origin1 );

			if( DotProduct( origin1, plane2->normal ) <= PatchPlaneDist( patch2 ) + MINIMUM_PATCH_DISTANCE )
				continue;

			if( g_patchaa )
			{
				byte shadowing = 0;
				for( int k = 0; k < PATCH_MAX_TRACE_ORIGINS; k++ )
					if( TestLine( threadnum, patch1->trace_origins[k], origin2, false ) == CONTENTS_EMPTY )
						shadowing++;
				if( shadowing == 0 )
					continue;			
				patch_shadowing[count] = shadowing;
			}
			else
			{
				VectorCopy( patch1->trace_origins[j % PATCH_MAX_TRACE_ORIGINS], origin1 );

				if( TestLine( threadnum, origin1, origin2, false ) != CONTENTS_EMPTY )
					continue;
			}

			vispatches[count] = patch2;
			count++;
		}

		// compute transfers for this patch
		patch1->iIndex = patch1->iData = 0;
		normal1 = plane1->normal;
		tIndex = tIndex_All;
		tData = tData_All;

		trans_sum = 0.0f;
		vec3_t tangent, binormal;
		if( g_accurate_trans )
		{
			binormal[0] = normal1[1];
			binormal[1] = normal1[2];
			binormal[2] = normal1[0];
			CrossProduct( normal1, binormal, tangent );
			VectorNormalize( tangent );
			CrossProduct( normal1, tangent, binormal );
		}

		for( j = 0; j < count; j++ )
		{
			bool	light_behind_surface = false;
			vec_t	dot1, dot2;

			patch2 = vispatches[j];

			if( g_accurate_trans )
			{
				trans = CalcPatchLambert( patch1->origin, tangent, binormal, normal1, patch2->winding );
			}
			else
			{
				normal2 = GetPlaneFromFace( patch2->faceNumber )->normal;

				// calculate transference
				VectorSubtract( patch2->origin, patch1->origin, delta );

				// move emitter back to its plane
				VectorMA( delta, -DEFAULT_HUNT_OFFSET, normal2, delta );

				dist = VectorNormalize( delta );
				dot1 = DotProduct( delta, normal1 );
				dot2 = -DotProduct( delta, normal2 );

				if( dot1 <= NORMAL_EPSILON )
					light_behind_surface = true;

				if( dot2 * dist <= MINIMUM_PATCH_DISTANCE )
					continue;

				// inverse square falloff factoring angle between patch normals
				trans = (dot1 * dot2) / (dist * dist);

				// HLRAD_TRANSWEIRDFIX:
				// we should limit "trans( patch2receive ) * patch1area" 
				// instead of "trans( patch2receive ) * patch2area".
				// also raise "0.4f" to "0.8f" ( 0.8 / M_PI = 1 / 4).
				if( trans * patch2->area > 0.8f )
					trans = 0.8f / patch2->area;

				if( dist < patch2->emitter_range - ON_EPSILON )
				{
					if( light_behind_surface )
						trans = 0.0f;

					vec_t	sightarea = CalcSightArea( patch1->origin, normal1, patch2->winding, patch2->emitter_skylevel );
					vec_t	frac = dist / patch2->emitter_range;

					frac = (frac - 0.5f) * 2.0f; // make a smooth transition between the two methods
					frac = bound( 0.0f, frac, 1.0f );
					trans = frac * trans + (1.0f - frac) * (sightarea / patch2->area); // because later we will multiply this back
				}
				else if( light_behind_surface )
				{
					continue;
				}

				trans *= patch2->exposure;
				trans *= patch2->area;
			}

			if( g_patchaa )
				trans *= (float)patch_shadowing[j] / (float)PATCH_MAX_TRACE_ORIGINS;
			trans_sum += trans;

			// scale to 16 bit (black magic)
			trans = trans * INVERSE_TRANSFER_SCALE;
			if( trans > TRANSFER_SCALE_MAX )
				trans = TRANSFER_SCALE_MAX;
			if( trans <= 0.0f ) continue;


			*tData = trans;
			patch1->iData++;
			*tIndex = patch2 - g_patches;
			tIndex++;
			tData++;
		}

		if( trans_sum > M_PI )
		{
			trans_sum = M_PI;
		}
		patch1->trans_sum = trans_sum;

		// copy the transfers out
		if( patch1->iData )
		{
			uint		data_size = patch1->iData * sizeof( transfer_data_t );
			transfer_data_t	*t1, *t2;

			patch1->tData = (transfer_data_t *)Mem_Alloc( data_size );
			patch1->tIndex = CompressTransferIndicies( tIndex_All, patch1->iData, &patch1->iIndex, threadnum );
			g_transfer_data_size[threadnum] += data_size;

			total = 0.25f;

			t1 = patch1->tData;
			t2 = tData_All;

			for( uint x = 0; x < patch1->iData; x++, t1++, t2++ )
				(*t1) = (*t2) * total;
		}
	}

	Mem_Free( vispatches );
	Mem_Free( tIndex_All );
	Mem_Free( tData_All );
	Mem_Free( patch_shadowing );	//nc add
}

/*
============
CalcTransferSize
============
*/
void CalcTransferSize( void )
{
	g_transfer_data_bytes = 0;

	for( int i = 0; i < MAX_THREADS; i++ )
		g_transfer_data_bytes += g_transfer_data_size[i];

	MsgDev( D_INFO, "transfer lists: %s\n", Q_memprint( g_transfer_data_bytes ));
}

/*
==============
MakeTransfers
==============
*/
void MakeTransfers( void )
{
	RunThreadsOn( g_num_patches, true, MakeTransfers );

	// display transfer size
	CalcTransferSize();
}

/*
=============
CollectLight
=============
*/
void CollectLight( void )
{
	patch_t	*patch;
	int	i, j;

	for( i = 0, patch = g_patches; i < g_num_patches; i++, patch++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && newstyles[i][j] != 255; j++ )
		{
			VectorAdd( patch->totallight[j], addlight[i][j], patch->totallight[j] );
			VectorScale( addlight[i][j], TRANSFER_SCALE, emitlight[i][j] );
			VectorClear( addlight[i][j] );
#ifdef HLRAD_DELUXEMAPPING
			VectorAdd( patch->totallight_dir[j], addlight_dir[i][j], patch->totallight_dir[j] );
			VectorCopy( addlight_dir[i][j], emitlight_dir[i][j] );
			VectorClear( addlight_dir[i][j] );
#endif
		}

		// store new styles back into patch
		memcpy( g_patches[i].totalstyle, newstyles[i], sizeof( byte[MAXLIGHTMAPS] ));
	}
}

/*
=============
BounceLight

Get light from other patches
  Run multi-threaded
=============
*/
void BounceLight( int threadnum )
{
	int	j, k, m;
	patch_t	*patch;
	vec3_t	v;

	while( 1 )
	{
		if(( j = GetThreadWork()) == -1 )
			break;

		patch = &g_patches[j];

		transfer_data_t	*tData = patch->tData;
		transfer_index_t	*tIndex = patch->tIndex;
		uint		iIndex = patch->iIndex;
		int		overflowed_styles = 0;

		const dplane_t *plane1 = GetPlaneFromFace( patch->faceNumber );

		for( m = 0; m < MAXLIGHTMAPS && newstyles[j][m] != 255; m++ )
			VectorClear( addlight[j][m] );

		for( k = 0; k < iIndex; k++, tIndex++ )
		{
			uint	size = (tIndex->size + 1);
			uint	patchnum = tIndex->index;
			uint	l;

			for( l = 0; l < size; l++, tData++, patchnum++ )
			{
				if( patchnum < 0 || patchnum >= g_num_patches )
				{
					MsgDev( D_ERROR, "bad patchnum %i, max %i\n", patchnum, g_num_patches );
					continue;
				}

				patch_t	*emitpatch = &g_patches[patchnum];
#ifdef HLRAD_DELUXEMAPPING
				vec3_t	direction;

				VectorSubtract( emitpatch->origin, patch->origin, direction );

				if( DotProduct( direction, plane1->normal ) <= 0.0f )	//vector between origins can be negative, have to fix it
				{
					vec3_t	origin2;
					GetAlternateOrigin( patch->origin, plane1->normal, emitpatch, origin2 );
					VectorSubtract( origin2, patch->origin, direction );
				}

				VectorNormalize( direction );
#endif
				// for each style on the emitting patch
				for( int emitstyle = 0; emitstyle < MAXLIGHTMAPS && emitpatch->totalstyle[emitstyle] != 255; emitstyle++ )
				{
					// find the matching style on this (destination) patch
					for( m = 0; m < MAXLIGHTMAPS && newstyles[j][m] != 255; m++ )
					{
						if( newstyles[j][m] == emitpatch->totalstyle[emitstyle] )
							break;
					}

					if( m < MAXLIGHTMAPS )
					{
						VectorScale( emitlight[patchnum][emitstyle], (float)(*tData), v );
						VectorMultiply( v, emitpatch->reflectivity, v );

						if( !VectorIsFinite( v )) continue;

						if( newstyles[j][m] == 255 )
							newstyles[j][m] = emitpatch->totalstyle[emitstyle];

						VectorAdd( addlight[j][m], v, addlight[j][m] );
#ifdef HLRAD_DELUXEMAPPING
						vec_t	brightness = VectorAvg( v );
						VectorMA( addlight_dir[j][m], brightness, direction, addlight_dir[j][m] );
#endif
					}
					else
					{
						overflowed_styles++;
					}
				}
			}
		}

		g_overflowed_styles_onpatch[threadnum] += overflowed_styles;
	}
}

/*
=============
BounceLight
=============
*/
void BounceLight( void )
{
	int	i, j;

	for( i = 0; i < g_num_patches; i++ )
	{
		patch_t	*patch = &g_patches[i];

		for( j = 0; j < MAXLIGHTMAPS && g_patches[i].totalstyle[j] != 255; j++ )
		{
			VectorScale( patch->totallight[j], TRANSFER_SCALE, emitlight[i][j] );
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( patch->totallight_dir[j], emitlight_dir[i][j] );
#endif
		}
		memcpy( newstyles[i], g_patches[i].totalstyle, sizeof( byte[MAXLIGHTMAPS] ));
	}

	for( i = 0; i < g_numbounce; i++ )
	{
		RunThreadsOnIncremental( g_num_patches, true, BounceLight, i + 1 );
		CollectLight();
	}
}

//==============================================================
/*
=============
RadWorld
=============
*/
void RadWorld( void )
{
	MakeBackplanes();

	// turn each face into a single patch
	MakePatches ();

	SortPatches ();

	PairEdges ();

	BuildFaceNeighbors();

	BuildDiffuseNormals();

	// create directlights out of patches and lights
	CreateDirectLights ();

	if( g_onlylights )
	{
		DeleteDirectLights ();
		FreeDiffuseNormals ();
		FreeFaceNeighbors();
		FreeSharedEdges();
		FreeFaceLights();
		FreePatches();

		return;
	}

	InitWorldTrace();

	// generate a position map for each face
	RunThreadsOnIndividual( g_numfaces, true, FindFacePositions );
	CalcPositionsSize();

	if( g_envsky )
		LoadEnvSkyTextures();

#ifdef HLRAD_LIGHTMAPMODELS
	BuildModelLightmaps();
	CalcModelSampleSize();
#endif

#ifdef HLRAD_VERTEXLIGHTING
	BuildVertexLights();	//BuildFaceLights will get single gi bounce from studiomodels

	if( g_numstudiobounce > 0 )
	{
		g_studiogipasscounter = 1;		
		VertexPatchLights();
		VertexBlendGI();
	}
#endif


	// build initial facelights
	RunThreadsOnIndividual( g_numfaces, true, BuildFaceLights );
	CalcSampleSize ();

	FreeFacePositions ();

	CalcLuxelsCount();

	if( g_numbounce > 0 && g_usingpatches )
	{
		// build transfer lists	
		MakeTransfers();

		emitlight = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc(( g_num_patches + 1 ) * sizeof( vec3_t[MAXLIGHTMAPS] ));
		addlight = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc(( g_num_patches + 1 ) * sizeof( vec3_t[MAXLIGHTMAPS] ));
		newstyles = (byte (*)[MAXLIGHTMAPS])Mem_Alloc(( g_num_patches + 1 ) * sizeof( byte[MAXLIGHTMAPS] ));
#ifdef HLRAD_DELUXEMAPPING
		emitlight_dir = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc(( g_num_patches + 1 ) * sizeof( vec3_t[MAXLIGHTMAPS] ));
		addlight_dir = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc(( g_num_patches + 1 ) * sizeof( vec3_t[MAXLIGHTMAPS] ));
#endif
		// spread light around
		BounceLight ();

		Mem_Free( emitlight );
		Mem_Free( addlight );
		Mem_Free( newstyles );
		emitlight = NULL;
		addlight = NULL;
		newstyles = NULL;
#ifdef HLRAD_DELUXEMAPPING
		Mem_Free( emitlight_dir );
		Mem_Free( addlight_dir );
		emitlight_dir = NULL;
		addlight_dir = NULL;
#endif
		// transfers don't need anymore
		FreeTransfers();
	}

	// remove direct light from patches
	for( int i = 0; i < g_num_patches; i++ )
	{
		patch_t	*p = &g_patches[i];

		for( int j = 0; j < MAXLIGHTMAPS && p->totalstyle[j] != 255; j++ )
		{
			VectorSubtract( p->totallight[j], p->directlight[j], p->totallight[j] );
#ifdef HLRAD_DELUXEMAPPING
			VectorSubtract( p->totallight_dir[j], p->directlight_dir[j], p->totallight_dir[j] );
#endif
		}
	}

	if( !g_lightbalance )
		ScaleDirectLights();

	// because fastmode uses patches instead of samples
	if( g_usingpatches )
		RunThreadsOnIndividual( g_numfaces, false, CreateTriangulations );

	// blend bounced light into direct light and save
	PrecompLightmapOffsets();

	if( g_usingpatches )
	{
		CreateFacelightDependencyList();

		RunThreadsOnIndividual( g_numfaces, true, FacePatchLights );

		FreeFacelightDependencyList();
	}

	if( g_usingpatches )
		FreeTriangulations();

	if( g_lightbalance )
		ScaleDirectLights();

	RunThreadsOnIndividual( g_numfaces, true, FinalLightFace );
#ifdef HLRAD_LIGHTMAPMODELS
	FinalModelLightFace();
#endif


#ifdef HLRAD_VERTEXLIGHTING
	g_studiogipasscounter = 0;
	for( int i = 0; i < Q_max( 1, g_numstudiobounce ); i++ )
	{
		g_studiogipasscounter++;		
		VertexPatchLights();
		VertexBlendGI();
	}
	
	FinalLightVertex();
#endif

#ifdef HLRAD_AMBIENTCUBES
	if (g_compatibility_mode != CompatibilityMode::GoldSrc) {
	ComputeLeafAmbientLighting();
	}
#endif

	DeleteDirectLights ();

#ifndef HLRAD_PARANOIA_BUMP
	ReduceLightmap();
#endif

	FreeFaceLights();

#ifdef HLRAD_LIGHTMAPMODELS
	WriteModelLighting();
#endif
#ifdef HLRAD_VERTEXLIGHTING
	UnparseEntities();
#endif

	FreeDiffuseNormals ();

	if( g_envsky )
		FreeEnvSkyTextures();

	FreeFaceNeighbors();

	FreeSharedEdges();

	FreePatches();
}

/*
============
PrintRadSettings

show compiler settings like ZHLT
============
*/
static void PrintRadSettings( void )
{
	char	buf1[1024];
	char	buf2[1024];

	Msg( "Current %s settings\n", APP_ABBREVIATION );
	Msg( "Name                 |  Setting  |  Default\n" );
	Msg( "---------------------|-----------|-------------------------\n" );
	Msg( "developer             [ %7d ] [ %7d ]\n", GetDeveloperLevel(), DEFAULT_DEVELOPER );
	Msg( "fast rad              [ %7s ] [ %7s ]\n", g_fastmode ? "on" : "off", DEFAULT_FASTMODE ? "on" : "off" );
	Msg( "extra rad             [ %7s ] [ %7s ]\n", g_extra ? "on" : "off", DEFAULT_EXTRAMODE ? "on" : "off" );
	Msg( "bounces               [ %7d ] [ %7d ]\n", g_numbounce, DEFAULT_BOUNCE );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_smoothvalue );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_SMOOTHVALUE );
	Msg( "smoothing threshold   [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_blur );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_BLUR );
	Msg( "blur size             [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_lightbalance ? 1.0f : g_direct_scale );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_DLIGHT_SCALE );
	Msg( "direct light scale    [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_lightbalance ? g_direct_scale : 1.0f );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_LIGHT_SCALE );
	Msg( "global light scale    [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_gamma );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_GAMMA );
	Msg( "gamma factor          [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_chop );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_CHOP );
	Msg( "chop value            [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_texchop );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_TEXCHOP );
	Msg( "texchop value         [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_indirect_sun );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_INDIRECT_SUN );
	Msg( "global sky diffusion  [ %7s ] [ %7s ]\n", buf1, buf2 );
	Msg( "dirtmapping           [ %7s ] [ %7s ]\n", g_dirtmapping ? "on" : "off", DEFAULT_DIRTMAPPING ? "on" : "off" );
	Msg( "compatibility mode    [ %7s ] [ %7s ]\n", 
		CompatibilityMode::GetString(g_compatibility_mode), CompatibilityMode::GetString(DEFAULT_COMPAT_MODE));
#ifdef HLRAD_PARANOIA_BUMP
	Msg( "gamma mode            [ %7d ] [ %7d ]\n", g_gammamode, DEFAULT_GAMMAMODE );
#endif
	Msg( "\n" );
}

/*
============
PrintRadUsage

show compiler usage like ZHLT
============
*/
static void PrintRadUsage( void )
{
	Msg( "\n-= %s Options =-\n\n", APP_ABBREVIATION );
	Msg( "    -dev #         : compile with developer message (1 - 4). default is %d\n", DEFAULT_DEVELOPER );
	Msg( "    -threads #     : manually specify the number of threads to run\n" );
 	Msg( "    -extra         : improve lighting quality with lightmap filtering\n" );
	Msg( "    -bounce #      : set number of radiosity bounces\n" );
	Msg( "    -ambient r g b : set ambient world light (0.0 to 1.0, r g b)\n" );
	Msg( "    -smooth #      : set smoothing threshold for blending (in degrees)\n" );
	Msg( "    -blur          : filtering the lightmap by post-processing\n" );
	Msg( "    -dscale        : direct light scaling factor\n" );
	Msg( "    -gamma         : set global gamma value\n" );
	Msg( "    -chop #        : set radiosity patch size for normal textures\n" );
	Msg( "    -texchop #     : set radiosity patch size for texture light faces\n" );
	Msg( "    -sky #         : set ambient sunlight contribution in the shade outside\n" );
	Msg( "    -nomodelshadow : ignore shadows from alias and studiomodels\n" );
	Msg( "    -balance       : -dscale will be interpret as global scaling factor\n" );
	Msg( "    -dirty         : enable dirtmapping (baked AO)\n" );
	Msg( "    -onlylights    : update only worldlights lump\n" );
	Msg( "    -compat <type> : enable compatibility mode (goldsrc/xashxt)\n" );

	Msg( "    -scale #.#     : global light scaling factor. default is %f\n", DEFAULT_GLOBAL_SCALE );
	Msg( "    -texgamma #.#  : texture gamma. default is %f\n", DEFAULT_TEXREFLECTGAMMA );
	Msg( "    -hdrcompresslog: compress hdr lightmap to ldr using logarithmic profile\n" );
	Msg( "    -tonemap       : apply tonemapping\n" );
	Msg( "    -accurate_trans: accurate radiosity patch transfers calculation\n" );
	Msg( "    -fastsky       : less sky samples for outdoors\n" );
	Msg( "    -nolerp        : disable interpolation of radiosity patches\n" );
	Msg( "    -envsky        : get sky color from gfx\\env textures\n" );
	Msg( "    -solidsky      : use solid sky color from the light_environment, do not mix it with the sun color\n" );
	Msg( "    -aa            : 3x3 antialiaing for direct lighting\n" );
	Msg( "    -delambert     : removes lambert component from the final lightmap\n" );	
	Msg( "    -worldspace    : deluxe map in world space, not tangent space\n" );
	Msg( "    -studiolegacy  : use legacy tree for studio models tracing instead of BVH\n" );
	Msg( "    -lightprobeepsilon #.#: set light probe importance threshold value. default is %f\n", DEFAULT_LIGHTPROBE_EPSILON );
	Msg( "    -lightprobesamples #: set number of rays for light probes baking. default is 256\n" );
	Msg( "    -studiobounce #: set number of studio model radiosity bounces. default is %d\n", DEFAULT_STUDIO_BOUNCE );
	Msg( "    -vertexblur    : blur per-vertex lighting\n" );	
	Msg( "    -noemissive    : do not add emissive textures to the lightmap\n" );
	Msg( "    -skystyle #    : set lightstyle for sky lighting. default is %d\n", LS_NORMAL );
	Msg( "    -perpixelsky   : per pixel calculation of sky lighting\n" );
	Msg( "    -patchaa       : use multiple samples for patch visibility\n" );

#ifdef HLRAD_PARANOIA_BUMP
	Msg( "    -gammamode #   : gamma correction mode (0, 1, 2)\n" );
#endif
	Msg( "    bspfile        : The bspfile to compile\n\n" );
}

/*
========
main

light modelfile
========
*/
int main( int argc, char **argv )
{
	double	start, end;
	char	str[64];
	int		i;

	atexit( Sys_CloseLog );
	source[0] = '\0';

	CrashHandler::Setup();
	g_smoothing_threshold = cos( DEG2RAD( g_smoothvalue )); // Originally zero.

	for( i = 1; i < argc; i++ )
	{
		if( !Q_strcmp( argv[i], "-dev" ))
		{
			SetDeveloperLevel( atoi( argv[i+1] ));
			i++;
		}
		else if( !Q_strcmp( argv[i], "-threads" ))
		{
			g_numthreads = atoi( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-fast" ))
		{
			g_nomodelshadow = true;
			g_lerp_enabled = false;
			g_dirtmapping = false;
			g_fastmode = true;
		}
		else if( !Q_strcmp( argv[i], "-extra" ))
		{
			g_lerp_enabled = true;
			g_extra = true;
		}
		else if( !Q_strcmp( argv[i], "-bounce" ))
		{
			g_numbounce = atoi( argv[i+1] );
			g_numbounce = bound( 0, g_numbounce, 128 );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-ambient" ))
		{
			if( argc > ( i + 3 ))
			{
 				g_ambient[0] = (float)atof( argv[i+1] ) * 0.5f;
 				g_ambient[1] = (float)atof( argv[i+2] ) * 0.5f;
 				g_ambient[2] = (float)atof( argv[i+3] ) * 0.5f;
				i += 3;
			}
			else
			{
				break;
			}
		}
		else if( !Q_strcmp( argv[i], "-smooth" ))
		{
			g_smoothvalue = atof( argv[i+1] );
			g_smoothing_threshold = (float)cos( DEG2RAD( g_smoothvalue ));
			i++;
		}
		else if( !Q_strcmp( argv[i], "-blur" ))
		{
			g_blur = atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-dscale" ))
		{
			g_direct_scale = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-gamma" ))
		{
			g_gamma = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-texgamma" ))
		{
			g_texgamma = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-hdrcompresslog" ))
		{
			g_hdrcompresslog = true;
		}
		else if( !Q_strcmp( argv[i], "-tonemap" ))
		{
			g_tonemap = true;
		}	
		else if( !Q_strcmp( argv[i], "-accurate_trans" ))
		{
			g_accurate_trans = true;
		}	
		else if( !Q_strcmp( argv[i], "-fastsky" ))
		{
			g_accurate_trans = true;	//very leaky without it
			g_fastsky = true;
		}
		else if( !Q_strcmp( argv[i], "-nolerp" ))
		{
			g_lerp_enabled = false;
			g_nolerp = true;
		}	
		else if( !Q_strcmp( argv[i], "-envsky" ))
		{
			g_envsky = true;
		}
		else if( !Q_strcmp( argv[i], "-solidsky" ))
		{
			g_solidsky = true;
		}	
		else if( !Q_strcmp( argv[i], "-aa" ))
		{
			g_aa = true;
		}
		else if( !Q_strcmp( argv[i], "-delambert" ))
		{
			g_delambert = true;
		}	
		else if( !Q_strcmp( argv[i], "-worldspace" ))
		{
			g_worldspace = true;
		}	
		else if( !Q_strcmp( argv[i], "-studiolegacy" ))
		{
			g_studiolegacy = true;
		}
		else if( !Q_strcmp( argv[i], "-scale" ))
		{
			g_scale = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-lightprobeepsilon" ))
		{
			g_lightprobeepsilon = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-lightprobesamples" ))
		{
			g_lightprobesamples = atoi( argv[i+1] );
			i++;
		}			
		else if( !Q_strcmp( argv[i], "-studiobounce" ))
		{
			g_numstudiobounce = atoi( argv[i+1] );
			i++;
		}	
		else if( !Q_strcmp( argv[i], "-vertexblur" ))
		{
			g_vertexblur = true;
		}	
		else if( !Q_strcmp( argv[i], "-noemissive" ))
		{
			g_noemissive = true;
		}
		else if( !Q_strcmp( argv[i], "-skystyle" ))
		{
			g_skystyle = atoi( argv[i+1] );
			i++;
		}		
		else if( !Q_strcmp( argv[i], "-perpixelsky" ))
		{
			g_perpixelsky = true;
		}
		else if( !Q_strcmp( argv[i], "-patchaa" ))
		{
			g_patchaa = true;
		}
		else if( !Q_strcmp( argv[i], "-chop" ))
		{
			g_chop = (float)atof( argv[i+1] );
			g_chop = bound( 16, g_chop, 128 );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-texchop" ))
		{
			g_texchop = (float)atof( argv[i+1] );
			g_texchop = bound( 32, g_texchop, 128 );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-sky" ))
		{
			g_indirect_sun = (float)atof( argv[i+1] );
			g_indirect_sun = bound( 0.0f, g_indirect_sun, 2.0f );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-nomodelshadow" ))
		{
			g_nomodelshadow = true;
		}
		else if( !Q_strcmp( argv[i], "-balance" ))
		{
			g_lightbalance = true;
		}
		else if( !Q_strcmp( argv[i], "-onlylights" ))
		{
			g_onlylights = true;
		}
		else if( !Q_strcmp( argv[i], "-quake" ))
		{
			// special preset for quake
			g_lightbalance = true;
			g_indirect_scale = 1.0f;
			g_direct_scale = 0.5f;
			g_indirect_sun = 0.0f;
			g_gamma = 1.0f;
		}
		else if( !Q_strcmp( argv[i], "-dirty" ))
		{
			g_dirtmapping = !g_fastmode;
		}
		else if (!Q_strcmp(argv[i], "-compat"))
		{
			i++;
			if (!Q_strcmp(argv[i], "goldsrc")) {
				g_compatibility_mode = CompatibilityMode::GoldSrc;
			}
			else 
			{
				MsgDev(D_ERROR, "\nUnknown compatibility mode parameter \"%s\"\n", argv[i]);
				break;
			}
		}
#ifdef HLRAD_PARANOIA_BUMP
		else if( !Q_strcmp( argv[i], "-gammamode" ))
		{
			g_gammamode = (float)atoi( argv[i+1] );
			i++;
		}
#endif
		else if( argv[i][0] == '-' )
		{
			MsgDev( D_ERROR, "\nUnknown option \"%s\"\n", argv[i] );
			break;
		}
		else if( !source[0] )
		{
			Q_strncpy( source, COM_ExpandArg( argv[i] ), sizeof( source ));
			COM_StripExtension( source );
		}
		else
		{
			MsgDev( D_ERROR, "\nUnknown option \"%s\"\n", argv[i] );
			break;
		}
	}

	if (i != argc || !source[0])
	{
		if (!source[0]) {
			Msg("no mapfile specified\n");
		}

		PrintRadUsage();
		exit(1);
	}

	start = I_FloatTime ();

	Sys_InitLogAppend( va( "%s.log", source ));

	Msg( "\n%s %s (%s, commit %s, arch %s, platform %s)\n\n", TOOLNAME, VERSIONSTRING, 
		BuildInfo::GetDate(), 
		BuildInfo::GetCommitHash(), 
		BuildInfo::GetArchitecture(), 
		BuildInfo::GetPlatform()
	);

	PrintRadSettings();

	ThreadSetDefault ();

	// starting base filesystem
	FS_Init( source );

	BuildGammaTable();	//for gamma-corrected filtering of sky textures and others

	// Set the required global lights filename
	// try looking in the directory we were run from
	Q_getwd( global_lights, sizeof( global_lights ) );
	Q_strncat( global_lights, "lights.rad", sizeof( global_lights ));

	// Set the optional level specific lights filename
	COM_FileBase( source, str );
	Q_snprintf( level_lights, sizeof( level_lights ), "maps/%s.rad", str );
	if( !FS_FileExists( level_lights, false )) level_lights[0] = '\0';	

	// parse .rad files
	ReadLightFile(global_lights, true);	// Required
	if (*level_lights) {
		ReadLightFile(level_lights, false);	// Optional & implied
	}

	COM_DefaultExtension( source, ".bsp" );

	LoadBSPFile( source );

	if( g_nummodels <= 0 )
		COM_FatalError( "map %s without any models\n", source );

	ParseEntities();
	TEX_LoadTextures();
	ReadInfoTexlights();
	SetupDirt();

	if( !g_visdatasize && g_numbounce > 0 )
		MsgDev( D_ERROR, "no vis information, compile time may be adversely affected.\n" );

	if( g_skystyle < 0 )
		g_skystyle = IntForKey( &g_entities[0], "zhlt_skystyle" );
	g_usingpatches = g_numbounce > 0 || g_fastmode || g_indirect_sun > 0.0f;

	// keep it in acceptable range
	g_blur = bound( 1.0, g_blur, 8.0 );
	g_gamma = bound( 0.3, g_gamma, 1.0 );
	g_skystyle = bound( 0, g_skystyle, 254 );
	g_lightprobesamples = bound( 1, g_lightprobesamples, SKYNORMALS_RANDOM );

	RadWorld ();

	PrintBSPFileSizes(g_compatibility_mode == CompatibilityMode::GoldSrc);
	WriteBSPFile( source );
	TEX_FreeTextures ();
	FreeWorldTrace ();
	FreeEntities ();
	FS_Shutdown();
	CrashHandler::Restore();

	SetDeveloperLevel( D_REPORT );
	Mem_Check();

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	Msg( "%s elapsed\n", str );
	
	return 0;
}
