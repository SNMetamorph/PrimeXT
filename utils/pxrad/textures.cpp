/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// textures.c

#include "qrad.h"

typedef struct wadlist_s
{
	char		wadnames[MAX_TEXFILES][32];
	int		count;
} wadlist_t;

bool		g_wadtextures = DEFAULT_WADTEXTURES;	// completely ignore textures in the bsp-file if enabled
static miptex_t	*g_textures[MAX_MAP_TEXTURES];
static wadlist_t	wadlist;

void TEX_LoadTextures( void )
{
	char	wadstring[MAX_VALUE];
	char	*pszWadString;
	char	*pszWadFile;

	memset( g_textures, 0 , sizeof( g_textures ));
	wadlist.count = 0;

	if( g_numentities <= 0 )
		return;

	pszWadString = ValueForKey( g_entities, "wad" );
	if( !*pszWadString ) pszWadString = ValueForKey( g_entities, "_wad" );
	if( !*pszWadString ) return; // may be -nowadtextures was used?

	Q_strncpy( wadstring, pszWadString, MAX_VALUE - 2 );
	wadstring[MAX_VALUE-1] = 0;

	if( !Q_strchr( wadstring, ';' ))
		Q_strcat( wadstring, ";" );

	// parse wad pathes
	for( pszWadFile = strtok( wadstring, ";" ); pszWadFile != NULL; pszWadFile = strtok( NULL, ";" ))
	{
		COM_FixSlashes( pszWadFile );
		COM_FileBase( pszWadFile, wadlist.wadnames[wadlist.count++] );
		if( wadlist.count >= MAX_TEXFILES )
			break; // too many wads...
	}
}

miptex_t *GetTextureByMiptex( int miptex )
{
	ASSERT( g_dtexdata != NULL );

	int	nummiptex = ((dmiptexlump_t*)g_dtexdata)->nummiptex;
	char	texname[64];
	int	ofs;

	if( miptex < 0 || miptex >= nummiptex )
		return NULL; // bad miptex ?

	if(( ofs = ((dmiptexlump_t*)g_dtexdata)->dataofs[miptex] ) == -1 )
		return NULL; // texture is completely missed

	// check cache
	if( g_textures[miptex] )
		return g_textures[miptex];

	miptex_t	*mt = (miptex_t *)(&g_dtexdata[ofs]);

	// trying wad texture (force while g_wadtextures is 1)
	if( g_wadtextures || mt->offsets[0] <= 0 )
	{
		Q_snprintf( texname, sizeof( texname ), "%s.mip", mt->name );

		// check wads in reverse order
		for( int i = wadlist.count - 1; i >= 0; i-- )
		{
			char	*texpath = va( "%s.wad/%s", wadlist.wadnames[i], texname );

			if( FS_FileExists( texpath, false ))
			{
				g_textures[miptex] = (miptex_t *)FS_LoadFile( texpath, NULL, false );
				break;
			}
		}
	}

	// wad failed, so use internal texture (if present)
	if( mt->offsets[0] > 0 && !g_textures[miptex] )
	{
		size_t	size = sizeof( miptex_t ) + ((mt->width * mt->height * 85) >> 6) + sizeof( short ) + 768;
		g_textures[miptex] = (miptex_t *)Mem_Alloc( size, C_FILESYSTEM );
		memcpy( g_textures[miptex], &g_dtexdata[ofs], size );
	}

	return g_textures[miptex];
}

void TEX_FreeTextures( void )
{
	// purge texture cache
	for( int i = 0; i < MAX_MAP_TEXTURES; i++ )
	{
		Mem_Free( g_textures[i], C_FILESYSTEM );
	}
}
