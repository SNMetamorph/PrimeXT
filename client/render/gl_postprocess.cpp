//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "gl_postprocess.h"
#include "hud.h"
#include "utils.h"
#include "const.h"
#include "gl_local.h"
#include "stringlib.h"
#include "gl_world.h"
#include "gl_cvars.h"
#include "gl_debug.h"

static CBasePostEffects	post;
void V_RenderPostEffect(word hProgram);

void CBasePostEffects::InitializeTextures()
{
	InitScreenColor();
	InitScreenDepth();
	InitTargetColor(0);
	InitDepthOfField();
	InitLuminanceTexture();
}

void CBasePostEffects::InitializeShaders()
{
	char options[MAX_OPTIONS_LENGTH];

	// monochrome effect
	monoShader = GL_FindShader("postfx/monochrome", "postfx/generic", "postfx/monochrome");

	// gaussian blur for X
	GL_SetShaderDirective(options, "BLUR_X");
	blurShader[0] = GL_FindShader("postfx/gaussblur", "postfx/generic", "postfx/gaussblur", options);

	// gaussian blur for Y
	GL_SetShaderDirective(options, "BLUR_Y");
	blurShader[1] = GL_FindShader("postfx/gaussblur", "postfx/generic", "postfx/gaussblur", options);

	// DOF with bokeh
	dofShader = GL_FindShader("postfx/dofbokeh", "postfx/generic", "postfx/dofbokeh");

	// prepare sunshafts
	genSunShafts = GL_FindShader("postfx/genshafts", "postfx/generic", "postfx/genshafts");

	// render sunshafts
	drawSunShafts = GL_FindShader("postfx/drawshafts", "postfx/generic", "postfx/drawshafts");

	// tonemapping 
	tonemapShader = GL_FindShader("postfx/tonemap", "postfx/generic", "postfx/tonemap");
	luminanceGenShader = GL_FindShader("postfx/generate_luminance", "postfx/generic", "postfx/generate_luminance");
	luminanceDownscaleShader = GL_FindShader("postfx/downscale_luminance", "postfx/generic", "postfx/downscale_luminance");

	// bloom effect
	blurMipShader = GL_FindShader("postfx/gaussblurmip", "postfx/generic", "postfx/gaussblurmip");
	bloomShader = GL_FindShader("postfx/bloom", "postfx/generic", "postfx/bloom");
}

void CBasePostEffects :: InitScreenColor( void )
{
	int tex_flags = 0;
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);

	if (tr.screen_color)
	{
		FREE_TEXTURE(tr.screen_color);
		tr.screen_color = 0;
	}

	if (hdr_rendering)
		tex_flags = TF_COLORBUFFER | TF_HAS_ALPHA | TF_ARB_16BIT | TF_ARB_FLOAT;
	else
		tex_flags = TF_COLORBUFFER;

	tr.screen_color = CREATE_TEXTURE( "*screencolor", glState.width, glState.height, NULL, tex_flags);
	if (hdr_rendering)
	{
		if (!tr.screencopy_fbo)
			tr.screencopy_fbo = GL_AllocDrawbuffer("*screencopy_fbo", glState.width, glState.height, 1);

		GL_AttachColorTextureToFBO(tr.screencopy_fbo, tr.screen_color, 0);
		GL_CheckFBOStatus(tr.screencopy_fbo);
	}
}

void CBasePostEffects :: InitScreenDepth( void )
{
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	if (tr.screen_depth)
	{
		FREE_TEXTURE(tr.screen_depth);
		tr.screen_depth = 0;
	}

	tr.screen_depth = CREATE_TEXTURE( "*screendepth", glState.width, glState.height, NULL, TF_DEPTHBUFFER ); 
	if (hdr_rendering)
	{
		if (!tr.screencopy_fbo)
			tr.screencopy_fbo = GL_AllocDrawbuffer("*screencopy_fbo", glState.width, glState.height, 1);

		GL_AttachDepthTextureToFBO(tr.screencopy_fbo, tr.screen_depth, 0);
		GL_CheckFBOStatus(tr.screencopy_fbo);
	}

}

