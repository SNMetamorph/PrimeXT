/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "csg.h"
#include "compatibility_mode.h"
#include "imagelib.h"
#include <string>
#include <cstring>
#include <unordered_map>

#define FWAD_USED		BIT( 0 )
#define FWAD_INCLUDE	BIT( 1 )

typedef struct
{
	char		name[64];
	int		flags;
} wadentry_t;

static const vec3_t	baseaxis[18] =
{
{ 0, 0, 1}, { 1, 0, 0}, { 0,-1, 0},	// floor
{ 0, 0,-1}, { 1, 0, 0}, { 0,-1, 0},	// ceiling
{ 1, 0, 0}, { 0, 1, 0}, { 0, 0,-1},	// west wall
{-1, 0, 0}, { 0, 1, 0}, { 0, 0,-1},	// east wall
{ 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1},	// south wall
{ 0,-1, 0}, { 1, 0, 0}, { 0, 0,-1}, 	// north wall
};

static mipentry_t	g_miptex[MAX_MAP_TEXTURES];
static int	g_nummiptex;
static wadentry_t	g_wadlist[MAX_TEXFILES];
static int	g_wadcount;
static std::unordered_map<int32_t, std::string> g_hashToMiptexName;
char	g_pszWadInclude[MAX_TEXFILES][64];
int		g_nWadInclude;

void TEX_LoadTextures( CUtlArray<mapent_t> *entities, bool merge )
{
	char	wadstring[MAX_VALUE];
	char	tmpWadName[64];
	char	*pszWadString;
	char	*pszWadFile;
	int		i, startcount;

	if( !merge )
	{
		memset( g_wadlist, 0 , sizeof( g_wadlist ));
		memset( g_miptex, 0 , sizeof( g_miptex ));
		g_wadcount = g_nummiptex = 0;
	}

	if( entities->Count() <= 0 )
		return;

	pszWadString = ValueForKey( (entity_t *)&entities[0], "wad" );
	if( !*pszWadString ) pszWadString = ValueForKey( (entity_t *)&entities[0], "_wad" );
	if( !*pszWadString ) return; // may be -nowadtextures was used?
	startcount = g_wadcount;

	Q_strncpy( wadstring, pszWadString, MAX_VALUE - 2 );
	wadstring[MAX_VALUE-1] = 0;

	if( !Q_strchr( wadstring, ';' ))
		Q_strcat( wadstring, ";" );

	// parse wad pathes
	for( pszWadFile = strtok( wadstring, ";" ); pszWadFile != NULL; pszWadFile = strtok( NULL, ";" ))
	{
		COM_FixSlashes( pszWadFile );
		COM_FileBase( pszWadFile, tmpWadName );

		// check for duplicate
		for( i = 0; i < g_wadcount; i++ )
		{
			if( !Q_stricmp( g_wadlist[i].name, tmpWadName ))
				break;
		}

		if( i != g_wadcount )
			continue;	// already included

		Q_strncpy( g_wadlist[g_wadcount].name, tmpWadName, sizeof( g_wadlist[0].name ));

		if( ++g_wadcount >= MAX_TEXFILES )
			break; // too many wads...
	}

	// set wads that should be included...
	for( i = startcount; i < g_wadcount; i++ )
	{
		if( g_wadtextures )
		{
			for( int j = 0; j < g_nWadInclude; j++ )
			{
				if( !Q_stricmp( g_wadlist[i].name, g_pszWadInclude[j] ))
				{
					MsgDev( D_INFO, "Including WAD File: %s\n", g_wadlist[i].name );
					SetBits( g_wadlist[i].flags, FWAD_INCLUDE );
					break;
				}
			}
		}
		else
		{
			// force to include all the wads
			SetBits( g_wadlist[i].flags, FWAD_INCLUDE );
		}
	}
}

bool TEX_CheckMiptex( const char *name )
{
	char	texname[64];

	Q_snprintf( texname, sizeof( texname ), "%s.mip", name );

	// check wads in reverse order
	for( int i = g_wadcount - 1; i >= 0; i-- )
	{
		char	*texpath = va( "%s.wad/%s", g_wadlist[i].name, texname );

		if( FS_FileExists( texpath, false ))
			return true;
	}

	// maybe it has non-quake format and lies in textures folder?
	Q_snprintf( texname, sizeof( texname ), "textures/%s", name );

	if( COM_LoadImage(texname, false, FS_LoadFile) )
		return true;

	return false;
}

