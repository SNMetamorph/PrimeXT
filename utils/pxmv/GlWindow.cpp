//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           GlWindow.cpp
// last modified:  May 04 1999, Mete Ciragan
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
#include <string.h>
#include "GlWindow.h"
#include "StudioModel.h"
#include "ViewerSettings.h"
#include "ControlPanel.h"
#include "stringlib.h"
#include "mdlviewer.h"
#include "muzzle1.h"
#include "muzzle2.h"
#include "muzzle3.h"

extern char g_appTitle[];
extern bool g_bStopPlaying;
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;
extern bool bUseParanoiaFOV;

GlWindow *g_GlWindow = 0;

GlWindow :: GlWindow( mxWindow *parent, int x, int y, int w, int h, const char *label, int style ) : mxGlWindow( parent, x, y, w, h, label, style )
{
	glDepthFunc( GL_LEQUAL );

	if( !parent ) setVisible( true );
	else mx :: setIdleWindow ( this );

	// load muzzle flahses
	loadTextureBuffer( muzzleflash1_bmp, sizeof( muzzleflash1_bmp ), TEXTURE_MUZZLEFLASH1 );
	loadTextureBuffer( muzzleflash2_bmp, sizeof( muzzleflash2_bmp ), TEXTURE_MUZZLEFLASH2 );
	loadTextureBuffer( muzzleflash3_bmp, sizeof( muzzleflash3_bmp ), TEXTURE_MUZZLEFLASH3 );
}

GlWindow :: ~GlWindow( void )
{
	mx::setIdleWindow( 0 );
	loadTexture( NULL, TEXTURE_GROUND );
	loadTexture( NULL, TEXTURE_BACKGROUND );
	loadTexture( NULL, TEXTURE_MUZZLEFLASH1 );
	loadTexture( NULL, TEXTURE_MUZZLEFLASH2 );
	loadTexture( NULL, TEXTURE_MUZZLEFLASH3 );
}

