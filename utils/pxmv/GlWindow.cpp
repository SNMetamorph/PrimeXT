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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "GlWindow.h"
#include "StudioModel.h"
#include "ViewerSettings.h"
#include "ControlPanel.h"
#include "stringlib.h"
#include "cmdlib.h"
#include "mdlviewer.h"
#include "app_info.h"
#include "build.h"
#include "muzzle1.h"
#include "muzzle2.h"
#include "muzzle3.h"
#include "imagelib.h"

extern bool g_bStopPlaying;
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;
extern bool bUseParanoiaFOV;

GlWindow *g_GlWindow = 0;

GlWindow :: GlWindow( mxWindow *parent, ViewerSettings &settings, StudioModel &model, int x, int y, int w, int h, const char *label, int style ) : 
	mxGlWindow( parent, x, y, w, h, label, style ),
	m_studioModel( model ),
	m_settings( settings )
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
			Sys_Sleep( Q_max( 10 - dt * 1000.0, 0 ) );
			return 1;
		}
#endif
		m_studioModel.updateTimings( curr, dt );

		rand (); // keep the random time dependent

		if( !g_bStopPlaying && !m_settings.pause && prev != 0.0 )
		{
			if( !m_studioModel.AdvanceFrame( dt * m_settings.speedScale ))
				d_cpl->resetPlayingSequence(); // hit the end of sequence
		}

		if( !m_settings.pause )
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
		m_settings.pause = false;
	}
	break;

	case mxEvent::MouseDown:
	{
		oldrx = m_settings.rot[0];
		oldry = m_settings.rot[1];
		oldtx = m_settings.trans[0];
		oldty = m_settings.trans[1];
		oldtz = m_settings.trans[2];
		oldlx = m_settings.gLightVec[0];
		oldly = m_settings.gLightVec[1];
		oldx = event->x;
		oldy = event->y;
		m_settings.pause = true;

#if XASH_WIN32
		// HACKHACK: reset focus to main window to catch hot-keys again
		if( g_MDLViewer ) {
			SetFocus( (HWND) g_MDLViewer->getHandle ());
		}
#endif
		return 1;
	}
	break;

	case mxEvent::MouseDrag:
	{
		if( m_settings.showTexture )
		{
			redraw ();
			return 1;
		}
		if( event->buttons & mxEvent::MouseLeftButton )
		{
			if( event->modifiers & mxEvent::KeyShift )
			{
				m_settings.trans[0] = oldtx - (float)(event->x - oldx) * m_settings.movementScale;
				m_settings.trans[1] = oldty + (float)(event->y - oldy) * m_settings.movementScale;
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

				m_settings.gLightVec[0] = (cp*cy);
				m_settings.gLightVec[1] = (-sy);
				m_settings.gLightVec[2] = (sp*cy);
			}
			else
			{
				m_settings.rot[0] = oldrx + (float)(event->y - oldy);
				m_settings.rot[1] = oldry + (float)(event->x - oldx);
			}
		}
		else if( event->buttons & mxEvent::MouseRightButton )
		{
			m_settings.trans[2] = oldtz + (float)(event->y - oldy) * m_settings.movementScale;
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

		m_studioModel.GetMovement( m_studioModel.m_prevGroundCycle, deltaPos, deltaAngles );

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
	switch( m_settings.renderMode )
	{
	case ViewerSettings::RM_WIREFRAME:
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );
		break;
	case ViewerSettings::RM_FLATSHADED:
	case ViewerSettings::RM_SMOOTHSHADED:
	case ViewerSettings::RM_NORMALS:		
	case ViewerSettings::RM_BONEWEIGHTS:
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glDisable( GL_TEXTURE_2D );
		glEnable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );

		if( m_settings.renderMode == ViewerSettings::RM_FLATSHADED )
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
	glClearColor( m_settings.bgColor[0], m_settings.bgColor[1], m_settings.bgColor[2], 0.0f );

	if( m_settings.useStencil )
		glClear (GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT );
	else glClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );

	glViewport( 0, 0, w2(), h2() );
	glDisable( GL_MULTISAMPLE );

	// remap textures if changed
	m_studioModel.RemapTextures();

	//
	// show textures
	//
	if( m_settings.showTexture )
	{
		m_projection.CreateOrtho( 0.0f, w2(), h2(), 0.0f, 1.0f, -1.0f );
		SetProjectionMatrix( m_projection );
		glDisable( GL_MULTISAMPLE );

		studiohdr_t *hdr = m_studioModel.getTextureHeader();

		if( hdr )
		{
			mstudiotexture_t *ptextures = (mstudiotexture_t *)((byte *)hdr + hdr->textureindex);
			float w = (float) ptextures[m_settings.texture].width * m_settings.textureScale;
			float h = (float) ptextures[m_settings.texture].height * m_settings.textureScale;

			ResetModelviewMatrix();

			glDisable( GL_CULL_FACE );
			glDisable( GL_BLEND );

			if( ptextures[m_settings.texture].flags & STUDIO_NF_MASKED )
			{
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_GREATER, 0.25f );
			}
			else glDisable( GL_ALPHA_TEST );

			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			float x = ((float)w2 () - w) / 2;
			float y = ((float)h2 () - h) / 2;

			if(( m_settings.show_uv_map || m_settings.pending_export_uvmap ) && !m_settings.overlay_uv_map )
			{
				glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
				glDisable (GL_TEXTURE_2D);
			}
			else
			{
				glEnable (GL_TEXTURE_2D);
				glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
				glBindTexture (GL_TEXTURE_2D, TEXTURE_COUNT + m_settings.texture );
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

			if( m_settings.show_uv_map || m_settings.pending_export_uvmap || m_settings.overlay_uv_map )
			{
				if( m_settings.anti_alias_lines )
				{
					glEnable( GL_LINE_SMOOTH );
					glEnable( GL_POLYGON_SMOOTH );
					glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
					glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}

				m_studioModel.SetOffset2D( x, y );
				m_studioModel.DrawModelUVMap();

				if( m_settings.anti_alias_lines )
				{
					glDisable( GL_LINE_SMOOTH );
					glDisable(GL_POLYGON_SMOOTH);
				}
                              }

			if( m_settings.pending_export_uvmap && m_settings.uvmapPath[0] )
			{
				mxImage *image = (mxImage *)Mem_Alloc(sizeof(mxImage));
				new (image) mxImage();

				if( image->create((int)w, (int)h, 24 ))
				{
					glReadBuffer( GL_BACK );
					glReadPixels((int)x-1, (int)y, (int)w, (int)h, GL_RGB, GL_UNSIGNED_BYTE, image->data );

					image->flip_vertical();

					if( !mxBmpWrite( m_settings.uvmapPath.c_str(), image))
						mxMessageBox( this, "Error writing .BMP texture.", APP_TITLE_STR, MX_MB_OK|MX_MB_ERROR );
				}

				// cleanup
				m_settings.uvmapPath.clear();
				m_settings.pending_export_uvmap = false;
				Mem_Free(image);
			}

			glClear( GL_DEPTH_BUFFER_BIT );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		return;
	}

	//
	// draw background
	//
	if( m_settings.showBackground && d_textureNames[TEXTURE_BACKGROUND] && !m_settings.showTexture )
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
		m_modelview.ConcatTranslate( -m_settings.trans[0], -m_settings.trans[1], -m_settings.trans[2] );
		m_modelview.ConcatRotate( m_settings.rot[0], 1, 0, 0 );
		m_modelview.ConcatRotate( m_settings.rot[1], 0, 0, 1 );
		vectors = matrix3x3( Vector( 180.0f - m_settings.rot[0], 270.0f - m_settings.rot[1], 0.0f ));
	}

	SetModelviewMatrix( m_modelview );

	// setup stencil buffer
	if( m_settings.useStencil && !bUseWeaponOrigin )
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

	if( m_settings.mirror && !bUseWeaponOrigin )
	{
		const GLdouble flClipPlane[] = { 0.0, 0.0, -1.0, 0.0 };

		setupRenderMode();
		glCullFace( GL_BACK );
		glEnable( GL_CLIP_PLANE0 );
		glClipPlane( GL_CLIP_PLANE0, flClipPlane );
		m_studioModel.DrawModel( true );
		glDisable( GL_CLIP_PLANE0 );
	}

	m_settings.drawn_polys = 0;

	if( m_settings.useStencil )
		glDisable( GL_STENCIL_TEST );

	setupRenderMode();

	if( bUseWeaponOrigin && bUseWeaponLeftHand ) 
		glCullFace( GL_BACK );
	else glCullFace( GL_FRONT );

	m_studioModel.DrawModel();

	//
	// draw ground
	//
	if( m_settings.showGround && !bUseWeaponOrigin )
	{
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_CULL_FACE );

		if( m_settings.useStencil )
			glFrontFace( GL_CW );
		else glDisable( GL_CULL_FACE );

		glEnable( GL_BLEND );

		if( !d_textureNames[TEXTURE_GROUND] )
		{
			glDisable( GL_TEXTURE_2D );
			glColor4f( m_settings.gColor[0], m_settings.gColor[1], m_settings.gColor[2], 0.7f );
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

		if( m_settings.useStencil )
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

	m_studioModel.incrementFramecounter();
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

			m_studioModel.UploadTexture( &texture, (byte *)image->data, (byte *)image->palette, name );
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

		Mem_Free(image);
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
				m_settings.backgroundTexFile = "";
			else if( name == TEXTURE_GROUND )
				m_settings.groundTexFile = "";
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
			m_settings.backgroundTexFile = filename;
		else if( name == TEXTURE_GROUND )
			m_settings.groundTexFile = filename;
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

	image = readBmpFromBuffer( buffer, size );

	return loadTextureImage( image, name );
}

