//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.cpp
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include "ViewerSettings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mx.h>
#include <time.h>
#include "SpriteModel.h"
#include "stringlib.h"
#include <io.h>

ViewerSettings g_viewerSettings;

void InitViewerSettings( void )
{
	memset (&g_viewerSettings, 0, sizeof (ViewerSettings));

	g_viewerSettings.renderMode = SPR_NORMAL;
	g_viewerSettings.orientType = SPR_FWD_PARALLEL;

	g_viewerSettings.bgColor[0] = 0.5f;
	g_viewerSettings.bgColor[1] = 0.5f;
	g_viewerSettings.bgColor[2] = 0.5f;

	g_viewerSettings.gColor[0] = 0.85f;
	g_viewerSettings.gColor[1] = 0.85f;
	g_viewerSettings.gColor[2] = 0.69f;

	g_viewerSettings.lColor[0] = 1.0f;
	g_viewerSettings.lColor[1] = 1.0f;
	g_viewerSettings.lColor[2] = 1.0f;

	g_viewerSettings.speedScale = 1.0f;
	g_viewerSettings.textureLimit = 256;
	g_viewerSettings.showGround = 0;
	g_viewerSettings.showMaximized = 0;
	g_viewerSettings.sequence_autoplay = true;

	// init random generator
	srand( (unsigned)time( NULL ) );
}

bool InitRegistry( void )
{
	return mx::regCreateKey( HKEY_CURRENT_USER, "Software\\XashXT Group\\Half-Life SpriteViewer" );
}

bool SaveString( const char *pKey, char *pValue )
{
	return mx::regSetValue( HKEY_CURRENT_USER, "Software\\XashXT Group\\Half-Life SpriteViewer", pKey, pValue );
}

bool LoadString( const char *pKey, char *pValue )
{
	return mx::regGetValue( HKEY_CURRENT_USER, "Software\\XashXT Group\\Half-Life SpriteViewer", pKey, pValue );
}

void SaveVector4D( const char *pName, float *pValue )
{
	char	str[256];

	mx_snprintf( str, sizeof( str ), "%g %g %g %g", pValue[0], pValue[1], pValue[2], pValue[3] );
	SaveString( pName, str );
}

void LoadVector4D( const char *pName, float *pValue )
{
	char	str[256];

	if( LoadString( pName, str ))
	{
		sscanf( str, "%g %g %g %g", &pValue[0], &pValue[1], &pValue[2], &pValue[3] );
	}
}

void SaveVector3D( const char *pName, float *pValue )
{
	char	str[256];

	mx_snprintf( str, sizeof( str ), "%g %g %g", pValue[0], pValue[1], pValue[2] );
	SaveString( pName, str );
}

void LoadVector3D( const char *pName, float *pValue )
{
	char	str[256];

	if( LoadString( pName, str ))
	{
		sscanf( str, "%g %g %g", &pValue[0], &pValue[1], &pValue[2] );
	}
}

void SaveInt( const char *pName, int iValue )
{
	char	str[256];

	mx_snprintf( str, sizeof( str ), "%d", iValue );
	SaveString( pName, str );
}

void LoadInt( const char *pName, int *iValue )
{
	char	str[256];

	if( LoadString( pName, str ))
	{
		sscanf( str, "%d", iValue );
	}
}

void SaveFloat( const char *pName, float fValue )
{
	char	str[256];

	mx_snprintf( str, sizeof( str ), "%f", fValue );
	SaveString( pName, str );
}

void LoadFloat( const char *pName, float *fValue )
{
	char	str[256];

	if( LoadString( pName, str ))
          {
		sscanf( str, "%f", fValue );
	}
}

int LoadViewerSettings( void )
{
	InitViewerSettings ();

	LoadVector4D( "Background Color", g_viewerSettings.bgColor );
	LoadVector4D( "Light Color", g_viewerSettings.lColor );
	LoadVector4D( "Ground Color", g_viewerSettings.gColor );
	LoadInt( "Sequence AutoPlay", &g_viewerSettings.sequence_autoplay );
	LoadInt( "Show Ground", &g_viewerSettings.showGround );
	LoadString( "Ground TexPath", g_viewerSettings.groundTexFile );
	LoadInt( "Show Maximized", &g_viewerSettings.showMaximized );

	return 1;
}

int SaveViewerSettings( void )
{
	if( !InitRegistry( ))
		return 0;

	SaveVector4D( "Background Color", g_viewerSettings.bgColor );
	SaveVector4D( "Light Color", g_viewerSettings.lColor );
	SaveVector4D( "Ground Color", g_viewerSettings.gColor );
	SaveInt( "Sequence AutoPlay", g_viewerSettings.sequence_autoplay );
	SaveInt( "Show Ground", g_viewerSettings.showGround );
	SaveString( "Ground TexPath", g_viewerSettings.groundTexFile );
	SaveInt( "Show Maximized", g_viewerSettings.showMaximized );

	return 1;
}