int GlWindow :: handleEvent( mxEvent *event )
{
	static float oldrx, oldry, oldtz, oldtx, oldty;
	static float oldlx, oldly;
	static int oldx, oldy;
	static double lastupdate;

	switch( event->event )
	{

	case mxEvent::Idle:
	{
		static double prev;
		double curr = (double) mx::getTickCount () / 1000.0;
		double dt = (curr - prev);
#if 1
		// clamp to 100fps
		if( dt >= 0.0 && dt < 0.01 )
		{
			Sleep( Q_max( 10 - dt * 1000.0, 0 ) );
			return 1;
		}
#endif
		g_studioModel.updateTimings( curr, dt );

		rand (); // keep the random time dependent

		if( !g_bStopPlaying && !g_viewerSettings.pause && prev != 0.0 )
		{
			if( !g_studioModel.AdvanceFrame( dt * g_viewerSettings.speedScale ))
				d_cpl->resetPlayingSequence(); // hit the end of sequence
		}

		if( !g_viewerSettings.pause )
			redraw();

		// update counter every 0.2 secs
		if(( curr - lastupdate ) > 0.2 )
		{ 
			g_ControlPanel->updateDrawnPolys();
			lastupdate = curr;
		}

		prev = curr;

		return 1;
	}
	break;

	case mxEvent::MouseUp:
	{
		g_viewerSettings.pause = false;
	}
	break;

	case mxEvent::MouseDown:
	{
		oldrx = g_viewerSettings.rot[0];
		oldry = g_viewerSettings.rot[1];
		oldtx = g_viewerSettings.trans[0];
		oldty = g_viewerSettings.trans[1];
		oldtz = g_viewerSettings.trans[2];
		oldlx = g_viewerSettings.gLightVec[0];
		oldly = g_viewerSettings.gLightVec[1];
		oldx = event->x;
		oldy = event->y;

		// HACKHACK: reset focus to main window to catch hot-keys again
		if( g_MDLViewer ) SetFocus( (HWND) g_MDLViewer->getHandle ());
		g_viewerSettings.pause = true;

		return 1;
	}
	break;

	case mxEvent::MouseDrag:
	{
		if( g_viewerSettings.showTexture )
		{
			redraw ();
			return 1;
		}
		if( event->buttons & mxEvent::MouseLeftButton )
		{
			if( event->modifiers & mxEvent::KeyShift )
			{
				g_viewerSettings.trans[0] = oldtx - (float)(event->x - oldx) * g_viewerSettings.movementScale;
				g_viewerSettings.trans[1] = oldty + (float)(event->y - oldy) * g_viewerSettings.movementScale;
			}
			else if( event->modifiers & mxEvent::KeyCtrl )
			{
				float yaw = oldlx + (float)(event->x - oldx);
				float pitch = oldly + (float)(event->y - oldy);
				float sy, cy, sp, cp;

				pitch = DEG2RAD( anglemod( pitch * 0.6f ));
				yaw = DEG2RAD( anglemod( yaw * 0.6f ));
				SinCos( yaw, &sy, &cy );
				SinCos( pitch, &sp, &cp );

				g_viewerSettings.gLightVec[0] = (cp*cy);
				g_viewerSettings.gLightVec[1] = (-sy);
				g_viewerSettings.gLightVec[2] = (sp*cy);
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

		return 1;
	}
	break;
	} // switch (event->event)

	return 1;
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

		Vector deltaPos;
		Vector deltaAngles;

		g_studioModel.GetMovement( g_studioModel.m_prevGroundCycle, deltaPos, deltaAngles );

		float dpdd = scale / dist;

		tMap.x = tMap.x + dxMap.x * deltaPos.x * dpdd + dxMap.y * deltaPos.y * dpdd;	
		tMap.y = tMap.y + dyMap.x * deltaPos.x * dpdd + dyMap.y * deltaPos.y * dpdd;

		while (tMap.x < 0.0) tMap.x +=  1.0f;
		while (tMap.x > 1.0) tMap.x += -1.0f;
		while (tMap.y < 0.0) tMap.y +=  1.0f;
		while (tMap.y > 1.0) tMap.y += -1.0f;

		dxMap = VectorYawRotate( dxMap, -deltaAngles.y );
		dyMap = VectorYawRotate( dyMap, -deltaAngles.y );

		glBegin( GL_QUADS );
			glTexCoord2f( tMap.x - (dxMap.x - dyMap.x) * scale, tMap.y - (-dxMap.y + dyMap.y) * scale );
			glVertex3f( -dist, -dist, 0 );

			glTexCoord2f( tMap.x + (dxMap.x + dyMap.x) * scale, tMap.y + (-dxMap.y - dyMap.y) * scale );
			glVertex3f( dist, -dist, 0 );

			glTexCoord2f( tMap.x + (dxMap.x - dyMap.x) * scale, tMap.y + (-dxMap.y + dyMap.y) * scale );
			glVertex3f( dist, dist, 0 );

			glTexCoord2f( tMap.x + (-dxMap.x - dyMap.x) * scale, tMap.y + (dxMap.y + dyMap.y) * scale );
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

	// restore original modelview
	SetModelviewMatrix( m_modelview );
	glDisable( GL_MULTISAMPLE );
}

void GlWindow :: setupRenderMode( void )
{
	switch( g_viewerSettings.renderMode )
	{
	case RM_WIREFRAME:
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );
		break;
	case RM_FLATSHADED:
	case RM_SMOOTHSHADED:
	case RM_BONEWEIGHTS:
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glDisable( GL_TEXTURE_2D );
		glEnable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );

		if( g_viewerSettings.renderMode == RM_FLATSHADED )
			glShadeModel( GL_FLAT );
		else glShadeModel( GL_SMOOTH );
		break;
	default:
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glEnable( GL_TEXTURE_2D );
		glEnable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );
		glShadeModel( GL_SMOOTH );
		break;
	}
}

