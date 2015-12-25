/*
r_beams.cpp - beams rendering
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "mathlib.h"
#include "customentity.h"
#include "triangleapi.h"
#include "particledef.h"
#include "beamdef.h"
#include "r_efx.h"

#define NOISE_DIVISIONS	64	// don't touch - many tripmines cause the crash when it equal 128

typedef struct
{
	Vector	pos;
	Vector	color;
	float	texcoord;	// Y texture coordinate
	float	width;
	float	alpha;
} beamseg_t;

static float	rgNoise[NOISE_DIVISIONS+1];	// global noise array
static BEAM	**cl_active_beams;
static BEAM	**cl_free_beams;
static particle_t	**cl_free_trails;

void R_InitViewBeams( void )
{
	INIT_BEAMCHAINS( &cl_active_beams, &cl_free_beams, &cl_free_trails );
}

/*
================
R_AddServerBeam

Add the beam that encoded as custom entity
================
*/
void R_AddServerBeam( cl_entity_t *pEnvBeam )
{
	if( tr.num_beams_entities >= MAX_VISIBLE_PACKET )
	{
		ALERT( at_error, "Too many static beams %d!\n", tr.num_beams_entities );
		return;
	}

	if( pEnvBeam )
	{
		tr.beams_entities[tr.num_beams_entities] = pEnvBeam;
		tr.num_beams_entities++;
	}
}

/*
==============================================================

VIEWBEAMS DRAW METHODS

==============================================================
*/

// freq2 += step * 0.1;
// Fractal noise generator, power of 2 wavelength
static void FracNoise( float *noise, int divs, float scale )
{
	int div2;
	
	div2 = divs >> 1;
	if( divs < 2 ) return;

	// noise is normalized to +/- scale
	noise[div2] = ( noise[0] + noise[divs] ) * 0.5f + scale * RANDOM_FLOAT( -1.0f, 1.0f );

	if( div2 > 1 )
	{
		FracNoise( &noise[div2], div2, scale * 0.5f );
		FracNoise( noise, div2, scale * 0.5f );
	}
}

static void SineNoise( float *noise, int divs )
{
	float freq = 0;
	float step = M_PI / (float)divs;

	for( int i = 0; i < divs; i++ )
	{
		noise[i] = sin( freq );
		freq += step;
	}
}

static cl_entity_t *GET_BEAM_ENTITY( int index )
{
	cl_entity_t *ent;

	if( index > 0 )
		index = BEAMENT_ENTITY( index );
	ent = GET_ENTITY( index );

	return ent;
}

static bool ComputeBeamEntPosition( int beamEnt, Vector &pt )
{
	cl_entity_t *pEnt;
	int nAttachment;

	pEnt = GET_BEAM_ENTITY( beamEnt );
	nAttachment = ( beamEnt > 0 ) ? BEAMENT_ATTACHMENT( beamEnt ) : 0;

	if( !pEnt )
	{
		pt = g_vecZero;
		return false;
	}

	if( UTIL_IsLocal( pEnt->index ) && !RI.thirdPerson )
	{
		// if we view beam at firstperson use viewmodel instead
		pEnt = GET_VIEWMODEL();
	}

	// get attachment
	if( nAttachment > 0 )
		pt = pEnt->attachment[nAttachment - 1];
	else pt = pEnt->origin;

	return true;
}

static void ComputeBeamPerpendicular( const Vector &vecBeamDelta, Vector &pPerp )
{
	// Direction in worldspace of the center of the beam
	pPerp = CrossProduct( RI.vforward, vecBeamDelta.Normalize( )).Normalize();
}

static void ComputeNormal( const Vector &vStartPos, const Vector &vNextPos, Vector &pNormal )
{
	// vTangentY = line vector for beam
	Vector vTangentY = vStartPos - vNextPos;

	// vDirToBeam = vector from viewer origin to beam
	Vector vDirToBeam = vStartPos - RI.vieworg;

	// Get a vector that is perpendicular to us and perpendicular to the beam.
	// This is used to fatten the beam.
	pNormal = CrossProduct( vTangentY, vDirToBeam ).Normalize();
}

static void SetBeamRenderMode( int rendermode )
{
	if( rendermode == kRenderTransAdd )
	{
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	}
	else pglDisable( GL_BLEND );	// solid mode

	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}