void CBasePostEffects :: InitTargetColor( int slot )
{
	int tex_flags = 0;
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);

	if( target_rgb[slot] )
	{
		FREE_TEXTURE( target_rgb[slot] );
		target_rgb[slot] = 0;
	}

	if (hdr_rendering)
		tex_flags = TF_IMAGE | TF_ARB_16BIT | TF_ARB_FLOAT;
	else
		tex_flags = TF_IMAGE;

	target_rgb[slot] = CREATE_TEXTURE(va( "*target%i", slot ), TARGET_SIZE, TARGET_SIZE, NULL, tex_flags);
}

void CBasePostEffects :: RequestScreenColor( void )
{
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	GL_Bind(GL_TEXTURE0, tr.screen_color);
	if (hdr_rendering)
	{
		pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, tr.screencopy_fbo->id);
		pglBlitFramebuffer(0, 0, glState.width, glState.height, 0, 0, glState.width, glState.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer);
	}
	else
	{
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height);
	}
}

void CBasePostEffects :: RequestScreenDepth( void )
{
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	GL_Bind(GL_TEXTURE0, tr.screen_depth);
	if (hdr_rendering)
	{
		pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, tr.screencopy_fbo->id);
		pglBlitFramebuffer(0, 0, glState.width, glState.height, 0, 0, glState.width, glState.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer);
	}
	else
	{
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height);
	}
}

void CBasePostEffects :: RequestTargetCopy( int slot )
{
	GL_Bind(GL_TEXTURE0, target_rgb[slot]);
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, TARGET_SIZE, TARGET_SIZE);
}

void CBasePostEffects::GenerateLuminance()
{
	GL_DebugGroupPush(__FUNCTION__);
	// render luminance to first mip
	pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, avg_luminance_fbo[0]->id);
	V_RenderPostEffect(luminanceGenShader);

	// downscale luminance to other mips
	GL_Bind(GL_TEXTURE0, avg_luminance_texture);
	GL_BindShader(&glsl_programs[luminanceDownscaleShader]);
	const int mipCount = ARRAYSIZE(avg_luminance_fbo);
	for (int a = 2, i = 1; i < mipCount; ++i, a *= 2)
	{
		gl_drawbuffer_t *fbo = avg_luminance_fbo[i];
		int w = Q_max(fbo->width / a, 1);
		int h = Q_max(fbo->height / a, 1);
		
		for (int j = 0; j < RI->currentshader->numUniforms; j++)
		{
			uniform_t *u = &RI->currentshader->uniforms[j];
			switch (u->type)
			{
				case UT_SCREENSIZEINV:
					u->SetValue(1.0f / (float)w, 1.0f / (float)h);
					break;
				case UT_MIPLOD:
					u->SetValue((float)(i - 1));
					break;
				case UT_TEXCOORDCLAMP:
					u->SetValue(0.0f, 0.0f, 1.0f, 1.0f);
					break;
			}
		}

		pglViewport(0, 0, w, h);
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo->id);
		RenderFSQ(glState.width, glState.height);
	}

	// extract average luminance from last mip
	pglBindFramebuffer(GL_READ_FRAMEBUFFER, avg_luminance_fbo[mipCount - 1]->id);
	pglReadPixels(0, 0, 1, 1, GL_RED, GL_FLOAT, &avg_luminance);
	float t = Q_min(1.0f, tr.frametime * 3.0f);
	avg_luminance_interp = avg_luminance_interp * (1.0f - t) + t * avg_luminance;

	// restore GL state
	pglViewport(0, 0, glState.width, glState.height);
	pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer);
	GL_DebugGroupPop();
}

void CBasePostEffects :: SetNormalViewport( void )
{
	pglViewport( RI->glstate.viewport[0], RI->glstate.viewport[1], RI->glstate.viewport[2], RI->glstate.viewport[3] );
}

void CBasePostEffects :: SetTargetViewport( void )
{
	pglViewport( 0, 0, TARGET_SIZE, TARGET_SIZE );
}

void CBasePostEffects :: InitDepthOfField( void )
{

}
 