void GlWindow :: dumpViewport( const char *filename )
{
	redraw();
	int w = w2();
	int h = h2();

	mxImage *image = (mxImage *)Mem_Alloc(sizeof(mxImage));
	new (image) mxImage();

	if (image && image->create( w, h, 24 ))
	{
		glReadBuffer( GL_FRONT );
		glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, image->data );

		image->flip_vertical();

		if (!mxBmpWrite( filename, image ))
			mxMessageBox( this, "Error writing screenshot.", APP_TITLE_STR, MX_MB_OK|MX_MB_ERROR );

		Mem_Free(image);
	}
}

mxImage *GlWindow::readBmpFromBuffer(const byte * buffer, size_t size)
{
	int i;
	mxBitmapFileHeader bmfh;
	mxBitmapInfoHeader bmih;
	mxBitmapRGBQuad rgrgbPalette[256];
	int columns, column, rows, row, bpp = 1;
	int cbBmpBits, padSize = 0, bps = 0;
	const byte *bufstart, *bufend;
	byte *pbBmpBits;
	byte *pb, *pbHold;
	size_t cbPalBytes = 0;
	int biTrueWidth;
	mxImage *image = 0;

	// Buffer exists?
	if (!buffer || size <= 0)
		return 0;

	bufstart = buffer;
	bufend = bufstart + size;

	// Read file header
	memcpy(&bmfh, buffer, sizeof bmfh);
	buffer += sizeof(bmfh);

	// Bogus file header check
	if (!(bmfh.bfReserved1 == 0 && bmfh.bfReserved2 == 0))
		goto GetOut;

	// Read info header
	memcpy(&bmih, buffer, sizeof bmih);
	buffer += sizeof(bmih);

	// Bogus info header check
	if (!(bmih.biSize == sizeof bmih && bmih.biPlanes == 1))
		goto GetOut;

	if (memcmp((void *)&bmfh.bfType, "BM", 2))
		goto GetOut;

	// Bogus bit depth?
	if (bmih.biBitCount < 8)
		goto GetOut;

	// Bogus compression?  Only non-compressed supported.
	if (bmih.biCompression != 0) //BI_RGB)
		goto GetOut;

	if (bmih.biBitCount == 8)
	{
		// Figure out how many entires are actually in the table
		if (bmih.biClrUsed == 0)
		{
			cbPalBytes = static_cast<size_t>(pow(2, bmih.biBitCount)) * sizeof(mxBitmapRGBQuad);
			bmih.biClrUsed = 256;
		}
		else
		{
			cbPalBytes = bmih.biClrUsed * sizeof(mxBitmapRGBQuad);
		}

		// Read palette (bmih.biClrUsed entries)
		memcpy(rgrgbPalette, buffer, cbPalBytes);
		buffer += cbPalBytes;
	}

	image = (mxImage *)Mem_Alloc(sizeof(mxImage));
	new (image) mxImage();

	if (!image) 
		goto GetOut;

	if (!image->create(bmih.biWidth, abs(bmih.biHeight), bmih.biBitCount))
	{
		Mem_Free(image);
		goto GetOut;
	}

	if (bmih.biBitCount <= 8)
	{
		pb = (byte *)image->palette;

		// Copy over used entries
		for (i = 0; i < (int)bmih.biClrUsed; i++)
		{
			*pb++ = rgrgbPalette[i].rgbRed;
			*pb++ = rgrgbPalette[i].rgbGreen;
			*pb++ = rgrgbPalette[i].rgbBlue;
		}

		// Fill in unused entires will 0,0,0
		for (i = bmih.biClrUsed; i < 256; i++)
		{
			*pb++ = 0;
			*pb++ = 0;
			*pb++ = 0;
		}
	}
	else
	{
		if (bmih.biBitCount == 24)
			bpp = 3;
		else if (bmih.biBitCount == 32)
			bpp = 4;
	}

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - (buffer - bufstart);

	pbHold = pb = (byte *)Mem_Alloc(cbBmpBits * sizeof(byte));
	if (pb == 0)
	{
		Mem_Free(image);
		goto GetOut;
	}

	memcpy(pb, buffer, cbBmpBits);
	buffer += cbBmpBits;

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bmih.biWidth + 3) & ~3;
	bps = bmih.biWidth * (bmih.biBitCount >> 3);

	columns = bmih.biWidth;
	rows = abs(bmih.biHeight);

	switch (bmih.biBitCount)
	{
		case 8:
		case 24:
			padSize = (4 - (bps % 4)) % 4;
			break;
	}

	for (row = rows - 1; row >= 0; row--)
	{
		pbBmpBits = (byte *)image->data + (row * columns * bpp);

		for (column = 0; column < columns; column++)
		{
			byte	red, green, blue, alpha;
			int	palIndex;

			switch (bmih.biBitCount)
			{
				case 8:
					palIndex = *pb++;
					*pbBmpBits++ = palIndex;
					break;
				case 24:
					blue = *pb++;
					green = *pb++;
					red = *pb++;
					*pbBmpBits++ = red;
					*pbBmpBits++ = green;
					*pbBmpBits++ = blue;
					break;
				case 32:
					blue = *pb++;
					green = *pb++;
					red = *pb++;
					alpha = *pb++;
					*pbBmpBits++ = red;
					*pbBmpBits++ = green;
					*pbBmpBits++ = blue;
					*pbBmpBits++ = alpha;
					break;
				default:
					Mem_Free(pbHold);
					Mem_Free(image);
					goto GetOut;
			}
		}

		pb += padSize;	// actual only for 4-bit bmps
	}

	Mem_Free(pbHold);