/*
================
CL_DrawSegs

general code for drawing beams
================
*/
static void CL_DrawSegs( int modelIndex, float frame, int rendermode, const Vector &source, const Vector &delta,
	float width, float scale, float freq, float speed, int segments, int flags, const Vector &color )
{
	int	noiseIndex, noiseStep;
	int	total_segs, segs_drawn;
	float	div, length, fraction, factor;
	float	flMaxWidth, vLast, vStep, brightness;
	Vector	perp1, vLastNormal;
	model_t	*m_pSprite;
	beamseg_t	curSeg;

	if( !r_draw_beams->value )
		return;

	m_pSprite = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pSprite || segments < 2  )
		return;

	length = delta.Length();
	flMaxWidth = width * 0.5f;
	div = 1.0f / ( segments - 1 );

	if( length * div < flMaxWidth * 1.414f )
	{
		// Here, we have too many segments; we could get overlap... so lets have less segments
		segments = (int)( length / ( flMaxWidth * 1.414f )) + 1;
		if( segments < 2 ) segments = 2;
	}

	if( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	div = 1.0f / (segments - 1);
	length *= 0.01f;
	vStep = length * div;	// Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1 );

	if( flags & FBEAM_SINENOISE )
	{
		if( segments < 16 )
		{
			segments = 16;
			div = 1.0f / ( segments - 1 );
		}
		scale *= 10;
		length = segments * ( 1.0f / 10 );
	}
	else
	{
		scale *= length;
	}

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)( NOISE_DIVISIONS - 1 ) * div * 65536.0f );
	noiseIndex = 0;
	
	if( flags & FBEAM_SINENOISE )
	{
		noiseIndex = 0;
	}

	brightness = 1.0f;
	if( flags & FBEAM_SHADEIN )
	{
		brightness = 0;
	}

	// Choose two vectors that are perpendicular to the beam
	ComputeBeamPerpendicular( delta, perp1 );

	segs_drawn = 0;
	total_segs = segments;

	SetBeamRenderMode( rendermode );
	gEngfuncs.pTriAPI->SpriteTexture( m_pSprite, frame );	
	pglBegin( GL_TRIANGLE_STRIP );

	// specify all the segments.
	for( int i = 0; i < segments; i++ )
	{
		beamseg_t	nextSeg;
		Vector vPoint1, vPoint2;
	
		assert( noiseIndex < ( NOISE_DIVISIONS << 16 ));
		nextSeg.alpha = 1.0f;

		fraction = i * div;

		if(( flags & FBEAM_SHADEIN ) && ( flags & FBEAM_SHADEOUT ))
		{
			if( fraction < 0.5f )
			{
				brightness = 2.0f * fraction;
			}
			else
			{
				brightness = 2.0f * ( 1.0f - fraction );
			}
		}
		else if( flags & FBEAM_SHADEIN )
		{
			brightness = fraction;
		}
		else if( flags & FBEAM_SHADEOUT )
		{
			brightness = 1.0f - fraction;
		}

		// clamps
		brightness = bound( 0.0f, brightness, 1.0f );
		nextSeg.color = color * brightness;

		// UNDONE: Make this a spline instead of just a line?
		nextSeg.pos = source + fraction * delta;

		// distort using noise
		if( scale != 0 )
		{
			factor = rgNoise[noiseIndex>>16] * scale;
			if( flags & FBEAM_SINENOISE )
			{
				float s, c;

				SinCos( fraction * M_PI * length + freq, &s, &c );
				nextSeg.pos = nextSeg.pos + (factor * s) * RI.vup;

				// rotate the noise along the perpendicluar axis a bit to keep the bolt
				// from looking diagonal
				nextSeg.pos = nextSeg.pos + (factor * c) * RI.vright;
			}
			else
			{
				nextSeg.pos = nextSeg.pos + factor * perp1;
			}
		}

		// specify the next segment.
		nextSeg.width = width * 2.0f;
		nextSeg.texcoord = vLast;

 		if( segs_drawn > 0 )
		{
			// Get a vector that is perpendicular to us and perpendicular to the beam.
			// This is used to fatten the beam.
			Vector vNormal, vAveNormal;

			ComputeNormal( curSeg.pos, nextSeg.pos, vNormal );

			if( segs_drawn > 1 )
			{
				// Average this with the previous normal
				vAveNormal = ((vNormal + vLastNormal) * 0.5f).Normalize();
			}
			else
			{
				vAveNormal = vNormal;
			}

			vLastNormal = vNormal;

			// draw regular segment
			vPoint1 = curSeg.pos + ( curSeg.width * 0.5f ) * vAveNormal;
			vPoint2 = curSeg.pos + (-curSeg.width * 0.5f ) * vAveNormal;

			pglColor4f( curSeg.color[0], curSeg.color[1], curSeg.color[2], curSeg.alpha );
			pglTexCoord2f( 0.0f, curSeg.texcoord );
			pglNormal3fv( vAveNormal );
			pglVertex3fv( vPoint1 );

			pglColor4f( curSeg.color[0], curSeg.color[1], curSeg.color[2], curSeg.alpha );
			pglTexCoord2f( 1.0f, curSeg.texcoord );
			pglNormal3fv( vAveNormal );
			pglVertex3fv( vPoint2 );
		}

		curSeg = nextSeg;
		segs_drawn++;

 		if( segs_drawn == total_segs )
		{
			// draw the last segment
			vPoint1 = curSeg.pos + ( curSeg.width * 0.5f ) * vLastNormal;
			vPoint2 = curSeg.pos + (-curSeg.width * 0.5f ) * vLastNormal;

			// specify the points.
			pglColor4f( curSeg.color[0], curSeg.color[1], curSeg.color[2], curSeg.alpha );
			pglTexCoord2f( 0.0f, curSeg.texcoord );
			pglNormal3fv( vLastNormal );
			pglVertex3fv( vPoint1 );

			pglColor4f( curSeg.color[0], curSeg.color[1], curSeg.color[2], curSeg.alpha );
			pglTexCoord2f( 1.0f, curSeg.texcoord );
			pglNormal3fv( vLastNormal );
			pglVertex3fv( vPoint2 );
		}

		vLast += vStep; // advance texture scroll (v axis only)
		noiseIndex += noiseStep;
	}

	pglEnd();
}