void CBasePostEffects::InitLuminanceTexture()
{
	const int texWidth = 1280;
	const int texHeight = 720;
	const int mipCount = ARRAYSIZE(avg_luminance_fbo);

	FREE_TEXTURE(avg_luminance_texture);
	avg_luminance_texture = CREATE_TEXTURE("*screen_avg_luminance", texWidth, texHeight, NULL, TF_ARB_16BIT | TF_ARB_FLOAT); // make it TF_LUMINANCE for using just 1 color channel
	GL_Bind(GL_TEXTURE0, avg_luminance_texture);
	pglGenerateMipmap(GL_TEXTURE_2D);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	GL_Bind(GL_TEXTURE0, 0);

	for (int i = 0; i < mipCount; ++i)
	{
		if (!avg_luminance_fbo[i]) {
			avg_luminance_fbo[i] = GL_AllocDrawbuffer("*screen_avg_luminance_fbo", texWidth, texHeight, 1);
		}
		GL_AttachColorTextureToFBO(avg_luminance_fbo[i], avg_luminance_texture, 0, 0, i);
		GL_CheckFBOStatus(avg_luminance_fbo[i]);
	}
}

bool CBasePostEffects :: ProcessDepthOfField( void )
{
	if( !CVAR_TO_BOOL( r_dof ) ) // or no iron sight on weapon
		return false; // disabled or unitialized

	if( m_flDOFStartTime == 0.0f )
		return false; // iron sight disabled

	if( !Begin( )) 
		return false;

	if( m_flDOFStartTime != 0.0f )
	{
		float flDegree = (tr.time - m_flDOFStartTime) / 0.3f;

		if( flDegree >= 1.0f )
		{
			// all done. holds the final value
			m_flLastLength = m_flStartLength + m_flOffsetLength;
			m_flDOFStartTime = 0.0f; // done
		}
		else
		{
			// evaluate focal length
			m_flLastLength = m_flStartLength + m_flOffsetLength * flDegree;
		}
	}

	float zNear = Z_NEAR; // fixed
	float zFar = RI->view.farClip;
	float depthValue = 0.0f;

	// get current depth value
	pglReadPixels( glState.width >> 1, glState.height >> 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthValue );
	depthValue = RemapVal( depthValue, 0.0, 0.8, 0.0, 1.0 );
	depthValue = -zFar * zNear / ( depthValue * ( zFar - zNear ) - zFar ); // linearize it
	float holdTime = bound( 0.01f, r_dof_hold_time->value, 0.5f );
	float changeTime = bound( 0.1f, r_dof_change_time->value, 2.0f );

	if( Q_round( m_flCachedDepth, 10 ) != Q_round( depthValue, 10 ))
	{
		m_flStartTime = 0.0f; // cancelling changes
		m_flDelayTime = tr.time; // make sure what focal point is not changed more than 0.5 secs
		m_flStartDepth = m_flLastDepth; // get last valid depth
		m_flOffsetDepth = depthValue - m_flStartDepth;
		m_flCachedDepth = depthValue;
	}

	if(( tr.time - m_flDelayTime ) > holdTime && m_flStartTime == 0.0f && m_flDelayTime != 0.0f )
	{
		// begin the change depth
		m_flStartTime = tr.time;
	}

	if( m_flStartTime != 0.0f )
	{
		float flDegree = (tr.time - m_flStartTime) / changeTime;

		if( flDegree >= 1.0f )
		{
			// all done. holds the final value
			m_flLastDepth = m_flStartDepth + m_flOffsetDepth;
			m_flStartTime = m_flDelayTime = 0.0f;
		}
		else
		{
			// evaluate focal depth
			m_flLastDepth = m_flStartDepth + m_flOffsetDepth * flDegree;
		}
	}

	return true;
}

bool CBasePostEffects :: ProcessSunShafts( void )
{
	if( !CVAR_TO_BOOL( v_sunshafts ))
		return false;

	if( !FBitSet( world->features, WORLD_HAS_SKYBOX ))
		return false;

	if( tr.sky_normal == g_vecZero )
		return false;

	// update blur params
	blurFactor[0] = 0.15f;
	blurFactor[1] = 0.15f;

	if( tr.sun_light_enabled ) m_vecSunLightColor = tr.sun_diffuse;
	else m_vecSunLightColor = tr.sky_ambient * (1.0f/128.0f) * tr.diffuseFactor;
	Vector sunPos = tr.cached_vieworigin + tr.sky_normal * 1000.0;
	ColorNormalize( m_vecSunLightColor, m_vecSunLightColor );

	Vector ndc, view;

	// project sunpos to screen 
	R_TransformWorldToDevice( sunPos, ndc );
	R_TransformDeviceToScreen( ndc, m_vecSunPosition );
	m_vecSunPosition.z = DotProduct( -tr.sky_normal, GetVForward( ));

	if( m_vecSunPosition.z < 0.01f )
		return false; // fade out

	// convert to screen pixels
	m_vecSunPosition.x = m_vecSunPosition.x / glState.width;
	m_vecSunPosition.y = m_vecSunPosition.y / glState.height;

	return Begin();
}

