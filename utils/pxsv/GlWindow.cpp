//
//                 MD2 Viewer (c) 1999 by Mete Ciragan
//
// file:           GlWindow.cpp
// last modified:  Apr 29 1999, Mete Ciragan
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
#include <mx.h>
#include <mxMessageBox.h>
#include <mxTga.h>
#include <mxPcx.h>
#include <mxBmp.h>
#include <gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stringlib.h>
#include "ViewerSettings.h"
#include "GlWindow.h"
#include "SpriteModel.h"

extern bool g_bStopPlaying;
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;

GlWindow *g_GlWindow = 0;

GlWindow::GlWindow( mxWindow *parent, int x, int y, int w, int h, const char *label, int style ) : mxGlWindow( parent, x, y, w, h, label, style )
{
	d_textureNames[0] = 0;
	d_textureNames[1] = 0;
	d_textureNames[2] = 0;

	setFrameInfo (0, 0);
	setRenderMode (SPR_NORMAL);
	setOrientType (SPR_FWD_PARALLEL);
	d_pol = 0.0f;

	glDepthFunc( GL_LEQUAL );
	glCullFace (GL_FRONT);

	mx::setIdleWindow (this);
}

GlWindow::~GlWindow ()
{
	mx::setIdleWindow (0);
	loadTexture (0, TEXTURE_BACKGROUND);
	loadTexture (0, TEXTURE_GROUND);
	loadSprite (0);
}

int GlWindow::handleEvent (mxEvent *event)
{
	static float oldrx, oldry, oldtz, oldtx, oldty;
	static int oldx, oldy;
	static double lastupdate;

	switch (event->event)
	{
	case mxEvent::MouseUp:
	{
		g_viewerSettings.pause = false;
	}
	break;
	case mxEvent::MouseDown:
		oldrx = g_viewerSettings.rot[0];
		oldry = g_viewerSettings.rot[1];
		oldtx = g_viewerSettings.trans[0];
		oldty = g_viewerSettings.trans[1];
		oldtz = g_viewerSettings.trans[2];
		oldx = event->x;
		oldy = event->y;

		// HACKHACK: reset focus to main window to catch hot-keys again
		if( g_SPRViewer ) SetFocus( (HWND) g_SPRViewer->getHandle ());
		g_viewerSettings.pause = true;

		break;

	case mxEvent::MouseDrag:
		if( event->buttons & mxEvent::MouseLeftButton )
		{
			if( event->modifiers & mxEvent::KeyShift )
			{
				g_viewerSettings.trans[0] = oldtx - (float)(event->x - oldx) * g_viewerSettings.movementScale;
				g_viewerSettings.trans[1] = oldty + (float)(event->y - oldy) * g_viewerSettings.movementScale;
			}
			else
			{
				g_viewerSettings.rot[0] = oldrx + (float)(event->y - oldy);
				g_viewerSettings.rot[1] = oldry + (float)(event->x - oldx);
			}
		}
		else if( event->buttons & mxEvent::MouseRightButton )
		{
			g_viewerSettings.trans[2] = oldtz + (float)(event->y - oldy) * g_viewerSettings.movementScale;
		}
		redraw ();
		break;

	case mxEvent::Idle:
	{
		static double prev;
		double curr = (double) mx::getTickCount () / 1000.0;
		double dt = (curr - prev);
#if 1
		// clamp to 100fps
		if( dt >= 0.0 && dt < 0.01 )
		{
			Sleep( max( 10 - dt * 1000.0, 0 ) );
			return 1;
		}
#endif
		g_spriteModel.updateTimings( curr, dt );

		if( !g_bStopPlaying && !g_viewerSettings.pause && prev != 0.0 )
		{
			d_pol += (dt / 0.1) * g_viewerSettings.speedScale;

			if( d_pol >= 1.0f )
			{
				if( d_startFrame == d_endFrame )
					g_spriteModel.setFrame( d_startFrame );
				else g_spriteModel.setFrame( d_currFrame++ );
				d_pol = 0.0f;

				if( d_currFrame > d_endFrame )
					d_currFrame = d_startFrame;
			}
		}

		if( !g_viewerSettings.pause )
			redraw ();

		prev = curr;
	}
	break;

	case mxEvent::KeyDown:
		switch (event->key)
		{
		case 27:
			if( !getParent( )) // fullscreen mode ?
				mx::quit();
			break;
		}
	}

	return 1;
}

void GlWindow :: setupRenderMode( void )
{
	glDisable( GL_MULTISAMPLE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glShadeModel( GL_SMOOTH );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_TEXTURE_2D );
	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glDisable( GL_ALPHA_TEST );
	glDepthMask( GL_TRUE );

	switch( g_viewerSettings.renderMode )
	{
	case SPR_NORMAL:
		break;
	case SPR_ADDITIVE:
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		glDepthMask( GL_FALSE );
		break;
	case SPR_INDEXALPHA:
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask( GL_FALSE );
		break;
	case SPR_ALPHTEST:
#if 0
		glEnable( GL_ALPHA_TEST );
		glAlphaFunc( GL_GREATER, 0.25f );
#else
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask( GL_FALSE );
#endif
		break;
	}
}

