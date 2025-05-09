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

#include "vector.h"
#include "studio.h"
#include <array>

enum // render modes
{
	RM_WIREFRAME,
	RM_FLATSHADED,
	RM_SMOOTHSHADED,
	RM_TEXTURED,
	RM_BONEWEIGHTS,
	RM_NORMALS
};

enum
{
	EDIT_SOURCE = 0,
	EDIT_MODEL,
};

typedef struct
{
	// model 
	Vector rot;
	Vector trans;
	float movementScale;
	float editStep;
	int editMode;
	bool editSize;

	// render
	int renderMode;
	float transparency;
	bool showBackground;
	int showGround;
	bool showHitBoxes;
	bool showBones;
	bool showTexture;
	bool showAttachments;
	bool showNormals;
	bool showWireframeOverlay;
	int enableIK;
	int texture;
	float textureScale;
	int skin;
	bool mirror;
	bool useStencil;	// if 3dfx fullscreen set false
	bool pending_export_uvmap;

	// animation
	int sequence;
	float speedScale;

	// bodyparts and bonecontrollers
	std::array<int, MAXSTUDIOMODELS> submodels;

	// fullscreen
	int width, height;
	bool cds;
	int showMaximized;

	// colors
	float bgColor[4];
	float lColor[4];
	float gColor[4];
	float gLightVec[3];

	int sequence_autoplay;
	int studio_blendweights;
	int topcolor;
	int bottomcolor;

	bool show_uv_map;
	bool overlay_uv_map;
	bool anti_alias_lines;

	// misc
	int textureLimit;
	bool pause;
	int drawn_polys;
	int numModelChanges;	// if user touch settings that directly stored into the model
	int numSourceChanges;	// editor counter

	// only used for fullscreen mode
	char modelFile[256];
	char modelPath[256];
	char oldModelPath[256];
	char backgroundTexFile[256];
	char groundTexFile[256];
	char uvmapPath[256];

	char modelPathList[2048][256];
	int numModelPathes;
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
const char *LoadNextModel( void );
const char *LoadPrevModel( void );
bool IsAliasModel( const char *path );
#ifdef __cplusplus
}
#endif



#endif // INCLUDED_VIEWERSETTINGS