bool TEX_LoadMiptex( const char *name, mipentry_t *tex )
{
	char		texname[64];
	rgbdata_t	*nonwadtex;

	Q_snprintf( texname, sizeof( texname ), "%s.mip", name );
	Q_strncpy( tex->name, name, sizeof( tex->name ));
	tex->width = tex->height = 16; // same as "*default"

	// check wads in reverse order
	for( int i = g_wadcount - 1; i >= 0; i-- )
	{
		char	*texpath = va( "%s.wad/%s", g_wadlist[i].name, texname );

		if( FS_FileExists( texpath, false ))
		{
			// texture is loaded, wad is used
			SetBits( g_wadlist[i].flags, FWAD_USED );
			tex->data = FS_LoadFile( texpath, &tex->datasize, false );
			tex->width = ((miptex_t *)tex->data)->width;
			tex->height = ((miptex_t *)tex->data)->height;
			tex->wadnum = i; // wad where the tex is located
			return true;
		}
	}

	// maybe it has non-quake format and lies in textures folder?
	Q_snprintf(texname, sizeof(texname), "textures/%s", name);

	nonwadtex = COM_LoadImage(texname, true, FS_LoadFile);

	if( nonwadtex )
	{
		tex->datasize = nonwadtex->size;
		tex->width = nonwadtex->width;
		tex->height = nonwadtex->height;
		tex->wadnum = -1; // wad is not used
		Msg("%s width %d, height %d\n", texname, tex->width, tex->height);
		return true;
	}

	Msg("Texture \"%s\" failed to load\n", name);
	return false;
}

int TEX_FindMiptex( const char *name )
{
	int i;
	ThreadLock ();

	// see if already loaded
	for( i = 0; i < g_nummiptex; i++ )
	{
		if( !Q_stricmp( name, g_miptex[i].name ))
		{
			ThreadUnlock();
			return i;
		}
	}

	if( g_nummiptex == MAX_MAP_TEXTURES )
		COM_FatalError( "MAX_MAP_TEXTURES limit exceeded\n" );

	TEX_LoadMiptex( name, &g_miptex[i] );
	g_nummiptex++;
	ThreadUnlock ();

	return i;
}

void TEX_GetSize( const char *name, int *width, int *height )
{
	int	miptex = TEX_FindMiptex( name );

	*width = g_miptex[miptex].width;
	*height = g_miptex[miptex].height;
}

void TEX_FreeTextures( void )
{
	// purge texture cache
	for( int i = 0; i < g_nummiptex; i++ )
	{
		Mem_Free( g_miptex[i].data, C_FILESYSTEM );
	}
}