bool CBasePostEffects :: Begin( void )
{
	// we are in cubemap rendering mode
	if( !RP_NORMALPASS( ))
		return false;

	if( !CVAR_TO_BOOL( v_posteffects ))
		return false;

	GL_Setup2D();

	return true;
}

void CBasePostEffects :: End( void )
{
	GL_CleanUpTextureUnits( 0 );
	GL_BindShader( NULL );
	GL_Setup3D();
}

void InitPostEffects( void )
{
	v_posteffects = CVAR_REGISTER( "gl_posteffects", "1", FCVAR_ARCHIVE );
	v_sunshafts = CVAR_REGISTER( "gl_sunshafts", "1", FCVAR_ARCHIVE );
	v_grayscale = CVAR_REGISTER( "gl_grayscale", "0", 0 );
	r_tonemap = CVAR_REGISTER("r_tonemap", "1", FCVAR_ARCHIVE);
	r_bloom = CVAR_REGISTER("r_bloom", "1", FCVAR_ARCHIVE);
	memset( &post, 0, sizeof( post ));
	post.InitializeShaders();
}

void InitPostTextures( void )
{
	post.InitializeTextures();
}

static float GetGrayscaleFactor( void )
{
	float grayscale = v_grayscale->value;
	float deadTime = 0.0f; // FIXME STUB gHUD.m_flDeadTime
	if (deadTime)
	{
		float fact = (tr.time - deadTime) / DEAD_GRAYSCALE_TIME;

		fact = Q_min( fact, 1.0f );
		grayscale = Q_max( fact, grayscale );
	}

	return grayscale;
}

// rectangle version
void RenderFSQ( void )
{
	float screenWidth = (float)glState.width;
	float screenHeight = (float)glState.height;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, screenHeight );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( screenWidth, screenHeight );
		pglVertex2f( screenWidth, 0.0f );
		pglTexCoord2f( screenWidth, 0.0f );
		pglVertex2f( screenWidth, screenHeight );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex2f( 0.0f, screenHeight );
	pglEnd();
}

void RenderFSQ( int wide, int tall )
{
	float screenWidth = (float)wide;
	float screenHeight = (float)tall;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglNormal3fv( tr.screen_normals[0] );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglNormal3fv( tr.screen_normals[1] );
		pglVertex2f( screenWidth, 0.0f );
		pglTexCoord2f( 1.0f, 0.0f );
		pglNormal3fv( tr.screen_normals[2] );
		pglVertex2f( screenWidth, screenHeight );
		pglTexCoord2f( 0.0f, 0.0f );
		pglNormal3fv( tr.screen_normals[3] );
		pglVertex2f( 0.0f, screenHeight );
	pglEnd();
}

void GL_DrawScreenSpaceQuad( void )
{
	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglNormal3fv( tr.screen_normals[0] );
		pglVertex3f( 0.0f, 0.0f, 0.0f );
		pglNormal3fv( tr.screen_normals[1] );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3f( 0.0f, glState.height, 0.0f );
		pglNormal3fv( tr.screen_normals[2] );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3f( glState.width, glState.height, 0.0f );
		pglNormal3fv( tr.screen_normals[3] );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3f( glState.width, 0.0f, 0.0f );
	pglEnd();
}