void GlWindow :: drawFloor( int texture )
{
	float scale = 5.0f;
	float dist = -100.0f;
	glEnable( GL_MULTISAMPLE );

	if( texture ) 
	{
		static Vector tMap( 0, 0, 0 );
		static Vector dxMap( 1, 0, 0 );
		static Vector dyMap( 0, 1, 0 );

		vec3_t deltaPos;

		g_spriteModel.GetMovement( deltaPos );

		float dpdd = scale / dist;

		tMap[0] = tMap[0] + dxMap[0] * deltaPos[0] * dpdd + dxMap[1] * deltaPos[1] * dpdd;	
		tMap[1] = tMap[1] + dyMap[0] * deltaPos[0] * dpdd + dyMap[1] * deltaPos[1] * dpdd;

		while (tMap[0] < 0.0) tMap[0] +=  1.0f;
		while (tMap[0] > 1.0) tMap[0] += -1.0f;
		while (tMap[1] < 0.0) tMap[1] +=  1.0f;
		while (tMap[1] > 1.0) tMap[1] += -1.0f;

		glBegin( GL_QUADS );
			glTexCoord2f( tMap[0] - (dxMap[0] - dyMap[0]) * scale, tMap[1] - (-dxMap[1] + dyMap[1]) * scale );
			glVertex3f( -dist, -dist, 0 );

			glTexCoord2f( tMap[0] + (dxMap[0] + dyMap[0]) * scale, tMap[1] + (-dxMap[1] - dyMap[1]) * scale );
			glVertex3f( dist, -dist, 0 );

			glTexCoord2f( tMap[0] + (dxMap[0] - dyMap[0]) * scale, tMap[1] + (-dxMap[1] + dyMap[1]) * scale );
			glVertex3f( dist, dist, 0 );

			glTexCoord2f( tMap[0] + (-dxMap[0] - dyMap[0]) * scale, tMap[1] + (dxMap[1] + dyMap[1]) * scale );
			glVertex3f( -dist, dist, 0 );
		glEnd();
	}
	else
	{
		glBegin( GL_QUADS );
			glTexCoord2f( 0.0f, 1.0f );
			glVertex3f( -dist, -dist, 0 );

			glTexCoord2f( 1.0f, 1.0f );
			glVertex3f( dist, -dist, 0 );

			glTexCoord2f( 1.0f, 0.0f );
			glVertex3f( dist, dist, 0 );

			glTexCoord2f( 0.0f, 0.0f );
			glVertex3f( -dist, dist, 0 );
		glEnd();
	}

	glDisable( GL_MULTISAMPLE );
}

void GlWindow::draw ()
{
	glClearColor( g_viewerSettings.bgColor[0], g_viewerSettings.bgColor[1], g_viewerSettings.bgColor[2], 0.0f );

	glClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );

	glViewport( 0, 0, w2(), h2() );
	glDisable( GL_MULTISAMPLE );

	//
	// draw background
	//
	if( g_viewerSettings.showBackground && d_textureNames[TEXTURE_BACKGROUND] )
	{
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glOrtho( 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f );

		glMatrixMode( GL_MODELVIEW );
		glPushMatrix ();
		glLoadIdentity ();

		glDisable( GL_CULL_FACE );
		glEnable( GL_TEXTURE_2D );

		glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

		glBindTexture( GL_TEXTURE_2D, d_textureNames[TEXTURE_BACKGROUND] );

		glBegin( GL_TRIANGLE_STRIP );
			glTexCoord2f( 0, 0 );
			glVertex2f( 0, 0 );
			glTexCoord2f( 1, 0 );
			glVertex2f( 1, 0 );
			glTexCoord2f( 0, 1 );
			glVertex2f( 0, 1 );
			glTexCoord2f( 1, 1 );
			glVertex2f( 1, 1 );
		glEnd();

		glClear( GL_DEPTH_BUFFER_BIT );
		glBindTexture( GL_TEXTURE_2D, 0 );

		glPopMatrix ();
	}

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluPerspective (65.0f, (GLfloat) w () / (GLfloat) h (), 0.1f, 131072.0f );

	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	glLoadIdentity ();

	if( bUseWeaponOrigin )
	{
		glRotatef( -90,  1, 0, 0 );		// put Z going up
		glRotatef( 90,  0, 0, 1 );		// put Z going up
		glTranslatef( 0.0f, 0.0f, 1.0f );	// shift back like in HL
	}
	else
	{
		glTranslatef (-g_viewerSettings.trans[0], -g_viewerSettings.trans[1], -g_viewerSettings.trans[2]);

		glRotatef( g_viewerSettings.rot[0], 1, 0, 0 );
		glRotatef( g_viewerSettings.rot[1], 0, 0, 1 );
	}

	setupRenderMode();

	glCullFace( GL_FRONT );

	g_spriteModel.DrawSprite();

	//
	// draw ground
	//
	if( g_viewerSettings.showGround && !bUseWeaponOrigin )
	{
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_CULL_FACE );
		glDisable( GL_CULL_FACE );

		glEnable( GL_BLEND );

		if( !d_textureNames[TEXTURE_GROUND] )
		{
			glDisable( GL_TEXTURE_2D );
			glColor4f( g_viewerSettings.gColor[0], g_viewerSettings.gColor[1], g_viewerSettings.gColor[2], 0.7f );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		else
		{
			glEnable( GL_TEXTURE_2D );
			glColor4f( 1.0f, 1.0f, 1.0f, 0.6f );
			glBindTexture( GL_TEXTURE_2D, d_textureNames[TEXTURE_GROUND] );
		}

		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		drawFloor( d_textureNames[TEXTURE_GROUND] );
		glDisable( GL_BLEND );
		glEnable( GL_CULL_FACE );
	}

	glPopMatrix ();
}