bool IsSpriteModel( const char *path )
{
	byte	buffer[256];
	FILE	*fp;

	if( !path ) return false;

	// load the sprite
	if(( fp = fopen( path, "rb" )) == NULL )
		return false;

	fread( buffer, sizeof( buffer ), 1, fp );
	fclose( fp );

	if( ftell( fp ) < sizeof( 36 ))
		return false;

	// skip invalid signature
	if( Q_strncmp((const char *)buffer, "IDSP", 4 ))
		return false;

	// skip unknown version
	if( *(int *)&buffer[4] != 1 && *(int *)&buffer[4] != 2 )
		return false;
	return true;
}

static bool ValidateSprite( const char *path )
{
	byte	buffer[256];
	dsprite_t	*phdr;
	FILE	*fp;

	if( !path ) return false;

	// load the sprite
	if(( fp = fopen( path, "rb" )) == NULL )
		return false;

	fread( buffer, sizeof( buffer ), 1, fp );
	fclose( fp );

	if( ftell( fp ) < sizeof( dsprite_t ))
		return false;

	// skip invalid signature
	if( Q_strncmp((const char *)buffer, "IDSP", 4 ))
		return false;

	phdr = (dsprite_t *)buffer;

	// skip unknown version
	if( phdr->version != SPRITE_VERSION_Q1 && phdr->version != SPRITE_VERSION_HL )
		return false;

	return true;
}

static void AddPathToList( const char *path )
{
	char	spritePath[256];

	if( g_viewerSettings.numSpritePathes >= 2048 )
		return; // too many strings

	Q_snprintf( spritePath, sizeof( spritePath ), "%s\\%s", g_viewerSettings.oldSpritePath, path );

	if( !ValidateSprite( spritePath ))
		return;

	int i = g_viewerSettings.numSpritePathes++;

	Q_strncpy( g_viewerSettings.spritePathList[i], spritePath, sizeof( g_viewerSettings.spritePathList[0] ));
}

static void SortPathList( void )
{
	char	temp[256];
	int	i, j;

	// this is a selection sort (finds the best entry for each slot)
	for( i = 0; i < g_viewerSettings.numSpritePathes - 1; i++ )
	{
		for( j = i + 1; j < g_viewerSettings.numSpritePathes; j++ )
		{
			if( Q_strcmp( g_viewerSettings.spritePathList[i], g_viewerSettings.spritePathList[j] ) > 0 )
			{
				Q_strncpy( temp, g_viewerSettings.spritePathList[i], sizeof( temp ));
				Q_strncpy( g_viewerSettings.spritePathList[i], g_viewerSettings.spritePathList[j], sizeof( temp ));
				Q_strncpy( g_viewerSettings.spritePathList[j], temp, sizeof( temp ));
			}
		}
	}
}

void ListDirectory( void )
{
	char spritePath[256];
	struct _finddata_t	n_file;
	long hFile;

	COM_ExtractFilePath( g_viewerSettings.spritePath, spritePath );

	if( !Q_stricmp( spritePath, g_viewerSettings.oldSpritePath ))
		return;	// not changed

	Q_strncpy( g_viewerSettings.oldSpritePath, spritePath, sizeof( g_viewerSettings.oldSpritePath ));
	Q_strncat( spritePath, "\\*.spr", sizeof( spritePath ));
	g_viewerSettings.numSpritePathes = 0;

	// ask for the directory listing handle
	hFile = _findfirst( spritePath, &n_file );
	if( hFile == -1 ) return; // how this possible?

	// start a new chain with the the first name
	AddPathToList( n_file.name );

	// iterate through the directory
	while( _findnext( hFile, &n_file ) == 0 )
		AddPathToList( n_file.name );
	_findclose( hFile );

	SortPathList();
}

const char *LoadNextSprite( void )
{
	int	i;

	for( i = 0; i < g_viewerSettings.numSpritePathes; i++ )
	{
		if( !Q_stricmp( g_viewerSettings.spritePathList[i], g_viewerSettings.spritePath ))
		{
			i++;
			break;
		}
	}

	if( i == g_viewerSettings.numSpritePathes )
		i = 0;
	return g_viewerSettings.spritePathList[i];
}

const char *LoadPrevSprite( void )
{
	int	i;

	for( i = 0; i < g_viewerSettings.numSpritePathes; i++ )
	{
		if( !Q_stricmp( g_viewerSettings.spritePathList[i], g_viewerSettings.spritePath ))
		{
			i--;
			break;
		}
	}

	if( i < 0 ) i = g_viewerSettings.numSpritePathes - 1;
	return g_viewerSettings.spritePathList[i];
}