//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.h
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
#ifndef INCLUDED_VIEWERSETTINGS
#define INCLUDED_VIEWERSETTINGS

#include <basetypes.h>
#include <mathlib.h>

typedef struct
{
	// model 
	vec3_t rot;
	vec3_t trans;
	float movementScale;

	// render
	int renderMode;
	int orientType;
	bool showBackground;
	int showGround;
	int numModelChanges;	// if user touch settings that directly stored into the model
	int texture;

	// animation
	int sequence_autoplay;
	float speedScale;

	int showMaximized;

	// colors
	float bgColor[4];
	float lColor[4];
	float gColor[4];

	// misc
	int textureLimit;
	bool pause;

	// only used for fullscreen mode
	char spriteFile[256];
	char spritePath[256];
	char oldSpritePath[256];
	char backgroundTexFile[256];
	char groundTexFile[256];

	char spritePathList[2048][256];
	int numSpritePathes;
} ViewerSettings;

extern ViewerSettings g_viewerSettings;

bool InitRegistry( void );
bool SaveString( const char *pKey, char *pValue );
bool LoadString( const char *pKey, char *pValue );

#ifdef __cplusplus
extern "C" {
#endif

void InitViewerSettings (void);
int LoadViewerSettings (void);
int SaveViewerSettings (void);
void ListDirectory( void );
const char *LoadNextSprite( void );
const char *LoadPrevSprite( void );
bool IsSpriteModel( const char *path );
#ifdef __cplusplus
}
#endif



#endif // INCLUDED_VIEWERSETTINGS