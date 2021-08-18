//
//                 MDL Viewer (c) 1999 by Mete Ciragan
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
// version:        1.4
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#ifndef INCLUDED_GLWINDOW
#define INCLUDED_GLWINDOW



#ifndef INCLUDED_MXGLWINDOW
#include <mxGlWindow.h>
#endif

#ifndef INCLUDED_SPRVIEWER
#include "sprviewer.h"
#endif

#ifndef INCLUDED_VIEWERSETTINGS
#include "ViewerSettings.h"
#endif

#include "matrix.h"

enum // texture names
{
	TEXTURE_UNUSED = 0,
	TEXTURE_GROUND,
	TEXTURE_BACKGROUND,
	TEXTURE_COUNT
};

#define GL_TEXTURE0_ARB			0x84C0
#define GL_TEXTURE1_ARB			0x84C1
#define GL_GENERATE_MIPMAP_SGIS           	0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS      	0x8192
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF
#define GL_MULTISAMPLE			0x809D

class mxImage;

class GlWindow : public mxGlWindow
{
	unsigned int d_textureNames[TEXTURE_COUNT]; // 0 = none, 1 = model, 2 = weapon, 3 = water, 4 = font
	float d_pol; // interpolate value 0.0f - 1.0f
	int d_currFrame, d_startFrame, d_endFrame;
public:
	friend SPRViewer;

	// CREATORS
	GlWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style);
	~GlWindow ();

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	virtual void draw ();

	void drawFloor( int texture = 0 );
	int loadSprite (const char *filename, bool centerView = true);
	int loadTexture( const char *filename, int name );
	int loadTextureImage( mxImage *image, int name );
	void setupRenderMode( void );
	void setRenderMode (int mode);
	void setOrientType (int mode);
	void setFrameInfo (int startFrame, int endFrame);
	// ACCESSORS
	int getRenderMode () const { return g_viewerSettings.renderMode; }
	int getCurrFrame () const { return d_currFrame; }
	int getStartFrame () const { return d_startFrame; }
	int getEndFrame () const { return d_endFrame; }
};

extern GlWindow *g_GlWindow;

#endif // INCLUDED_GLWINDOW