int GlWindow :: loadSprite( const char *filename, bool centering )
{
	g_spriteModel.FreeSprite ();
	if( g_spriteModel.LoadSprite( filename ))
	{
		char str[256], basename[64];

		if( centering )
			g_spriteModel.centerView (false);
		g_viewerSettings.speedScale = 1.0f;
		mx_setcwd( mx_getpath( filename ));

		COM_FileBase( filename, basename );
		Q_snprintf( str, sizeof( str ), "%s - %s.spr", g_appTitle, basename );
		g_SPRViewer->setLabel( str );

		ListDirectory();
		return 1;
	}
	return 0;
}

int GlWindow :: loadTextureImage( mxImage *image, int name )
{
	if( image )
	{
		d_textureNames[name] = name;
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );

		if( image->bpp == 8 )
		{
			g_spriteModel.UploadTexture( (byte *)image->data, image->width, image->height, (byte *)image->palette, name );
		}
		else if( image->bpp == 24 )
		{
			glBindTexture( GL_TEXTURE_2D, d_textureNames[name] );
			glHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
			glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
			glTexImage2D( GL_TEXTURE_2D, 0, 3, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data );
			glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

			const char *extensions = (const char *)glGetString( GL_EXTENSIONS );

			// check for anisotropy support
			if( Q_strstr( extensions, "GL_EXT_texture_filter_anisotropic" ))
			{
				float	anisotropy = 1.0f;
				glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy );
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy );
			}
		}
		else if( image->bpp == 32 )
		{
			glBindTexture( GL_TEXTURE_2D, d_textureNames[name] );
			glHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
			glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
			glTexImage2D( GL_TEXTURE_2D, 0, 4, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data );
			glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

			const char *extensions = (const char *)glGetString( GL_EXTENSIONS );

			// check for anisotropy support
			if( Q_strstr( extensions, "GL_EXT_texture_filter_anisotropic" ))
			{
				float	anisotropy = 1.0f;
				glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy );
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy );
			}
		}

		delete image;

		return name;
	}

	return TEXTURE_UNUSED;
}

int GlWindow :: loadTexture( const char *filename, int name )
{
	if( !filename || !Q_strlen( filename ))
	{
		if( d_textureNames[name] )
		{
			glDeleteTextures( 1, (const GLuint *)&d_textureNames[name] );
			d_textureNames[name] = TEXTURE_UNUSED;

			if( name == TEXTURE_BACKGROUND )
				strcpy( g_viewerSettings.backgroundTexFile, "" );
			else if( name == TEXTURE_GROUND )
				strcpy( g_viewerSettings.groundTexFile, "" );
		}
		return TEXTURE_UNUSED;
	}

	mxImage *image = NULL;
	char ext[16];

	strcpy( ext, mx_getextension( filename ));

	if( !mx_strcasecmp( ext, ".tga" ))
		image = mxTgaRead( filename );
	else if( !mx_strcasecmp( ext, ".pcx" ))
		image = mxPcxRead( filename );
	else if( !mx_strcasecmp( ext, ".bmp" ))
		image = mxBmpRead( filename );

	if( image )
	{
		if( name == TEXTURE_BACKGROUND )
			strcpy( g_viewerSettings.backgroundTexFile, filename );
		else if( name == TEXTURE_GROUND )
			strcpy( g_viewerSettings.groundTexFile, filename );
	}

	return loadTextureImage( image, name );
}

void GlWindow :: setRenderMode( int mode )
{
	g_viewerSettings.renderMode = mode;
}

void GlWindow :: setOrientType( int mode )
{
	g_viewerSettings.orientType = mode;
}

void GlWindow::setFrameInfo (int startFrame, int endFrame)
{
	msprite_t *phdr = g_spriteModel.getSpriteHeader();

	if (phdr)
	{
		d_startFrame = startFrame;
		d_endFrame = endFrame;

		if (d_startFrame >= phdr->numframes)
			d_startFrame = phdr->numframes - 1;
		else if (d_startFrame < 0)
			d_startFrame = 0;

		if (d_endFrame >= phdr->numframes)
			d_endFrame = phdr->numframes - 1;
		else if (d_endFrame < 0)
			d_endFrame = 0;

		d_currFrame = d_startFrame;

		if (d_currFrame >= phdr->numframes)
			d_currFrame = phdr->numframes - 1;
	}
	else
	{
		d_startFrame = d_endFrame = d_currFrame = 0;
	}

	d_pol = 0;
}