/*
================
CL_DrawDisk

Draw beamdisk
================
*/
static void CL_DrawDisk( int modelIndex, float frame, int rendermode, const Vector &source, const Vector &delta,
	float width, float scale, float freq, float speed, int segments, const Vector &color )
{
	float	div, length, fraction;
	float	w, vLast, vStep;
	model_t	*m_pSprite;
	Vector	point;

	m_pSprite = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pSprite || segments < 2 )
		return;

	if( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	length = delta.Length() * 0.01f;
	if( length < 0.5f ) length = 0.5f;	// don't lose all of the noise/texture on short beams
		
	div = 1.0f / (segments - 1);
	vStep = length * div;		// Texture length texels per space pixel
	
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1 );
	scale = scale * length;

	w = freq * delta[2];

	SetBeamRenderMode( rendermode );
	gEngfuncs.pTriAPI->SpriteTexture( m_pSprite, frame );

	pglBegin( GL_TRIANGLE_STRIP );

	// NOTE: We must force the degenerate triangles to be on the edge
	for( int i = 0; i < segments; i++ )
	{
		float	s, c;

		fraction = i * div;
		point = source;

		pglColor4f( color[0], color[1], color[2], 1.0f );
		pglTexCoord2f( 1.0f, vLast );
		pglVertex3fv( point );

		SinCos( fraction * 2.0f * M_PI, &s, &c );
		point[0] = s * w + source[0];
		point[1] = c * w + source[1];
		point[2] = source[2];

		pglColor4f( color[0], color[1], color[2], 1.0f );
		pglTexCoord2f( 0.0f, vLast );
		pglVertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
	}

	pglEnd();
}

/*
================
CL_DrawCylinder

Draw beam cylinder
================
*/
static void CL_DrawCylinder( int modelIndex, float frame, int rendermode, const Vector &source, const Vector &delta,
	float width, float scale, float freq, float speed, int segments, const Vector &color )
{
	float	length, fraction;
	float	div, vLast, vStep;
	model_t	*m_pSprite;
	Vector	point;

	m_pSprite = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pSprite || segments < 2 )
		return;

	if( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	length = delta.Length() * 0.01f;
	if( length < 0.5f ) length = 0.5f;	// Don't lose all of the noise/texture on short beams
	
	div = 1.0f / (segments - 1);
	vStep = length * div;		// Texture length texels per space pixel
	
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1.0f );
	scale = scale * length;
	
	GL_Cull( GL_NONE );	// draw both sides
	SetBeamRenderMode( rendermode );
	gEngfuncs.pTriAPI->SpriteTexture( m_pSprite, frame );

	pglBegin( GL_TRIANGLE_STRIP );

	for( int i = 0; i < segments; i++ )
	{
		float	s, c;

		fraction = i * div;
		SinCos( fraction * 2.0f * M_PI, &s, &c );

		point[0] = s * freq * delta[2] + source[0];
		point[1] = c * freq * delta[2] + source[1];
		point[2] = source[2] + width;

		pglColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
		pglTexCoord2f( 1.0f, vLast );
		pglVertex3fv( point );

		point[0] = s * freq * (delta[2] + width) + source[0];
		point[1] = c * freq * (delta[2] + width) + source[1];
		point[2] = source[2] - width;

		pglColor4f( color[0], color[1], color[2], 1.0f );
		pglTexCoord2f( 0.0f, vLast );
		pglVertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
	}
	
	pglEnd();
	GL_Cull( GL_FRONT );
}

