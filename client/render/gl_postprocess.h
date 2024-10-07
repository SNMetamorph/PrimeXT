#pragma once
#include "mathlib.h"
#include "gl_local.h"
#include "gl_shader.h"
#include "postfx_parameters.h"

#define DEAD_GRAYSCALE_TIME		5.0f
#define TARGET_SIZE				256

// post-process shaders
class CBasePostEffects
{
public:
	word	blurShader[2];		// e.g. underwater blur	
	word	dofShader;		// iron sight with dof
	word	postprocessingShader;		// monochrome effect
	word	genSunShafts;		// sunshafts effect
	word	drawSunShafts;		// sunshafts effect
	word	tonemapShader;
	word	exposureGenShader;
	word	luminanceGenShader;
	word	luminanceDownscaleShader;
	word	blurMipShader;
	word	bloomShader;

	TextureHandle	target_rgb[2];
	float	blurFactor[2];
	bool	m_bUseTarget;
	CPostFxParameters fxParameters;

	// DOF parameters
	float	m_flCachedDepth;
	float	m_flLastDepth;
	float	m_flStartDepth;
	float	m_flOffsetDepth;
	float	m_flStartTime;
	float	m_flDelayTime;
	float	m_flStartLength;
	float	m_flOffsetLength;
	float	m_flLastLength;
	float	m_flDOFStartTime;

	// automatic exposure 
	gl_drawbuffer_t	**avg_luminance_fbo;
	gl_drawbuffer_t *exposure_storage_fbo[2];
	TextureHandle	avg_luminance_texture = TextureHandle::Null();
	TextureHandle exposure_storage_texture[2] = { TextureHandle::Null() };

	// sunshafts variables
	Vector	m_vecSunLightColor;
	Vector	m_vecSunPosition;

	void InitializeTextures();
	void InitializeShaders();

	void RequestScreenColor();
	void RequestScreenDepth();
	void RequestTargetCopy(int slot);
	bool ProcessDepthOfField();
	bool ProcessSunShafts();
	void SetNormalViewport();
	void SetTargetViewport();
	bool Begin();
	void End();

	void PrintLuminanceValue();
	void RenderAverageLuminance();
	TextureHandle RenderExposureStorage();

private:
	void InitScreencopyFramebuffer();
	void InitTargetColor(int slot);
	void InitDepthOfField();
	void InitAutoExposure();
};