void V_RenderPostEffect( word hProgram )
{
	if( hProgram <= 0 )
	{
		GL_BindShader( NULL );
		return; // bad shader?
	}

	if( RI->currentshader != &glsl_programs[hProgram] )
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[hProgram] );
	}

	glsl_program_t *shader = RI->currentshader;

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_SCREENMAP:
			if( post.m_bUseTarget ) // HACKHACK
				u->SetValue( post.target_rgb[0] );
			else u->SetValue( tr.screen_color );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth );
			break;
		case UT_COLORMAP:
			u->SetValue( post.target_rgb[0] );
			break;
		case UT_GRAYSCALE:
			u->SetValue( post.grayScaleFactor );
			break;
		case UT_BLURFACTOR:
			u->SetValue( post.blurFactor[0], post.blurFactor[1] );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_SCREENWIDTH:
			u->SetValue( (float)glState.width );
			break;
		case UT_SCREENHEIGHT:
			u->SetValue( (float)glState.height );
			break;
		case UT_FOCALDEPTH:
			u->SetValue( post.m_flLastDepth );
			break;
		case UT_FOCALLENGTH:
			u->SetValue( post.m_flLastLength );
			break;
		case UT_EXPOSURE:
		{
			const float minLuminance = 0.15f;
			const float maxLuminance = 100000.0f;
			const float avgBright = 0.25f;
			float exposure = avgBright / Q_min(maxLuminance, Q_max(post.avg_luminance_interp, minLuminance));
			u->SetValue(exposure);
			break;
		}
		case UT_DOFDEBUG:
			u->SetValue( CVAR_TO_BOOL( r_dof_debug ));
			break;
		case UT_FSTOP:
			u->SetValue( r_dof_fstop->value );
			break;
		case UT_ZFAR:
			u->SetValue( RI->view.farClip );
			break;
		case UT_GAMMATABLE:
			u->SetValue( &tr.gamma_table[0][0], 64 );
			break;
		case UT_DIFFUSEFACTOR:
			u->SetValue( tr.diffuseFactor );
			break;
		case UT_AMBIENTFACTOR:
			u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_LIGHTDIFFUSE:
			u->SetValue( post.m_vecSunLightColor.x, post.m_vecSunLightColor.y, post.m_vecSunLightColor.z );
			break;
		case UT_LIGHTORIGIN:
			u->SetValue( post.m_vecSunPosition.x, post.m_vecSunPosition.y, post.m_vecSunPosition.z ); 
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}

	// render a fullscreen quad
	RenderFSQ( glState.width, glState.height );
}

void RenderBlur( float blurX, float blurY )
{
	if( !blurX && !blurY )
		return;

	// update blur params
	post.blurFactor[0] = blurX;
	post.blurFactor[1] = blurY;

	if( !post.Begin( )) return;

	// do vertical blur
	post.RequestScreenColor();
	V_RenderPostEffect( post.blurShader[0] );

	// do horizontal blur
	post.RequestScreenColor();
	V_RenderPostEffect( post.blurShader[1] );

	post.End();
}

void RenderMonochrome( void )
{
	post.grayScaleFactor = GetGrayscaleFactor();
	if( post.grayScaleFactor <= 0.0f ) return;

	if( !post.Begin( )) return;

	// apply monochromatic
	GL_DebugGroupPush(__FUNCTION__);
	post.RequestScreenColor();
	V_RenderPostEffect( post.monoShader );
	post.End();
	GL_DebugGroupPop();
}

void RenderUnderwaterBlur( void )
{
	if( !CVAR_TO_BOOL( cv_water ) || tr.waterlevel < 3 )
		return;

	float factor = sin( tr.time * 0.1f * ( M_PI * 2.7f ));
	float blurX = RemapVal( factor, -1.0f, 1.0f, 0.18f, 0.23f );
	float blurY = RemapVal( factor, -1.0f, 1.0f, 0.15f, 0.24f );

	GL_DebugGroupPush(__FUNCTION__);
	RenderBlur( blurX, blurY );
	GL_DebugGroupPop();
}

void RenderNerveGasBlur( void )
{
	float blurAmount = 0.0f; // FIXME STUB gHUD.m_flBlurAmount
	if (blurAmount <= 0.0f )
		return;

	float factor = sin( tr.time * 0.4f * ( M_PI * 1.7f ));
	float blurX = RemapVal( factor, -1.0f, 1.0f, 0.0f, 0.3f );
	float blurY = RemapVal( factor, -1.0f, 1.0f, 0.0f, 0.3f );

	blurX = bound( 0.0f, blurX, blurAmount);
	blurY = bound( 0.0f, blurY, blurAmount);

	GL_DebugGroupPush(__FUNCTION__);
	RenderBlur( blurX, blurY );
	GL_DebugGroupPop();
}

