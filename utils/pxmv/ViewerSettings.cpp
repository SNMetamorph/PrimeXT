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
#include "StudioModel.h"
#include "stringlib.h"
#include <io.h>
#include "conprint.h"

ViewerSettings g_viewerSettings;

void InitViewerSettings( void )
{
	memset (&g_viewerSettings, 0, sizeof (ViewerSettings));

	g_viewerSettings.renderMode = RM_TEXTURED;
	g_viewerSettings.transparency = 1.0f;
	g_viewerSettings.movementScale = 1.0f;
	g_viewerSettings.enableIK = false;

	g_viewerSettings.bgColor[0] = 0.5f;
	g_viewerSettings.bgColor[1] = 0.5f;
	g_viewerSettings.bgColor[2] = 0.5f;

	g_viewerSettings.gColor[0] = 0.85f;
	g_viewerSettings.gColor[1] = 0.85f;
	g_viewerSettings.gColor[2] = 0.69f;

	g_viewerSettings.lColor[0] = 1.0f;
	g_viewerSettings.lColor[1] = 1.0f;
	g_viewerSettings.lColor[2] = 1.0f;

	g_viewerSettings.gLightVec[0] = 0.0f;
	g_viewerSettings.gLightVec[1] = 0.0f;
	g_viewerSettings.gLightVec[2] = -1.0f;

	g_viewerSettings.speedScale = 1.0f;
	g_viewerSettings.textureLimit = 256;
	g_viewerSettings.showGround = 0;
	g_viewerSettings.showMaximized = 0;

	g_viewerSettings.textureScale = 1.0f;
	g_viewerSettings.sequence_autoplay = true;
	g_viewerSettings.studio_blendweights = true;
	g_viewerSettings.topcolor = 0;
	g_viewerSettings.bottomcolor = 0;

	g_viewerSettings.editStep = 1.0f;
	g_viewerSettings.editMode = EDIT_SOURCE;
	g_viewerSettings.editSize = false;

	// init random generator
	srand( (unsigned)time( NULL ) );
}

bool InitRegistry( void )
{
	return mx::regCreateKey( HKEY_CURRENT_USER, "Software\\XashXT Group\\Paranoia 2 ModelViewer" );
}

bool SaveString( const char *pKey, char *pValue )
{
	return mx::regSetValue( HKEY_CURRENT_USER, "Software\\XashXT Group\\Paranoia 2 ModelViewer", pKey, pValue );
}

bool LoadString( const char *pKey, char *pValue )
{
	return mx::regGetValue( HKEY_CURRENT_USER, "Software\\XashXT Group\\Paranoia 2 ModelViewer", pKey, pValue );
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
	LoadVector3D( "Light Vector", g_viewerSettings.gLightVec );
	LoadInt( "Sequence AutoPlay", &g_viewerSettings.sequence_autoplay );
	LoadInt( "Studio BlendWeights", &g_viewerSettings.studio_blendweights );
	LoadInt( "Top Color", &g_viewerSettings.topcolor );
	LoadInt( "Bottom Color", &g_viewerSettings.bottomcolor );
	LoadFloat( "Edit Step", &g_viewerSettings.editStep );
	LoadInt( "Edit Mode", &g_viewerSettings.editMode );
	LoadInt( "Allow IK", &g_viewerSettings.enableIK );
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
	SaveVector3D( "Light Vector", g_viewerSettings.gLightVec );
	SaveInt( "Sequence AutoPlay", g_viewerSettings.sequence_autoplay );
	SaveInt( "Studio BlendWeights", g_viewerSettings.studio_blendweights );
	SaveInt( "Top Color", g_viewerSettings.topcolor );
	SaveInt( "Bottom Color", g_viewerSettings.bottomcolor );
	SaveFloat( "Edit Step", g_viewerSettings.editStep );
	SaveInt( "Edit Mode", g_viewerSettings.editMode );
	SaveInt( "Allow IK", g_viewerSettings.enableIK );
	SaveInt( "Show Ground", g_viewerSettings.showGround );
	SaveString( "Ground TexPath", g_viewerSettings.groundTexFile );
	SaveInt( "Show Maximized", g_viewerSettings.showMaximized );

	return 1;
}