/*
================
CL_DrawRing

Draw beamring
================
*/
void CL_DrawRing( int modelIndex, float frame, int rendermode, const Vector &source, const Vector &delta, float width, 
	float amplitude, float freq, float speed, int segments, const Vector &color )
{
	int	noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep;
	Vector	last1, last2, point, screen, screenLast;
	Vector	center, xaxis, yaxis, zaxis, tmp, normal;
	float	radius, x, y, scale;
	model_t	*m_pSprite;

	m_pSprite = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pSprite || segments < 2 )
		return;

	Vector d = delta;
	screenLast = g_vecZero;

	segments = segments * M_PI;
	
	if( segments > NOISE_DIVISIONS * 8 )
		segments = NOISE_DIVISIONS * 8;

	length = d.Length() * 0.01f * M_PI;
	if( length < 0.5f ) length = 0.5f;		// Don't lose all of the noise/texture on short beams
		
	div = 1.0 / ( segments - 1 );

	vStep = length * div / 8.0f;			// Texture length texels per space pixel
	
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1.0f );
	scale = amplitude * length / 8.0f;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)(( NOISE_DIVISIONS - 1 ) * div * 65536.0f ) * 8;
	noiseIndex = 0;

	d *= 0.5f;
	center = source + d;
	zaxis = g_vecZero;

	xaxis = d;
	radius = xaxis.Length();
	
	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1[0] = radius;
	last1[1] = radius;
	last1[2] = scale;

	tmp = center + last1;
	screen = center - last1;

	// Is that box in PVS && frustum?
	if( !Mod_BoxVisible( screen, tmp, Mod_GetCurrentVis( )) || R_CullBox( screen, tmp, RI.clipFlags ))	
	{
		return;
	}

	yaxis[0] = xaxis[1];
	yaxis[1] = -xaxis[0];
	yaxis[2] = 0;

	yaxis = yaxis.Normalize();
	yaxis *= radius;

	int j = segments / 8;

	SetBeamRenderMode( rendermode );
	gEngfuncs.pTriAPI->SpriteTexture( m_pSprite, frame );

	pglBegin( GL_TRIANGLE_STRIP );

	for( int i = 0; i < segments + 1; i++ )
	{
		fraction = i * div;
		SinCos( fraction * 2.0f * M_PI, &x, &y );

		point = xaxis * x + yaxis * y + center;

		// distort using noise
		factor = rgNoise[(noiseIndex >> 16) & (NOISE_DIVISIONS - 1)] * scale;
		point = point + factor * RI.vup;

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = rgNoise[(noiseIndex>>16) & (NOISE_DIVISIONS - 1)] * scale;
		factor *= cos( fraction * M_PI * 24 + freq );
		point = point + factor * RI.vright;

		// Transform point into screen space
		WorldToScreen( point, screen );

		if( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp = screen - screenLast;

			// We don't need Z, we're in screen space
			tmp.z = 0;
			tmp = tmp.Normalize();

			// Build point along normal line (normal is -y, x)
			normal = RI.vup * tmp.x;
			normal = normal + tmp.y * RI.vright;
			
			// make a wide line
			last1 = point + ( width * normal);
			last2 = point + (-width * normal);

			vLast += vStep;	// advance texture scroll (v axis only)
			pglColor4f( color[0], color[1], color[2], 1.0f );
			pglTexCoord2f( 1.0f, vLast );
			pglVertex3fv( last2 );

			pglColor4f( color[0], color[1], color[2], 1.0f );
			pglTexCoord2f( 0.0f, vLast );
			pglVertex3fv( last1 );
		}

		screenLast = screen;
		noiseIndex += noiseStep;

		j--;
		if( j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			FracNoise( rgNoise, NOISE_DIVISIONS, 1.0f );
		}
	}

	pglEnd();
}

/*
==============
CL_DrawLaser

Helper to drawing laser
==============
*/
void CL_DrawLaser( BEAM *pbeam, int frame, int rendermode, const Vector &color, int spriteIndex )
{
	Vector	color2;
	Vector	beamDir;
	float	flDot;
	
	color2 = color;

	beamDir = (pbeam->target - pbeam->source).Normalize();

	flDot = DotProduct( beamDir, RI.vforward );

	// abort if the player's looking along it away from the source
	if( flDot > 0 )
	{
		return;
	}
	else
	{
		// Fade the beam if the player's not looking at the source
		float	flFade = pow( flDot, 10 );
		Vector	localDir, vecProjection, tmp;
		float	flDistance;

		// Fade the beam based on the player's proximity to the beam
		localDir = RI.vieworg - pbeam->source;
		flDot = DotProduct( beamDir, localDir );
		vecProjection = beamDir * flDot;
		tmp = localDir - vecProjection;
		flDistance = tmp.Length();

		if( flDistance > 30 )
		{
			flDistance = 1.0f - (( flDistance - 30.0f ) / 64.0f );

			if( flDistance <= 0 ) flFade = 0;
			else flFade *= pow( flDistance, 3 );
		}

		if( flFade < ( 1.0f / 255.0f ))
			return;

		color2 *= flFade;
	}

	CL_DrawSegs( spriteIndex, frame, rendermode, pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude,
		pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags, color2 );

}