void RenderDOF( void )
{
	if( !post.ProcessDepthOfField( ))
		return;

	GL_DebugGroupPush(__FUNCTION__);
	post.RequestScreenColor();
	post.RequestScreenDepth();
	V_RenderPostEffect( post.dofShader );
	post.End();
	GL_DebugGroupPop();
}

void RenderSunShafts( void )
{
	if( !post.ProcessSunShafts( ))
		return;

	GL_DebugGroupPush(__FUNCTION__);
	post.RequestScreenColor();
	post.RequestScreenDepth();

	// we operate in small window to increase speedup
	post.SetTargetViewport();
	V_RenderPostEffect( post.genSunShafts );
	post.RequestTargetCopy( 0 );

	post.m_bUseTarget = true;
	V_RenderPostEffect( post.blurShader[0] );
	post.RequestTargetCopy( 0 );
	V_RenderPostEffect( post.blurShader[1] );
	post.RequestTargetCopy( 0 );
	post.m_bUseTarget = false;

	// back to normal size
	post.SetNormalViewport();
	V_RenderPostEffect( post.drawSunShafts );
	post.End();
	GL_DebugGroupPop();
}

void RenderBloom()
{
	if (!CVAR_TO_BOOL(r_bloom))
		return;

	if (FBitSet(RI->params, RP_ENVVIEW)) // no bloom in cubemaps
		return;

	if (post.blurMipShader <= 0)
	{
		GL_BindShader(NULL);
		return; // bad shader?
	}

	if (post.bloomShader <= 0)
	{
		GL_BindShader(NULL);
		return; // bad shader?
	}

	GL_DebugGroupPush(__FUNCTION__);
	GL_BindShader(&glsl_programs[post.blurMipShader]);
	GL_Bind(GL_TEXTURE0, tr.screen_temp_fbo->colortarget[0]);
	GL_Setup2D();

	int w = glState.width;
	int h = glState.height;
	glsl_program_t *shader = RI->currentshader;

	// render and blur mips
	for (int i = 0; i < 6; i++)
	{
		w /= 2;
		h /= 2;

		pglViewport(0, 0, w, h);
		for (int j = 0; j < shader->numUniforms; j++)
		{
			uniform_t *u = &shader->uniforms[j];
			switch (u->type)
			{
				case UT_SCREENSIZEINV:
					u->SetValue(1.0f / (float)w, 1.0f / (float)h);
					break;
				case UT_MIPLOD:
					u->SetValue((float)i);
					break;
				case UT_TEXCOORDCLAMP:
					u->SetValue(0.0f, 0.0f, 1.0f, 1.0f);
					break;
				case UT_BLOOMFIRSTPASS:
					u->SetValue((i < 1) ? 1 : 0);
					break;
			}
		}

		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, tr.screen_temp_fbo_mip[i]);
		RenderFSQ(glState.width, glState.height);
	}

	w = glState.width / 2;
	h = glState.height / 2;

	pglViewport(0, 0, w, h);
	pglBindFramebuffer(GL_FRAMEBUFFER_EXT, tr.screen_temp_fbo_mip[0]);
	GL_BindShader(&glsl_programs[post.bloomShader]);

	shader = RI->currentshader;
	for (int j = 0; j < shader->numUniforms; j++)
	{
		uniform_t *u = &shader->uniforms[j];
		switch (u->type)
		{
			case UT_SCREENSIZEINV:
				u->SetValue((float)w, (float)h);	//actual screen size, not inverted
				break;
		}
	}
	RenderFSQ(glState.width, glState.height);

	// blend screen image & first mip together
	pglViewport(0, 0, glState.width, glState.height);
	pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer);
	GL_BindShader(NULL);
	GL_Blend(GL_TRUE);
	pglBlendFunc(GL_ONE, GL_ONE);
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 1.0);
	RenderFSQ(glState.width, glState.height);
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0.0);
	GL_Blend(GL_FALSE);
	post.End();
	GL_DebugGroupPop();
}

void RenderTonemap()
{
	if (!CVAR_TO_BOOL(r_tonemap))
		return;

	GL_DebugGroupPush(__FUNCTION__);
	GL_Setup2D();
	post.RequestScreenColor();
	post.GenerateLuminance();
	V_RenderPostEffect(post.tonemapShader);
	post.End();
	GL_DebugGroupPop();
}
