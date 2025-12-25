//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           GlWindow.h
// last modified:  Apr 28 1999, Mete Ciragan
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
#ifndef INCLUDED_GLWINDOW
#define INCLUDED_GLWINDOW



#ifndef INCLUDED_MXGLWINDOW
#include <mxGlWindow.h>
#endif

#ifndef INCLUDED_VIEWERSETTINGS
#include "ViewerSettings.h"
#endif

#include "matrix.h"
#include "conprint.h"
#include "basetypes.h"

enum // texture names
{
	TEXTURE_UNUSED = 0,
	TEXTURE_GROUND,
	TEXTURE_BACKGROUND,
	TEXTURE_MUZZLEFLASH1,
	TEXTURE_MUZZLEFLASH2,
	TEXTURE_MUZZLEFLASH3,
	TEXTURE_COUNT
};

#define GL_GENERATE_MIPMAP_SGIS           	0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS      	0x8192
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF
#define GL_MULTISAMPLE			0x809D

class StudioModel;
class ControlPanel;
class ViewerSettings;
class mxImage;

class GlWindow : public mxGlWindow
{
	int d_textureNames[TEXTURE_COUNT];

	matrix4x4		m_projection;
	matrix4x4		m_modelview;
	ControlPanel	*d_cpl;
	StudioModel		&m_studioModel;
	ViewerSettings	&m_settings;

public:
	// CREATORS
	GlWindow (mxWindow *parent, ViewerSettings &settings, StudioModel &model, int x, int y, int w, int h, const char *label, int style);
	~GlWindow ();

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	virtual void draw ();

	int loadTexture (const char *filename, int name);
	int loadTextureBuffer( const byte *buffer, size_t size, int name );
	int loadTextureImage( mxImage *image, int name );
	void dumpViewport (const char *filename);
	void GluPerspective( float fov_y );
	void SetProjectionMatrix( const matrix4x4 source );
	void SetModelviewMatrix( const matrix4x4 source );
	void ResetModelviewMatrix( void );
	void setupRenderMode( void );
	void drawFloor( int texture = 0 );
	const matrix4x4& GetModelview( void ) const { return m_modelview; } 
	void setControlPanel( ControlPanel *panel ) { d_cpl = panel; }

	mxImage *readBmpFromBuffer(const byte *buffer, size_t size);
	mxImage *readTextureFromFile(const char *filename);
	void imageFree(mxImage *image);

	// ACCESSORS
	matrix3x3	vectors;	// muzzleflashes uses this
};



extern GlWindow *g_GlWindow;



#endif // INCLUDED_GLWINDOW