int32_t TEX_AddMiptexNameToHashTable(const char *name)
{
	uint32_t hash, i;
	size_t length = std::strlen(name);
	
	for (hash = i = 0; i < length; ++i)
	{
		hash += static_cast<uint8_t>(std::tolower(name[i]));
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	g_hashToMiptexName.insert({ static_cast<int32_t>(hash), name });
	return static_cast<int32_t>(hash);
}

const char *TEX_GetMiptexNameByHash(int32_t hash)
{
	return g_hashToMiptexName[hash].c_str();
}

/*
=================
lump_sorters
=================
*/
static int lump_sorter_by_wad_and_name( const void *lump1, const void *lump2 )
{
	mipentry_t *plump1 = (mipentry_t *)lump1;
	mipentry_t *plump2 = (mipentry_t *)lump2;

	if( plump1->wadnum == plump2->wadnum )
		return Q_stricmp( plump1->name, plump2->name );
	else return plump1->wadnum - plump2->wadnum;
}

/*
==================
LoadLump
==================
*/
size_t LoadLump( mipentry_t *source, byte *dest )
{
	if( source->datasize )
	{
		// should we just load the texture header w/o the palette & bitmap?
		if( source->wadnum == -1 || !FBitSet( g_wadlist[source->wadnum].flags, FWAD_INCLUDE ))
		{
			// Just read the miptex header and zero out the data offsets.
			// We will load the entire texture from the WAD at engine runtime
			miptex_t	*miptex = (miptex_t *)dest;

			Q_strncpy( miptex->name, source->name, WAD3_NAMELEN );
			miptex->width = source->width;
			miptex->height = source->height;
			for( int i = 0; i < MIPLEVELS; i++ )
				miptex->offsets[i] = 0;

			return sizeof( miptex_t );
		}
		else
		{
			// Load the entire texture here so the BSP contains the texture
			memcpy( dest, source->data, source->datasize );
			return source->datasize;
		}
	}

	return 0;
}

/*
==================
LumpSize

returns the size of lump
==================
*/
size_t LumpSize( const mipentry_t *source )
{
	if( source->datasize )
	{
		// should we just load the texture header w/o the palette & bitmap?
		if( source->wadnum == -1 || !FBitSet( g_wadlist[source->wadnum].flags, FWAD_INCLUDE ))
			return sizeof( miptex_t );
		return source->datasize;
	}
	return 0;
}

/*
==================
AddAnimatingTextures
==================
*/
void AddAnimatingTextures( void )
{
	int	base = g_nummiptex;
	char	name[64];
	
	for( int i = 0; i < base; i++ )
	{
		if( g_miptex[i].name[0] != '+' && g_miptex[i].name[0] != '-' )
			continue;
		Q_strcpy( name, g_miptex[i].name );

		for( int j = 0; j < 20; j++ )
		{
			if( j < 10 ) name[1] = '0' + j;
			else name[1] = 'A' + j - 10; // alternate animation

			// see if this name exists in the wadfile
			if( TEX_CheckMiptex( name ))
			{
				// add to the miptex list
				TEX_FindMiptex( name );
			}
		}
	}
	
	if( g_nummiptex - base )
		MsgDev( D_REPORT, "added %i additional animating textures.\n", g_nummiptex - base );
}

/*
==================
CheckSpecialTexture
==================
*/
bool CheckSpecialTexture( const char *name )
{
	if( !Q_stricmp( name, "NULL" ))
		return true;
	if( !Q_stricmp( name, "SKIP" ))
		return true;
	if( !Q_stricmp( name, "HINT" ))
		return true;
	if( !Q_stricmp( name, "SOLIDHINT" ))
		return true;
	if( !Q_stricmp( name, "SKY" ))
		return true;
	if( !Q_stricmp( name, "ORIGIN" ))
		return true;
	if( !Q_stricmp( name, "CLIP" ))
		return true;
	return false;
}

/*
==================
FixSkyMiptex

for the mystery reason, 'sky' texture name should be in lowercase for GoldSrc.
otherwise it will cause terrible bug with displaying lightmap on sky surfaces
and crashing when trying to illuminate sky surfaces with dlight
==================
*/
static void FixSkyMiptex(dmiptexlump_t *miptexLump)
{
	ptrdiff_t miptexLumpAddress = reinterpret_cast<ptrdiff_t>(miptexLump);
	for (size_t i = 0; i < miptexLump->nummiptex; ++i)
	{
		miptex_t *miptex = reinterpret_cast<miptex_t*>(miptexLumpAddress + miptexLump->dataofs[i]);
		if (!Q_strnicmp(miptex->name, "sky", 3)) 
		{
			size_t length = std::strlen(miptex->name);
			for (size_t i = 0; i < length; ++i) {
				miptex->name[i] = std::tolower(miptex->name[i]);
			}
		}
	}
}

/*
==================
WriteMiptex
==================
*/
void WriteMiptex( void )
{
	dtexinfo_t	*tx = g_texinfo;
	char		szTmpWad[1024];
	size_t		i, len;
	byte		*data;
	dmiptexlump_t	*l;

	g_texdatasize = 0;
	szTmpWad[0] = 0;

	// add animating textures finally
	AddAnimatingTextures();

	// sort them FIRST by wadfile and THEN by name for most efficient loading in the engine.
	qsort( (void *)g_miptex, (size_t)g_nummiptex, sizeof( g_miptex[0] ), lump_sorter_by_wad_and_name );

	// Sleazy Hack 104 Pt 2 - After sorting the miptex array, reset the texinfos to point to the right miptexs
	for( i = 0; i < g_numtexinfo; i++, tx++ )
	{
		const char *miptex_name = TEX_GetMiptexNameByHash(tx->miptex); //(char *)tx->miptex;
		tx->miptex = TEX_FindMiptex( miptex_name );
		// free up the originally strdup()'ed miptex_name
		//Mem_Free( miptex_name );
	}

	size_t totaldatasize = sizeof(int) + (sizeof(int) * g_nummiptex);

	for( i = 0; i < g_nummiptex; i++ )
	{
		len = LumpSize( g_miptex + i );
		totaldatasize += len;
	}

	Msg( "total texture data %s\n", Q_memprint( totaldatasize ));
	g_dtexdata = (byte *)Mem_Alloc( totaldatasize );

	if( totaldatasize > MAX_MAP_MIPTEX )
		MsgDev( D_WARN, "MAX_MAP_MIPTEX limit overflow\n" );

	// now setup to get the miptex data (or just the headers if using -wadtextures) from the wadfile
	l = (dmiptexlump_t *)g_dtexdata;
	data = (byte *)&l->dataofs[g_nummiptex];
	l->nummiptex = g_nummiptex;

	for( i = 0; i < g_nummiptex; i++ )
	{
		l->dataofs[i] = data - (byte *)l;
		len = LoadLump( g_miptex + i, data );
		if( !len ) l->dataofs[i] = -1;	// didn't find the texture
		data += len;
	}

	g_texdatasize = data - g_dtexdata;

	if( totaldatasize != g_texdatasize )
		COM_FatalError( "WriteMiptex: memory corrupted\n" );

	if (g_compatibility_mode == CompatibilityMode::GoldSrc) {
		FixSkyMiptex((dmiptexlump_t *)g_dtexdata);
	}

	for( i = 0; i < g_wadcount; i++ )
	{
		// all used textures from this wad will be include into map
		if( FBitSet( g_wadlist[i].flags, FWAD_INCLUDE ))
			continue;

		// nothing used from this wad
		if( !FBitSet( g_wadlist[i].flags, FWAD_USED ))
			continue;

		Q_snprintf( szTmpWad, sizeof( szTmpWad ), "%s%s.wad;", szTmpWad, g_wadlist[i].name );
		MsgDev( D_INFO, "Using WAD File: %s.wad\n", g_wadlist[i].name );
	}

	len = Q_strlen( szTmpWad );

	// cutoff last semicolon
	if( len > 0 && szTmpWad[Q_strlen( szTmpWad ) - 1] == ';' )
		szTmpWad[len-1] = '\0';

	if( *szTmpWad ) MsgDev( D_REPORT, "Wad files required to run the map: \"%s\"\n", szTmpWad );
	else MsgDev( D_REPORT, "Wad files required to run the map: (None)\n" );

	// update 'wad' field
	SetKeyValue( (entity_t *)&g_mapentities[0], "wad", szTmpWad );
}

// =====================================================================================
//  FaceinfoForTexinfo
// =====================================================================================
short FaceinfoForTexinfo( const char *landname, const int in_texture_step, const int in_max_extent, const int groupid )
{
	byte		texture_step = bound( MIN_CUSTOM_TEXTURE_STEP, in_texture_step, MAX_CUSTOM_TEXTURE_STEP );
	byte		max_extent = bound( MIN_CUSTOM_SURFACE_EXTENT, in_max_extent, MAX_CUSTOM_SURFACE_EXTENT );
	dfaceinfo_t	*fi = g_dfaceinfo;
	int		i;

	ThreadLock();

	for( i = 0; i < g_numfaceinfo; i++, fi++ )
	{
		if( Q_stricmp( landname, fi->landname ))
			continue;

		if( fi->texture_step != texture_step )
			continue;

		if( fi->max_extent != max_extent )
			continue;

		if( fi->groupid != groupid )
			continue;

		ThreadUnlock();
		return i;
	}

	if( g_numfaceinfo == MAX_MAP_FACEINFO )
		COM_FatalError( "MAX_MAP_FACEINFO limit exceeded\n" );

	// allocate tne entry
	Q_strncpy( fi->landname, landname, sizeof( fi->landname ));
	fi->texture_step = texture_step;
	fi->max_extent = max_extent;
	fi->groupid = groupid;
	g_numfaceinfo++;

	ThreadUnlock();

	return i;
}

/*
==================
ComputeAxisBase
==================
*/
void ComputeAxisBase( vec3_t normal, vec3_t texX, vec3_t texY )
{
	vec_t	RotY, RotZ;

	if( fabs( normal[0] ) < 1e-6 )
		normal[0] = 0.0f;

	if( fabs( normal[1] ) < 1e-6 )
		normal[1] = 0.0f;

	if( fabs( normal[2] ) < 1e-6 )
		normal[2] = 0.0f;

	// compute the two rotations around y and z to rotate x to normal
	RotY = -atan2( normal[2], sqrt( normal[1] * normal[1] + normal[0] * normal[0] ));
	RotZ = atan2( normal[1], normal[0] );

	// rotate (0,1,0) and (0,0,1) to compute texX and texY
	texX[0] = -sin( RotZ );
	texX[1] = cos( RotZ );
	texX[2] = 0;

	// the texY vector is along -z (t texture coorinates axis)
	texY[0] = -sin( RotY ) * cos( RotZ );
	texY[1] = -sin( RotY ) * sin( RotZ );
	texY[2] = -cos( RotY );
}

/*
==================
TextureAxisFromPlane
==================
*/
void TextureAxisFromNormal( vec3_t normal, vec3_t xv, vec3_t yv, bool brush_primitive )
{
	vec_t	dot, best = 0.0;	
	int	bestaxis = 0;

	if( brush_primitive )
	{
		ComputeAxisBase( normal, xv, yv );
		return;
	}

	for( int i = 0; i < 6; i++ )
	{
		dot = DotProduct( normal, baseaxis[i*3+0] );

		if( dot > best + 0.0001 )
		{
			bestaxis = i;
			best = dot;
		}
	}

	VectorCopy( baseaxis[bestaxis*3+1], xv );
	VectorCopy( baseaxis[bestaxis*3+2], yv );
}

/*
==================
TextureAxisFromPlane
==================
*/
void TextureAxisFromSide( const side_t *side, vec3_t xv, vec3_t yv, bool brush_primitive )
{
	vec3_t	t1, t2, normal;

	VectorSubtract( side->planepts[0], side->planepts[1], t1 );
	VectorSubtract( side->planepts[2], side->planepts[1], t2 );
	CrossProduct( t1, t2, normal );
	VectorNormalize( normal );
	TextureAxisFromNormal( normal, xv, yv, brush_primitive );
}

int TexinfoForSide( plane_t *plane, side_t *s, const vec3_t origin )
{
	vec_t		vecs[2][4];
	dtexinfo_t	tx, *tc;
	int		i, j, k;

	if( FBitSet( s->flags, FSIDE_NODRAW|FSIDE_SKIP ))
		return -1;

	memset( &tx, 0, sizeof( tx ));
	tx.miptex = TEX_FindMiptex( s->name );
	// Note: FindMiptex() still needs to be called here to add it to the global miptex array
	tx.faceinfo = s->faceinfo;

	if( FBitSet( s->flags, FSIDE_NOLIGHTMAP ))
		SetBits( tx.flags, TEX_SPECIAL );

	if( FBitSet( s->flags, FSIDE_NOSHADOW ))
		SetBits( tx.flags, TEX_NOSHADOW );

	if( FBitSet( s->flags, FSIDE_NODIRT ))
		SetBits( tx.flags, TEX_NODIRT );

	if( FBitSet( s->flags, FSIDE_SCROLL ))
		SetBits( tx.flags, TEX_SCROLL );

	if( !FBitSet( s->flags, FSIDE_PATCH ))
	{
		if( g_world_luxels >= 1 )
			SetBits( tx.flags, TEX_WORLD_LUXELS );

		if( g_world_luxels >= 2 )
			SetBits( tx.flags, TEX_AXIAL_LUXELS );
	}

	// prepare the source vecs
	memcpy( vecs, s->vecs, sizeof( vecs ));

	// add the origin offset
	if( origin[0] || origin[1] || origin[2] )
	{
		vecs[0][3] += DotProduct( origin, vecs[0] );
		vecs[1][3] += DotProduct( origin, vecs[1] );
	}

	// can't use memcpy because size can be mismatched
	for( i = 0; i < 2; i++ )
	{
		for( j = 0; j < 4; j++ )
		{
			tx.vecs[i][j] = vecs[i][j];
		}
	}

	//
	// find the texinfo
	//
	ThreadLock();
	tc = g_texinfo;

	for( i = 0; i < g_numtexinfo; i++, tc++ )
	{
		// Sleazy hack 104, Pt 3 - Use strcmp on names to avoid dups
		if (Q_stricmp(TEX_GetMiptexNameByHash(tc->miptex), s->name)) // if( Q_stricmp( (char *)tc->miptex, s->name ))
			continue;

		if( tc->flags != tx.flags )
			continue;

		if( tc->faceinfo != tx.faceinfo )
			continue;

		for( j = 0; j < 2; j++ )
		{
			for( k = 0; k < 4; k++ )
			{
				if( tc->vecs[j][k] != tx.vecs[j][k] )
					goto skip;
			}
		}

		ThreadUnlock();
		return i;
skip:;
	}

	if( g_numtexinfo == MAX_MAP_TEXINFO )
		COM_FatalError( "MAX_MAP_TEXINFO limit exceeded\n" );

	// Very Sleazy Hack 104 - since the tx.miptex index will be bogus after we sort the miptex array later
	// Put the string name of the miptex in this "index" until after we are done sorting it in WriteMiptex().
	// g-cont. do it only for unique elements, but i can't use copysting here because it may call ThreadLock again
	tx.miptex = TEX_AddMiptexNameToHashTable(s->name); //(int)strdup( s->name );

	*tc = tx;
	g_numtexinfo++;
	ThreadUnlock ();

	return i;
}