bool IsAliasModel( const char *path )
{
	byte	buffer[256];
	FILE	*fp;

	if( !path ) return false;

	// load the model
	if(( fp = fopen( path, "rb" )) == NULL )
		return false;

	fread( buffer, sizeof( buffer ), 1, fp );
	fclose( fp );

	if( ftell( fp ) < sizeof( 84 ))
		return false;

	// skip invalid signature
	if( Q_strncmp((const char *)buffer, "IDPO", 4 ))
		return false;

	// skip unknown version
	if( *(int *)&buffer[4] != 6 )
		return false;

	return true;
}

static bool ValidateModel( const char *path )
{
	byte		buffer[256];
	studiohdr_t	*phdr;
	FILE		*fp;

	if( !path ) return false;

	// load the model
	if(( fp = fopen( path, "rb" )) == NULL )
		return false;

	fread( buffer, sizeof( buffer ), 1, fp );
	fclose( fp );

	if( ftell( fp ) < sizeof( studiohdr_t ))
		return false;

	// skip invalid signature
	if( Q_strncmp((const char *)buffer, "IDST", 4 ))
		return false;

	phdr = (studiohdr_t *)buffer;

	// skip unknown version
	if( phdr->version != STUDIO_VERSION )
		return false;

	// skip modelnameT.mdl
	if( phdr->numbones <= 0 )
		return false;

	return true;
}

static void AddPathToList( const char *path )
{
	char	modelPath[256];

	if( g_viewerSettings.numModelPathes >= 2048 )
		return; // too many strings

	Q_snprintf( modelPath, sizeof( modelPath ), "%s\\%s", g_viewerSettings.oldModelPath, path );

	if( !ValidateModel( modelPath ))
		return;

	int i = g_viewerSettings.numModelPathes++;

	Q_strncpy( g_viewerSettings.modelPathList[i], modelPath, sizeof( g_viewerSettings.modelPathList[0] ));
}

static void SortPathList( void )
{
	char	temp[256];
	int	i, j;

	// this is a selection sort (finds the best entry for each slot)
	for( i = 0; i < g_viewerSettings.numModelPathes - 1; i++ )
	{
		for( j = i + 1; j < g_viewerSettings.numModelPathes; j++ )
		{
			if( Q_strcmp( g_viewerSettings.modelPathList[i], g_viewerSettings.modelPathList[j] ) > 0 )
			{
				Q_strncpy( temp, g_viewerSettings.modelPathList[i], sizeof( temp ));
				Q_strncpy( g_viewerSettings.modelPathList[i], g_viewerSettings.modelPathList[j], sizeof( temp ));
				Q_strncpy( g_viewerSettings.modelPathList[j], temp, sizeof( temp ));
			}
		}
	}
}

void ListDirectory( void )
{
	char modelPath[256];
	struct _finddata_t	n_file;
	long hFile;

	COM_ExtractFilePath( g_viewerSettings.modelPath, modelPath );

	if( !Q_stricmp( modelPath, g_viewerSettings.oldModelPath ))
		return;	// not changed

	Q_strncpy( g_viewerSettings.oldModelPath, modelPath, sizeof( g_viewerSettings.oldModelPath ));
	Q_strncat( modelPath, "\\*.mdl", sizeof( modelPath ));
	g_viewerSettings.numModelPathes = 0;

	// ask for the directory listing handle
	hFile = _findfirst( modelPath, &n_file );
	if( hFile == -1 ) return; // how this possible?

	// start a new chain with the the first name
	AddPathToList( n_file.name );

	// iterate through the directory
	while( _findnext( hFile, &n_file ) == 0 )
		AddPathToList( n_file.name );
	_findclose( hFile );

	SortPathList();
}

const char *LoadNextModel( void )
{
	int	i;

	for( i = 0; i < g_viewerSettings.numModelPathes; i++ )
	{
		if( !Q_stricmp( g_viewerSettings.modelPathList[i], g_viewerSettings.modelPath ))
		{
			i++;
			break;
		}
	}

	if( i == g_viewerSettings.numModelPathes )
		i = 0;
	return g_viewerSettings.modelPathList[i];
}

const char *LoadPrevModel( void )
{
	int	i;

	for( i = 0; i < g_viewerSettings.numModelPathes; i++ )
	{
		if( !Q_stricmp( g_viewerSettings.modelPathList[i], g_viewerSettings.modelPath ))
		{
			i--;
			break;
		}
	}

	if( i < 0 ) i = g_viewerSettings.numModelPathes - 1;
	return g_viewerSettings.modelPathList[i];
}