GetOut:
	return image;
}

bool ImageLib_Alloc(mxImage *image, int w, int h, int pixelSize) 
{
	if (image->data)
		Mem_Free(image->data);

	if (image->palette)
		Mem_Free(image->palette);

	image->data = Mem_Alloc(w * h * pixelSize / 8);
	if (!image->data)
		return false;

	// allocate a palette for 8-bit images
	if (pixelSize == 8)
	{
		image->palette = Mem_Alloc(3 * 256);
		if (!image->palette)
		{
			//delete[] data;
			Mem_Free(image->data);
			return false;
		}
	}
	else {
		image->palette = nullptr;
	}

	image->width = w;
	image->height = h;
	image->bpp = pixelSize;
	return true;
};

mxImage *GlWindow::readTextureFromFile(const char *filename)
{
	mxImage *image = nullptr;
	rgbdata_t *imageData = nullptr;

	imageData = COM_LoadImage(filename, true);
	if (!imageData)
	{
		return nullptr; // File didn't exist, or can't be reached
	}

	imageData = Image_Quantize(imageData);
	if (!(imageData->flags & IMAGE_QUANTIZED))
	{
		return nullptr; // Quantizer failed
	}

	image = (mxImage *)Mem_Alloc(sizeof(mxImage));
	new (image) mxImage();

	if (!image)
	{
		return nullptr; // image can't be allocated for some reason
	}

	if (!ImageLib_Alloc(image, imageData->width, imageData->height, 8))
	{
		imageFree(image);
		return nullptr;
	}

	byte *paletteBuffer = (byte *) image->palette;

	// Copy over used entries
	for (int i = 0; i < 256; i++)
	{
		*paletteBuffer++ = imageData->palette[i * 4 + 0];
		*paletteBuffer++ = imageData->palette[i * 4 + 1];
		*paletteBuffer++ = imageData->palette[i * 4 + 2];
	}

	memcpy(image->data, imageData->buffer, imageData->width * imageData->height);
	Image_Free(imageData);

	return image;
}

void GlWindow::imageFree(mxImage *image)
{
	if (image->data)
		Mem_Free(image->data);
	//delete[] data;

	if (image->palette)
		Mem_Free(image->palette);
	//delete[] palette;

	Mem_Free(image);
}