void GlWindow :: GluPerspective( float fov_y )
{
	GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;
	float	aspect = (float)w() / h();

	zFar = 131072.0f;	// don't cull giantic models (e.g. skybox models)
	zNear = bUseWeaponOrigin ? 4.0f : 0.1f;	// ammo shell issues

	if( bUseWeaponOrigin && bUseParanoiaFOV )
	{
		float fovX = 60.0f;	// HL2 uses 54 as default
		float fovY = V_CalcFov( fovX, w(), h() );

		yMax = zNear * tan( fovY * M_PI / 360.0 );
		yMin = -yMax;

		xMax = zNear * tan( fovX * M_PI / 360.0 );
		xMin = -xMax;
	}
	else
	{
		yMax = zNear * tan( fov_y * M_PI / 360.0 );
		yMin = -yMax;

		xMin = yMin * aspect;
		xMax = yMax * aspect;
	}

	m_projection.CreateProjection( xMax, xMin, yMax, yMin, zNear, zFar );
	SetProjectionMatrix( m_projection );
}

void GlWindow :: SetProjectionMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( dest );
}

void GlWindow :: SetModelviewMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( dest );
}

void GlWindow :: ResetModelviewMatrix( void )
{
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
}

void GlWindow :: draw( void )
{
	glClearColor( g_viewerSettings.bgColor[0], g_viewerSettings.bgColor[1], g_viewerSettings.bgColor[2], 0.0f );

	if( g_viewerSettings.useStencil )
		glClear (GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT );
	else glClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );

	glViewport( 0, 0, w2(), h2() );
	glDisable( GL_MULTISAMPLE );

	// remap textures if changed
	g_studioModel.RemapTextures();

	//
	// show textures
	//
	if( g_viewerSettings.showTexture )
	{
		m_projection.CreateOrtho( 0.0f, w2(), h2(), 0.0f, 1.0f, -1.0f );
		SetProjectionMatrix( m_projection );
		glDisable( GL_MULTISAMPLE );

		studiohdr_t *hdr = g_studioModel.getTextureHeader();

		if( hdr )
		{
			mstudiotexture_t *ptextures = (mstudiotexture_t *)((byte *)hdr + hdr->textureindex);
			float w = (float) ptextures[g_viewerSettings.texture].width * g_viewerSettings.textureScale;
			float h = (float) ptextures[g_viewerSettings.texture].height * g_viewerSettings.textureScale;

			ResetModelviewMatrix();

			glDisable( GL_CULL_FACE );
			glDisable( GL_BLEND );

			if( ptextures[g_viewerSettings.texture].flags & STUDIO_NF_MASKED )
			{
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_GREATER, 0.25f );
			}
			else glDisable( GL_ALPHA_TEST );

			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			float x = ((float)w2 () - w) / 2;
			float y = ((float)h2 () - h) / 2;

			if(( g_viewerSettings.show_uv_map || g_viewerSettings.pending_export_uvmap ) && !g_viewerSettings.overlay_uv_map )
			{
				glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
				glDisable (GL_TEXTURE_2D);
			}
			else
			{
				glEnable (GL_TEXTURE_2D);
				glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
				glBindTexture (GL_TEXTURE_2D, TEXTURE_COUNT + g_viewerSettings.texture );
			}

			glBegin( GL_TRIANGLE_STRIP );
				glTexCoord2f( 0, 0 );
				glVertex2f( x, y );
				glTexCoord2f( 1, 0 );
				glVertex2f( x + w, y );
				glTexCoord2f( 0, 1 );
				glVertex2f( x, y + h );
				glTexCoord2f( 1, 1 );
				glVertex2f( x + w, y + h );
			glEnd();

			if( g_viewerSettings.show_uv_map || g_viewerSettings.pending_export_uvmap || g_viewerSettings.overlay_uv_map )
			{
				if( g_viewerSettings.anti_alias_lines )
				{
					glEnable( GL_LINE_SMOOTH );
					glEnable( GL_POLYGON_SMOOTH );
					glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
					glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}

				g_studioModel.SetOffset2D( x, y );
				g_studioModel.DrawModelUVMap();

				if( g_viewerSettings.anti_alias_lines )
				{
					glDisable( GL_LINE_SMOOTH );
					glDisable(GL_POLYGON_SMOOTH);
				}
                              }

			if( g_viewerSettings.pending_export_uvmap && g_viewerSettings.uvmapPath[0] )
			{
				mxImage *image = new mxImage ();

				if( image->create((int)w, (int)h, 24 ))
				{
					glReadBuffer( GL_BACK );
					glReadPixels((int)x-1, (int)y, (int)w, (int)h, GL_RGB, GL_UNSIGNED_BYTE, image->data );

					image->flip_vertical();

					if( !mxBmpWrite( g_viewerSettings.uvmapPath, image ))
						mxMessageBox( this, "Error writing .BMP texture.", g_appTitle, MX_MB_OK|MX_MB_ERROR );
				}

				// cleanup
				memset( g_viewerSettings.uvmapPath, 0, sizeof( g_viewerSettings.uvmapPath ));
				g_viewerSettings.pending_export_uvmap = false;
				delete image;
			}

			glClear( GL_DEPTH_BUFFER_BIT );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		return;
	}

	//
	// draw background
	//
	if( g_viewerSettings.showBackground && d_textureNames[TEXTURE_BACKGROUND] && !g_viewerSettings.showTexture )
	{
		m_projection.CreateOrtho( 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f );
		SetProjectionMatrix( m_projection );

		ResetModelviewMatrix();

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
	}

	// calc viewer FOV
	float fov_x = (bUseWeaponOrigin) ? 68.0f : 65.0f; // same as original
	float fov_y = V_CalcFov( fov_x, w(), h() );

	// NOTE: GluPerspective receives fov_y as input but original code sent fov_x
	// this is completely wrong but i'm leave it for backward compatibility
	GluPerspective( fov_x );

	if( bUseWeaponOrigin )
	{
		m_modelview.CreateModelview(); // init quake world orientation
		m_modelview.ConcatTranslate( 1.0f, 0.0f, -1.0f );	// shift back like in HL
		vectors = matrix3x3( Vector( 90.0f, 0.0f, 0.0f ));
	}
	else
	{
		m_modelview.Identity();
		m_modelview.ConcatTranslate( -g_viewerSettings.trans[0], -g_viewerSettings.trans[1], -g_viewerSettings.trans[2] );
		m_modelview.ConcatRotate( g_viewerSettings.rot[0], 1, 0, 0 );
		m_modelview.ConcatRotate( g_viewerSettings.rot[1], 0, 0, 1 );
		vectors = matrix3x3( Vector( 180.0f - g_viewerSettings.rot[0], 270.0f - g_viewerSettings.rot[1], 0.0f ));
	}

	SetModelviewMatrix( m_modelview );

	// setup stencil buffer
	if( g_viewerSettings.useStencil && !bUseWeaponOrigin )
	{
		// Don't update color or depth.
		glDisable( GL_DEPTH_TEST );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

		// Draw 1 into the stencil buffer.
		glEnable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		glStencilFunc( GL_ALWAYS, 1, 0xffffffff );

		// Now render floor; floor pixels just get their stencil set to 1.
		drawFloor();

		// Re-enable update of color and depth. 
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glEnable( GL_DEPTH_TEST );

		// Now, only render where stencil is set to 1.
		glStencilFunc( GL_EQUAL, 1, 0xffffffff ); // draw if == 1
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	}

	if( g_viewerSettings.mirror && !bUseWeaponOrigin )
	{
		const GLdouble flClipPlane[] = { 0.0, 0.0, -1.0, 0.0 };

		setupRenderMode();
		glCullFace( GL_BACK );
		glEnable( GL_CLIP_PLANE0 );
		glClipPlane( GL_CLIP_PLANE0, flClipPlane );
		g_studioModel.DrawModel( true );
		glDisable( GL_CLIP_PLANE0 );
	}

	g_viewerSettings.drawn_polys = 0;

	if( g_viewerSettings.useStencil )
		glDisable( GL_STENCIL_TEST );

	setupRenderMode();

	if( bUseWeaponOrigin && bUseWeaponLeftHand ) 
		glCullFace( GL_BACK );
	else glCullFace( GL_FRONT );

	g_studioModel.DrawModel();

	//
	// draw ground
	//
	if( g_viewerSettings.showGround && !bUseWeaponOrigin )
	{
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_CULL_FACE );

		if( g_viewerSettings.useStencil )
			glFrontFace( GL_CW );
		else glDisable( GL_CULL_FACE );

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

		if( g_viewerSettings.useStencil )
		{
			glCullFace( GL_BACK );
			glColor4f( 0.1f, 0.1f, 0.1f, 1.0f );
			glBindTexture( GL_TEXTURE_2D, 0 );
			drawFloor();
			glFrontFace( GL_CCW );
		}
		else glEnable( GL_CULL_FACE );
	}

	if( bUseWeaponOrigin )
	{
		float hW = (float)w2() / 2;
		float hH = (float)h2() / 2;
		float flColor = 1.0f;
		int dir = 5;

		m_projection.CreateOrtho( 0.0f, w2(), h2(), 0.0f, 1.0f, -1.0f );
		SetProjectionMatrix( m_projection );

		glDisable( GL_MULTISAMPLE );
		glDisable( GL_TEXTURE_2D );
		ResetModelviewMatrix();

		glColor4f( 1.0f, flColor, flColor, 0.8f );

		glBegin( GL_LINES );
			glVertex3f( hW, hH - dir, 0.0f );
			glVertex3f( hW, hH - ( CROSS_LENGTH * 1.2f ) - dir, 0.0f );
		glEnd();

		glBegin( GL_LINES );
			glVertex3f( hW + dir, hH + dir, 0.0f );
			glVertex3f( hW + CROSS_LENGTH + dir, hH + CROSS_LENGTH + dir, 0.0f );
		glEnd();

		glBegin( GL_LINES );
			glVertex3f( hW - dir, hH + dir, 0.0f );
			glVertex3f( hW - CROSS_LENGTH - dir, hH + CROSS_LENGTH + dir, 0.0f );
		glEnd();

		glEnable( GL_TEXTURE_2D );
	}

	g_studioModel.incrementFramecounter();
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
			mstudiotexture_t texture;
			texture.width = image->width;
			texture.height = image->height;
			texture.flags = 0;

			g_studioModel.UploadTexture( &texture, (byte *)image->data, (byte *)image->palette, name );
		}
		else if( image->bpp == 24 )
		{
			glBindTexture( GL_TEXTURE_2D, d_textureNames[name] );
			glHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
			glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
			glTexImage2D( GL_TEXTURE_2D, 0, 3, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
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
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
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
	if( !filename || !strlen( filename ))
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

int GlWindow :: loadTextureBuffer( const byte *buffer, size_t size, int name )
{
	if( !buffer || size <= 0 )
	{
		if( d_textureNames[name] )
		{
			glDeleteTextures( 1, (const GLuint *)&d_textureNames[name] );
			d_textureNames[name] = TEXTURE_UNUSED;
		}
		return TEXTURE_UNUSED;
	}

	mxImage *image = NULL;

	image = mxBmpReadBuffer( buffer, size );

	return loadTextureImage( image, name );
}

void GlWindow :: dumpViewport( const char *filename )
{
	redraw();
	int w = w2();
	int h = h2();

	mxImage *image = new mxImage ();
	if( image->create( w, h, 24 ))
	{
		glReadBuffer( GL_FRONT );
		glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, image->data );

		image->flip_vertical();

		if( !mxBmpWrite( filename, image ))
			mxMessageBox( this, "Error writing screenshot.", g_appTitle, MX_MB_OK|MX_MB_ERROR );

		delete image;
	}
}