/*
================
CL_DrawBeamFollow

Draw beam trail
================
*/
static void DrawBeamFollow( int modelIndex, particle_t *pHead, int frame, int rendermode, const Vector &delta,
	Vector &screen, Vector &screenLast, float die, const Vector &source, int flags, float width,
	float amplitude, float freq, const Vector &color )
{
	float	fraction;
	float	div;
	float	vLast = 0.0;
	float	vStep = 1.0;
	Vector	last1, last2;
	Vector	tmp, normal, scaledColor;
	model_t	*m_pSprite;

	m_pSprite = IEngineStudio.GetModelByIndex( modelIndex );
	if( !m_pSprite ) return;

	// UNDONE: This won't work, screen and screenLast must be extrapolated here to fix the
	// first beam segment for this trail

	// Build world-space normal to screen-space direction vector
	tmp = screen - screenLast;

	// We don't need Z, we're in screen space
	tmp.z = 0;
	tmp = tmp.Normalize();

	// build point along noraml line (normal is -y, x)
	normal = RI.vup * tmp.x; // build point along normal line (normal is -y, x)
	normal = normal + tmp.y * RI.vright;

	// make a wide line
	last1 = delta + ( width * normal );
	last2 = delta + (-width * normal );

	div = 1.0f / amplitude;
	fraction = ( die - GET_CLIENT_TIME()) * div;

	scaledColor = color * fraction;

	SetBeamRenderMode( rendermode );
	gEngfuncs.pTriAPI->SpriteTexture( m_pSprite, frame );

	pglBegin( GL_QUADS );

	while( pHead )
	{
		pglColor4f( scaledColor[0], scaledColor[1], scaledColor[2], 1.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3fv( last2 );

		pglColor4f( scaledColor[0], scaledColor[1], scaledColor[2], 1.0f );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex3fv( last1 );

		// Transform point into screen space
		WorldToScreen( pHead->org, screen );

		// Build world-space normal to screen-space direction vector
		tmp = screen - screenLast;
		// We don't need Z, we're in screen space
		tmp.z = 0;
		tmp = tmp.Normalize();

		// build point along normal line (normal is -y, x)
		normal = RI.vup * tmp.x;
		normal = normal + tmp.y * RI.vright;

		// Make a wide line
		last1 = pHead->org + ( width * normal );
		last2 = pHead->org + (-width * normal );

		vLast += vStep; // advance texture scroll (v axis only)

		if( pHead->next != NULL )
		{
			fraction = (pHead->die - GET_CLIENT_TIME()) * div;
			scaledColor = color * fraction;
		}
		else
		{
			scaledColor = g_vecZero;
			fraction = 0.0;
		}
	
		pglColor4f( scaledColor[0], scaledColor[1], scaledColor[2], 1.0f );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3fv( last1 );

		pglColor4f( scaledColor[0], scaledColor[1], scaledColor[2], 255 );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3fv( last2 );
		
		screenLast = screen;
		pHead = pHead->next;
	}

	pglEnd();
}

/*
==============
CL_CullBeam

Cull the beam by bbox
==============
*/
bool CL_CullBeam( const Vector &start, const Vector &end, bool pvsOnly )
{
	// support for custom mirror management
	if( RI.currentbeam != NULL )
	{
		// don't reflect this entity in mirrors
		if( RI.currentbeam->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
			return true;

		// draw only in mirrors
		if( RI.currentbeam->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
			return true;
	}

	Vector mins, maxs;

	for( int i = 0; i < 3; i++ )
	{
		if( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}
		
		// don't let it be zero sized
		if( mins[i] == maxs[i] )
			maxs[i] += 1;
	}

	// check bbox
	if( Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( )))
	{
		if( pvsOnly || !R_CullBox( mins, maxs, RI.clipFlags ))
		{
			// beam is visible
			return false;	
		}
	}

	// beam is culled
	return true;
}

/*
==============
CL_RecomputeBeamEndpoints

Recomputes beam endpoints..
==============
*/
qboolean CL_RecomputeBeamEndpoints( BEAM *pbeam )
{
	if( pbeam->flags & FBEAM_STARTENTITY )
	{
		if( ComputeBeamEntPosition( pbeam->startEntity, pbeam->source ))
		{
			pbeam->flags |= FBEAM_STARTVISIBLE;
		}
		else if(!( pbeam->flags & FBEAM_FOREVER ))
		{
			pbeam->flags &= ~(FBEAM_STARTENTITY);
		}
		else
		{
			// ALERT( at_warning, "can't find start entity\n" );
			// return false;
		}

		// if we've never seen the start entity, don't display
		if( !( pbeam->flags & FBEAM_STARTVISIBLE ))
			return false;
	}

	if( pbeam->flags & FBEAM_ENDENTITY )
	{
		if( ComputeBeamEntPosition( pbeam->endEntity, pbeam->target ))
		{
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}
		else if( !( pbeam->flags & FBEAM_FOREVER ))
		{
			pbeam->flags &= ~(FBEAM_ENDENTITY);
			pbeam->die = GET_CLIENT_TIME();
			return false;
		}
		else
		{
			return false;
		}

		// if we've never seen the end entity, don't display
		if( !( pbeam->flags & FBEAM_ENDVISIBLE ))
			return false;
	}
	return true;
}

/*
==============
CL_BeamAttemptToDie

Check for expired beams
==============
*/
bool CL_BeamAttemptToDie( BEAM *pBeam )
{
	assert( pBeam != NULL );

	// premanent beams never die automatically
	if( pBeam->flags & FBEAM_FOREVER )
		return false;

	if( pBeam->type == TE_BEAMFOLLOW && pBeam->particles )
	{
		// wait for all trails are dead
		return false;
	}

	// other beams
	if( pBeam->die > GET_CLIENT_TIME())
		return false;

	return true;
}

/*
==============
CL_UpdateBeam

Update beam vars
==============
*/
void CL_UpdateBeam( BEAM *pbeam, float frametime )
{
	pbeam->flags |= FBEAM_ISACTIVE;

	if( Mod_GetType( pbeam->modelIndex ) == mod_bad )
	{
		pbeam->flags &= ~FBEAM_ISACTIVE; // force to ignore
		pbeam->die = GET_CLIENT_TIME();
		return;
	}

	// update frequency
	pbeam->freq += frametime;

	// Generate fractal noise
	if( RENDER_GET_PARM( PARM_CLIENT_INGAME, 0 ) && !RI.refdef.paused )
	{
		rgNoise[0] = 0;
		rgNoise[NOISE_DIVISIONS] = 0;
	}

	if( pbeam->amplitude != 0 && RENDER_GET_PARM( PARM_CLIENT_INGAME, 0 ) && !RI.refdef.paused )
	{
		if( pbeam->flags & FBEAM_SINENOISE )
		{
			SineNoise( rgNoise, NOISE_DIVISIONS );
		}
		else
		{
			FracNoise( rgNoise, NOISE_DIVISIONS, 1.0f );
		}
	}

	// update end points
	if( pbeam->flags & ( FBEAM_STARTENTITY|FBEAM_ENDENTITY ))
	{
		// makes sure attachment[0] + attachment[1] are valid
		if( !CL_RecomputeBeamEndpoints( pbeam ))
		{
			pbeam->flags &= ~FBEAM_ISACTIVE; // force to ignore
			return;
		}

		// compute segments from the new endpoints
		pbeam->delta = pbeam->target - pbeam->source;

		if( pbeam->amplitude >= 0.50f )
			pbeam->segments = pbeam->delta.Length() * 0.25f + 3.0f; // one per 4 pixels
		else pbeam->segments = pbeam->delta.Length() * 0.075f + 3.0f; // one per 16 pixels
	}

	// get position data for spline beam
	switch( pbeam->type )
	{
	case TE_BEAMPOINTS:
		// UNDONE: Build culling volumes for other types of beams
		if( CL_CullBeam( pbeam->source, pbeam->target, false ))
		{
			pbeam->flags &= ~FBEAM_ISACTIVE; // force to ignore
			return;
		}
		break;
	}

	if( pbeam->flags & ( FBEAM_FADEIN|FBEAM_FADEOUT ))
	{
		// update life cycle
		pbeam->t = pbeam->freq + ( pbeam->die - GET_CLIENT_TIME());
		if( pbeam->t != 0.0f ) pbeam->t = 1.0f - pbeam->freq / pbeam->t;
	}
}

/*
==============
CL_FreeDeadTrails

Free dead trails associated with beam
==============
*/
void CL_FreeDeadTrails( particle_t **trail )
{
	particle_t	*kill;
	particle_t	*p;

	// kill all the ones hanging direcly off the base pointer
	while( 1 ) 
	{
		kill = *trail;
		if( kill && kill->die < GET_CLIENT_TIME())
		{
			*trail = kill->next;
			kill->next = *cl_free_trails;
			*cl_free_trails = kill;
			continue;
		}
		break;
	}

	// kill off all the others
	for( p = *trail; p; p = p->next )
	{
		while( 1 )
		{
			kill = p->next;
			if( kill && kill->die < GET_CLIENT_TIME())
			{
				p->next = kill->next;
				kill->next = *cl_free_trails;
				*cl_free_trails = kill;
				continue;
			}
			break;
		}
	}
}

/*
==============
CL_FreeBeam

==============
*/
void CL_FreeBeam( BEAM *pBeam )
{
	// free particles that have died off.
	CL_FreeDeadTrails( &pBeam->particles );

	// now link into free list;
	pBeam->next = *cl_free_beams;
	*cl_free_beams = pBeam;
}

/*
==============
CL_NumBeamsForEntity

==============
*/
int CL_NumBeamsForEntity( cl_entity_t *ent )
{
	int	numBeams = 0;
	BEAM	*pBeam, *pNext;

	if( !ent ) return NULL;

	// draw temporary entity beams
	for( pBeam = *cl_active_beams; pBeam; pBeam = pNext )
	{
		// need to store the next one since we may delete this one
		pNext = pBeam->next;

		if( ent == GET_BEAM_ENTITY( pBeam->startEntity ))
			numBeams++;
	}

	return numBeams;
}

/*
==============
CL_DrawBeamFollow

Helper to drawing beam follow
==============
*/
void CL_DrawBeamFollow( int spriteIndex, BEAM *pbeam, int frame, int rendermode, float frametime, float *color )
{
	particle_t	*particles;
	particle_t	*pnew;
	float		div;
	Vector		delta;
	Vector		screenLast;
	Vector		screen;

	CL_FreeDeadTrails( &pbeam->particles );

	particles = pbeam->particles;
	pnew = NULL;

	div = 0;
	if( pbeam->flags & FBEAM_STARTENTITY )
	{
		if( particles )
		{
			delta = particles->org - pbeam->source;
			div = delta.Length();

			if( div >= 32 && cl_free_trails && *cl_free_trails )
			{
				pnew = *cl_free_trails;
				*cl_free_trails = pnew->next;
			}
		}
		else if( cl_free_trails && *cl_free_trails )
		{
			pnew = *cl_free_trails;
			*cl_free_trails = pnew->next;
			div = 0;
		}
	}

	if( pnew )
	{
		pnew->org = pbeam->source;
		pnew->die = GET_CLIENT_TIME() + pbeam->amplitude;
		pnew->vel = g_vecZero;

		pnew->next = particles;
		pbeam->particles = pnew;
		particles = pnew;
	}

	// nothing to draw
	if( !particles ) return;

	if( !pnew && div != 0 )
	{
		delta = pbeam->source;
		WorldToScreen( pbeam->source, screenLast );
		WorldToScreen( particles->org, screen );
	}
	else if( particles && particles->next )
	{
		delta = particles->org;
		WorldToScreen( particles->org, screenLast );
		WorldToScreen( particles->next->org, screen );
		particles = particles->next;
	}
	else
	{
		return;
	}

	// draw it
	DrawBeamFollow( spriteIndex, pbeam->particles, frame, rendermode, delta, screen, screenLast, 
		pbeam->die, pbeam->source, pbeam->flags, pbeam->width, 
		pbeam->amplitude, pbeam->freq, color );
	
	// drift popcorn trail if there is a velocity
	particles = pbeam->particles;

	while( particles )
	{
		particles->org = particles->org + frametime * particles->vel;
		particles = particles->next;
	}
}

/*
==============
CL_DrawBeam

General code to drawing all beam types
==============
*/
void CL_DrawBeam( BEAM *pbeam )
{
	int	frame, rendermode;
	Vector	color, srcColor;

	// don't draw really short or inactive beams
	if(!( pbeam->flags & FBEAM_ISACTIVE ) || pbeam->delta.Length() < 0.1f )
	{
		return;
	}

	if( Mod_GetType( pbeam->modelIndex ) == mod_bad )
	{
		// don't draw beams without models
		pbeam->die = GET_CLIENT_TIME();
		return;
	}

	frame = ((int)( pbeam->frame + GET_CLIENT_TIME() * pbeam->frameRate ) % pbeam->frameCount );
	rendermode = ( pbeam->flags & FBEAM_SOLID ) ? kRenderNormal : kRenderTransAdd;

	// set color
	srcColor[0] = pbeam->r;
	srcColor[1] = pbeam->g;
	srcColor[2] = pbeam->b;

	if( pbeam->flags & FBEAM_FADEIN )
	{
		color = srcColor * pbeam->t;
	}
	else if ( pbeam->flags & FBEAM_FADEOUT )
	{
		color = srcColor * ( 1.0f - pbeam->t );
	}
	else
	{
		color = srcColor;
	}

	if( pbeam->type == TE_BEAMFOLLOW )
	{
		cl_entity_t *pStart;

		// HACKHACK: get brightness from head entity
		pStart = GET_BEAM_ENTITY( pbeam->startEntity ); 

		if( pStart && pStart->curstate.rendermode != kRenderNormal )
			pbeam->brightness = pStart->curstate.renderamt;
	}

	color *= ( pbeam->brightness / 255.0f );
	color *= ( 1.0f / 255.0f );

	pglShadeModel( GL_SMOOTH );

	switch( pbeam->type )
	{
	case TE_BEAMDISK:
		CL_DrawDisk( pbeam->modelIndex, frame, rendermode, pbeam->source, pbeam->delta, pbeam->width,
			pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMTORUS:
	case TE_BEAMCYLINDER:
		CL_DrawCylinder( pbeam->modelIndex, frame, rendermode, pbeam->source, pbeam->delta, pbeam->width,
			pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMPOINTS:
		CL_DrawSegs( pbeam->modelIndex, frame, rendermode, pbeam->source, pbeam->delta, pbeam->width,
			pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags, color );
		break;
	case TE_BEAMFOLLOW:
		CL_DrawBeamFollow( pbeam->modelIndex, pbeam, frame, rendermode, RI.refdef.frametime, color );
		break;
	case TE_BEAMRING:
		CL_DrawRing( pbeam->modelIndex, frame, rendermode, pbeam->source, pbeam->delta, pbeam->width,
			pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMHOSE:
		CL_DrawLaser( pbeam, frame, rendermode, color, pbeam->modelIndex );
		break;
	default:
		ALERT( at_error, "CL_DrawBeam: Unknown beam type %i\n", pbeam->type );
		break;
	}
	pglShadeModel( GL_FLAT );
}

/*
==============
CL_DrawServerBeam

Draw server static beams
==============
*/
void CL_DrawServerBeam( cl_entity_t *pbeam )
{
	BEAM	beam;
	int	beamType;
	int	beamFlags;

	assert( pbeam != NULL );

	// bad texture ?
	if( Mod_GetType( pbeam->curstate.modelindex ) != mod_sprite )
		return;

	memset( &beam, 0, sizeof( beam ));

	beamType = ( pbeam->curstate.rendermode & 0x0F );
	beamFlags = ( pbeam->curstate.rendermode & 0xF0 );
	beam.modelIndex = pbeam->curstate.modelindex;
	beam.frame = pbeam->curstate.frame;
	beam.frameRate = pbeam->curstate.framerate;
	beam.speed = pbeam->curstate.animtime;
	Mod_GetFrames( beam.modelIndex, beam.frameCount );
	beam.source = pbeam->origin;
	beam.target = pbeam->angles;
	beam.freq	= GET_CLIENT_TIME() * beam.speed;
	beam.startEntity = pbeam->curstate.sequence;
	beam.endEntity = pbeam->curstate.skin;
	beam.width = pbeam->curstate.scale;
	beam.amplitude = (float)(pbeam->curstate.body * 0.1f);
	beam.brightness = pbeam->curstate.renderamt;
	beam.r = pbeam->curstate.rendercolor.r;
	beam.g = pbeam->curstate.rendercolor.g;
	beam.b = pbeam->curstate.rendercolor.b;
	beam.flags = 0;

	beam.delta = beam.target - beam.source;

	if( beam.amplitude >= 0.50f )
		beam.segments = beam.delta.Length() * 0.25f + 3; // one per 4 pixels
	else beam.segments = beam.delta.Length() * 0.075f + 3; // one per 16 pixels

	// handle code from relinking.
	switch( beamType )
	{
	case BEAM_ENTS:
		beam.type	= TE_BEAMPOINTS;
		beam.flags = FBEAM_STARTENTITY|FBEAM_ENDENTITY;
		break;
	case BEAM_HOSE:
		beam.type	= TE_BEAMHOSE;
		beam.flags = FBEAM_STARTENTITY|FBEAM_ENDENTITY;
		break;
	case BEAM_ENTPOINT:
		beam.type	= TE_BEAMPOINTS;
		if( beam.startEntity ) beam.flags |= FBEAM_STARTENTITY;
		if( beam.endEntity ) beam.flags |= FBEAM_ENDENTITY;
		break;
	case BEAM_POINTS:
		// already set up
		break;
	}

	beam.flags |= beamFlags & ( FBEAM_SINENOISE|FBEAM_SOLID|FBEAM_SHADEIN|FBEAM_SHADEOUT );

	// draw it
	CL_UpdateBeam( &beam, RI.refdef.frametime );
	CL_DrawBeam( &beam );
}

void CL_DrawBeams( int fTrans )
{
	BEAM	*pBeam, *pNext;
	BEAM	*pPrev = NULL;
	int	i;
	
	// server beams don't allocate beam chains
	// all params are stored in cl_entity_t
	for( i = 0; i < tr.num_beams_entities; i++ )
	{
		RI.currentbeam = tr.beams_entities[i];

		if( fTrans && ((RI.currentbeam->curstate.rendermode & 0xF0) & FBEAM_SOLID ))
			continue;

		if( !fTrans && !((RI.currentbeam->curstate.rendermode & 0xF0) & FBEAM_SOLID ))
			continue;

		CL_DrawServerBeam( RI.currentbeam );
		r_stats.c_view_beams_count++;
	}

	RI.currentbeam = NULL;

	if( !cl_active_beams || !*cl_active_beams )
		return;

	// draw temporary entity beams
	for( pBeam = *cl_active_beams; pBeam; pBeam = pNext )
	{
		// need to store the next one since we may delete this one
		pNext = pBeam->next;

		if( fTrans && pBeam->flags & FBEAM_SOLID )
			continue;
		if( !fTrans && !( pBeam->flags & FBEAM_SOLID ))
			continue;

		// retire old beams
		if( CL_BeamAttemptToDie( pBeam ))
		{
			// reset links
			if( pPrev ) pPrev->next = pNext;
			else *cl_active_beams = pNext;

			// free the beam
			CL_FreeBeam( pBeam );

			pBeam = NULL;
			continue;
		}

		// update beam state
		CL_UpdateBeam( pBeam, RI.refdef.frametime );
		r_stats.c_view_beams_count++;
		CL_DrawBeam( pBeam );

		pPrev = pBeam;
	}
}
