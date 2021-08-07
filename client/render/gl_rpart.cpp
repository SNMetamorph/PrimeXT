/*
gl_rpart.cpp - quake-like particle system
Copyright (C) 2014 Uncle Mike

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
#include "const.h"
#include <mathlib.h>
#include "gl_local.h"
#include "com_model.h"
#include "r_studioint.h"
#include "gl_rpart.h"
#include "pm_defs.h"
#include "event_api.h"
#include "triangleapi.h"
#include "gl_sprite.h"

CQuakePartSystem	g_pParticles;

bool CQuakePart :: Evaluate( float gravity )
{
	Vector org, org2, org3, vel;

	float time = ( tr.time - m_flTime );
	float time2 = time * time;

	float curAlpha = m_flAlpha + m_flAlphaVelocity * time;
	float curRadius = m_flRadius + m_flRadiusVelocity * time;
	float curLength = m_flLength + m_flLengthVelocity * time;

	if( curAlpha <= 0.0f || curRadius <= 0.0f || curLength <= 0.0f )
	{
		// faded out
		return false;
	}

	Vector curColor = m_vecColor + m_vecColorVelocity * time;

	org.x = m_vecOrigin.x + m_vecVelocity.x * time + m_vecAccel.x * time2;
	org.y = m_vecOrigin.y + m_vecVelocity.y * time + m_vecAccel.y * time2;
	org.z = m_vecOrigin.z + m_vecVelocity.z * time + m_vecAccel.z * time2 * gravity;

	if( FBitSet( m_iFlags, FPART_UNDERWATER ))
	{
		// underwater particle
		org2 = Vector( org.x, org.y, org.z + curRadius );

		int contents = POINT_CONTENTS( org );

		if( contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA )
		{
			// not underwater
			return false;
		}
	}

	if( FBitSet( m_iFlags, FPART_FRICTION ))
	{
		// water friction affected particle
		int contents = POINT_CONTENTS( org );

		if( contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA )
		{
			// add friction		
			switch( contents )
			{
			case CONTENTS_WATER:
				m_vecVelocity *= 0.25f;
				m_vecAccel *= 0.25f;
				break;
			case CONTENTS_SLIME:
				m_vecVelocity *= 0.20f;
				m_vecAccel *= 0.20f;
				break;
			case CONTENTS_LAVA:
				m_vecVelocity *= 0.10f;
				m_vecAccel *= 0.10f;
				break;
			}
			
			// don't add friction again
			m_iFlags &= ~FPART_FRICTION;
			curLength = 1.0f;
				
			// reset
			m_flTime = tr.time;
			m_vecColor = curColor;
			m_flAlpha = curAlpha;
			m_flRadius = curRadius;
			m_vecOrigin = org;

			// don't stretch
			m_iFlags &= ~FPART_STRETCH;
			m_flLengthVelocity = 0.0f;
			m_flLength = curLength;
		}
	}

	if( FBitSet( m_iFlags, FPART_BOUNCE ))
	{
		// bouncy particle
		pmtrace_t pmtrace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( m_vecLastOrg, org, PM_STUDIO_IGNORE, -1, &pmtrace );

		if( pmtrace.fraction != 1.0f )
		{
			// reflect velocity
			time = tr.time - (tr.frametime + tr.frametime * pmtrace.fraction);
			time = (time - m_flTime);

			vel.x = m_vecVelocity.x;
			vel.y = m_vecVelocity.y;
			vel.z = m_vecVelocity.z + m_vecAccel.z * gravity * time;

			float d = DotProduct( vel, pmtrace.plane.normal ) * 2.0f;
			m_vecVelocity = vel - pmtrace.plane.normal * d;
			m_vecVelocity *= bound( 0.0f, m_flBounceFactor, 1.0f );

			// check for stop or slide along the plane
			if( pmtrace.plane.normal.z > 0.0f && m_vecVelocity.z < 1.0f )
			{
				if( pmtrace.plane.normal.z >= 0.7f )
				{
					m_vecVelocity = g_vecZero;
					m_vecAccel = g_vecZero;
					m_iFlags &= ~FPART_BOUNCE;
				}
				else
				{
					// FIXME: check for new plane or free fall
					float dot = DotProduct( m_vecVelocity, pmtrace.plane.normal );
					m_vecVelocity += ( pmtrace.plane.normal * -dot );

					dot = DotProduct( m_vecAccel, pmtrace.plane.normal );
					m_vecAccel += ( pmtrace.plane.normal * -dot );
				}
			}

			org = pmtrace.endpos;
			curLength = 1.0f;

			// reset
			m_flTime = tr.time;
			m_vecColor = curColor;
			m_flAlpha = curAlpha;
			m_flRadius = curRadius;
			m_vecOrigin = org;

			// don't stretch
			m_iFlags &= ~FPART_STRETCH;
			m_flLengthVelocity = 0.0f;
			m_flLength = curLength;
		}
	}
	
	// save current origin if needed
	if( FBitSet( m_iFlags, ( FPART_BOUNCE|FPART_STRETCH )))
	{
		org2 = m_vecLastOrg;
		m_vecLastOrg = org;
	}

	// vertex lit particle
	if( FBitSet( m_iFlags, FPART_VERTEXLIGHT ))
	{
		Vector light;
		// gather static lighting
		gEngfuncs.pTriAPI->LightAtPoint( org, light );
		light *= (1.0f/255.0f);

		// gather dynamic lighting
		light += R_LightsForPoint( org, curRadius );

		// renormalize lighting
		float f = Q_max( Q_max( light.x, light.y ), light.z );
		if( f > 1.0f ) light *= ( 1.0f / f );
		curColor *= light;	// multiply to diffuse
	}

	if( FBitSet( m_iFlags, FPART_INSTANT ))
	{
		// instant particle
		m_flAlphaVelocity = 0.0f;
		m_flAlpha = 0.0f;
	}

	if( curRadius == 1.0f )
	{
		float scale = 0.0f;

		// hack a scale up to keep quake particles from disapearing
		scale += (org.x - GetVieworg().x) * GetVForward().x;
		scale += (org.y - GetVieworg().y) * GetVForward().y;
		scale += (org.z - GetVieworg().z) * GetVForward().z;
		if( scale >= 20.0f ) curRadius = 1.0f + scale * 0.004f;
	}

	Vector axis[3], verts[4];
	Vector absmin, absmax;

	// prepare to draw
	if( curLength != 1.0f )
	{
		// find orientation vectors
		axis[0] = GetVieworg() - org;
		axis[1] = org2 - org;
		axis[2] = CrossProduct( axis[0], axis[1] );

		axis[1] = axis[1].Normalize();
		axis[2] = axis[2].Normalize();

		// find normal
		axis[0] = CrossProduct( axis[1], axis[2] );
		axis[0] = axis[0].Normalize();

		org3 = org + ( axis[1] * -curLength );
		axis[2] *= m_flRadius;

		// setup vertexes
		verts[0] = org3 - axis[2];
		verts[1] = org3 + axis[2];
		verts[2] = org + axis[2];
		verts[3] = org - axis[2];
	}
	else
	{
		if( m_flRotation )
		{
			// Rotate it around its normal
			RotatePointAroundVector( axis[1], GetVForward(), GetVLeft(), m_flRotation );
			axis[2] = CrossProduct( GetVForward(), axis[1] );

			// the normal should point at the viewer
			axis[0] = -GetVForward();

			// Scale the axes by radius
			axis[1] *= curRadius;
			axis[2] *= curRadius;
		}
		else
		{
			// the normal should point at the viewer
			axis[0] = -GetVForward();

			// scale the axes by radius
			axis[1] = GetVLeft() * curRadius;
			axis[2] = GetVUp() * curRadius;
		}

		verts[0] = org + axis[1] - axis[2];
		verts[1] = org + axis[1] + axis[2];
		verts[2] = org - axis[1] + axis[2];
		verts[3] = org - axis[1] - axis[2];
	}

	ClearBounds( absmin, absmax );
	for( int i = 0; i < 4; i++ )
		AddPointToBounds( verts[i], absmin, absmax );
#if 0
	GL_Blend( GL_TRUE );

	if( FBitSet( m_iFlags, FPART_ADDITIVE ))
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	else pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// draw the particle
	GL_Bind( GL_TEXTURE0, m_hTexture );

	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if( FBitSet( m_iFlags, FPART_ADDITIVE ))
		pglColor4f( 1.0f, 1.0f, 1.0f, curAlpha );
	else pglColor4f( curColor.x, curColor.y, curColor.z, curAlpha );

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex3fv( verts[0] );

		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3fv( verts[1] );

		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3fv( verts[2] );

		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3fv( verts[3] );
	pglEnd();
#else
	CTransEntry entry;
	Vector4D partColor;
	int rendermode;

	if( FBitSet( m_iFlags, FPART_ADDITIVE ))
	{
		partColor = Vector4D( 1.0f, 1.0f, 1.0f, curAlpha );
		rendermode = kRenderTransAdd;
	}
	else
	{
		partColor = Vector4D( curColor.x, curColor.y, curColor.z, curAlpha );
		rendermode = kRenderTransTexture;
	}

	entry.SetRenderPrimitive( verts, partColor, m_hTexture, rendermode );
	entry.ComputeViewDistance( absmin, absmax );
	RI->frame.trans_list.AddToTail( entry );
#endif
	return true;
}

CQuakePartSystem :: CQuakePartSystem( void )
{
	memset( m_pParticles, 0, sizeof( CQuakePart ) * MAX_PARTICLES );

	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;
}

CQuakePartSystem :: ~CQuakePartSystem( void )
{
}

void CQuakePartSystem :: Clear( void )
{
	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	for( int i = 0; i < MAX_PARTICLES; i++ )
		m_pParticles[i].pNext = &m_pParticles[i+1];

	m_pParticles[MAX_PARTICLES-1].pNext = NULL;

	m_pAllowParticles = CVAR_REGISTER( "cl_particles", "1", FCVAR_ARCHIVE );
	m_pParticleLod = CVAR_REGISTER( "cl_particle_lod", "0", FCVAR_ARCHIVE );

	// loading TE shaders
	m_hDefaultParticle = FIND_TEXTURE( "*particle" );	// quake particle
	m_hSparks = LOAD_TEXTURE( "gfx/particles/spark", NULL, 0, TF_CLAMP );
	m_hSmoke = LOAD_TEXTURE( "gfx/particles/smoke", NULL, 0, TF_CLAMP );
	m_hWaterSplash = LOAD_TEXTURE( "gfx/particles/splash1", NULL, 0, TF_CLAMP );

	ParsePartInfos( "gfx/particles/effects.txt" );
}

void CQuakePartSystem :: ParsePartInfos( const char *filename )
{
	// we parse our effects each call of VidInit because we need to keep textures and sprites an actual

	ALERT( at_aiconsole, "loading %s\n", filename );

	char *afile = (char *)gEngfuncs.COM_LoadFile( (char *)filename, 5, NULL );

	if( !afile )
	{
		ALERT( at_error, "Cannot open file %s\n", filename );
		return;
	}

	char *pfile = afile;
	char token[256];
	m_iNumPartInfo = 0;

	memset( &m_pPartInfo, 0, sizeof( m_pPartInfo ));

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( m_iNumPartInfo >= MAX_PARTINFOS )
		{
			ALERT ( at_error, "particle effects info limit exceeded %d > %d\n", m_iNumPartInfo, MAX_PARTINFOS );
			break;
		}

		CQuakePartInfo *pCur = &m_pPartInfo[m_iNumPartInfo];

		// read the effect name
		Q_strncpy( pCur->m_szName, token, sizeof( pCur->m_szName ));

		// read opening brace
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] != '{' )
		{
			ALERT( at_error, "found %s when expecting {\n", token );
			break;
		}

		if( !ParsePartInfo( pCur, pfile ))
			break; // something bad happens

	}

	gEngfuncs.COM_FreeFile( afile );
	ALERT( at_aiconsole, "%d effects parsed\n", m_iNumPartInfo );
}

CQuakePartInfo *CQuakePartSystem :: FindPartInfo( const char *name )
{
	for( int i = 0; i <	m_iNumPartInfo; i++ )
	{
		if( !Q_stricmp( m_pPartInfo[i].m_szName, name ))
			return &m_pPartInfo[i];
	}

	return NULL;
}

void CQuakePartSystem :: CreateEffect( const char *name, const Vector &origin, const Vector &normal )
{
	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	if( !name || !*name )
		return;

	CQuakePartInfo *pInfo = FindPartInfo( name );
	int m_hTexture = m_hDefaultParticle;
	CQuakePart src;

	if( !pInfo )
	{
		ALERT( at_warning, "QuakeParticle: couldn't find effect '%s'\n", name );
		return;
	}

	// sparks
	int flags = pInfo->flags;

	if( FBitSet( flags, FPART_NOTWATER ) && POINT_CONTENTS( (float *)&origin ) == CONTENTS_WATER )
		return;

	int count = bound( 1, pInfo->count.Random(), 1024 ); // don't alloc too many particles

	for( int i = 0; i < count; i++ )
	{
		if( pInfo->m_pSprite )
		{
			int frame = bound( 0, pInfo->frame.Random(), pInfo->m_pSprite->numframes - 1 );
			mspriteframe_t *pframe = R_GetSpriteFrame( pInfo->m_pSprite, frame );
			if( pframe ) m_hTexture = pframe->gl_texturenum;
		}
		else m_hTexture = pInfo->m_hTexture;

		if( pInfo->normal == NORMAL_OFFSET || pInfo->normal == NORMAL_OFS_DIR )
		{
			src.m_vecOrigin.x = origin.x + pInfo->offset[0].Random() * normal.x;
			src.m_vecOrigin.y = origin.y + pInfo->offset[1].Random() * normal.y;
			src.m_vecOrigin.z = origin.z + pInfo->offset[2].Random() * normal.z;
		}
		else
		{
			src.m_vecOrigin.x = origin.x + pInfo->offset[0].Random();
			src.m_vecOrigin.y = origin.y + pInfo->offset[1].Random();
			src.m_vecOrigin.z = origin.z + pInfo->offset[2].Random();
		}

		if( pInfo->normal == NORMAL_DIRECTION || pInfo->normal == NORMAL_OFS_DIR )
		{
			src.m_vecVelocity.x = normal.x * pInfo->velocity[0].Random();
			src.m_vecVelocity.y = normal.y * pInfo->velocity[1].Random();
			src.m_vecVelocity.z = normal.z * pInfo->velocity[2].Random();
		}
		else
		{
			src.m_vecVelocity.x = pInfo->velocity[0].Random();
			src.m_vecVelocity.y = pInfo->velocity[1].Random();
			src.m_vecVelocity.z = pInfo->velocity[2].Random();
		}

		src.m_vecAccel.x = pInfo->accel[0].Random();
		src.m_vecAccel.y = pInfo->accel[1].Random();
		src.m_vecAccel.z = pInfo->accel[2].Random();
		src.m_vecColor.x = pInfo->color[0].Random();
		src.m_vecColor.y = pInfo->color[1].Random();
		src.m_vecColor.z = pInfo->color[2].Random();
		src.m_vecColorVelocity.x = pInfo->colorVel[0].Random();
		src.m_vecColorVelocity.y = pInfo->colorVel[1].Random();
		src.m_vecColorVelocity.z = pInfo->colorVel[2].Random();
		src.m_flAlpha = pInfo->alpha.Random();
		src.m_flAlphaVelocity = pInfo->alphaVel.Random();
		src.m_flRadius = pInfo->radius.Random();
		src.m_flRadiusVelocity = pInfo->radiusVel.Random();
		src.m_flLength = pInfo->length.Random();
		src.m_flLengthVelocity = pInfo->lengthVel.Random();
		src.m_flRotation = pInfo->rotation.Random();
		src.m_flBounceFactor = pInfo->bounce.Random();

		if( !AddParticle( &src, m_hTexture, flags ))
			return; // out of particles?
	}
}

bool CQuakePartSystem :: ParseRandomVector( char *&pfile, RandomRange out[3] )
{
	char token[256];

	int i;
	for( i = 0; i < 3 && pfile != NULL; i++ )
	{
		pfile = COM_ParseLine( pfile, token );
		out[i] = RandomRange( token );
	}

	return (i == 3) ? true : false;
}

int CQuakePartSystem :: ParseParticleFlags( char *pfile )
{
	char token[256];
	int iFlags = 0;

	if( !pfile || !*pfile )
		return iFlags;

	while( pfile != NULL )
	{
		pfile = COM_ParseLine( pfile, token );

		if( !Q_stricmp( token, "Bounce" )) 
			iFlags |= FPART_BOUNCE;
		else if( !Q_stricmp( token, "Friction" )) 
			iFlags |= FPART_FRICTION;
		else if( !Q_stricmp( token, "Light" )) 
			iFlags |= FPART_VERTEXLIGHT;
		else if( !Q_stricmp( token, "Stretch" )) 
			iFlags |= FPART_STRETCH;
		else if( !Q_stricmp( token, "Underwater" )) 
			iFlags |= FPART_UNDERWATER;
		else if( !Q_stricmp( token, "Instant" )) 
			iFlags |= FPART_INSTANT;
		else if( !Q_stricmp( token, "Additive" )) 
			iFlags |= FPART_ADDITIVE;
		else if( !Q_stricmp( token, "NotInWater" )) 
			iFlags |= FPART_NOTWATER;
		else if( pfile && token[0] != '|' )
			ALERT( at_warning, "unknown value %s for 'flags'\n", token );
	}

	return iFlags;
}

bool CQuakePartSystem :: ParsePartInfo( CQuakePartInfo *info, char *&pfile )
{
	char token[256];

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile )
		{
			ALERT( at_error, "EOF without closing brace\n" );
			return false;
		}

		// description end goto next material
		if( token[0] == '}' )
		{
			m_iNumPartInfo++;
			return true;
		}
		else if( !Q_stricmp( token, "texture" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'texture'\n" );
				break;
			}

			const char *ext = COM_FileExtension( token );

			if( !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "dds" ))
				info->m_hTexture = LOAD_TEXTURE( token, NULL, 0, TF_CLAMP );
			else if( !Q_stricmp( ext, "spr" ))
				info->m_pSprite = (model_t *)gEngfuncs.GetSpritePointer( SPR_Load( token ));

			if( !info->m_hTexture && !info->m_pSprite )
			{
				ALERT( at_error, "couldn't load texture for effect %s. using default particle\n", info->m_szName );
				info->m_hTexture = m_hDefaultParticle;
			}
		}
		else if( !Q_stricmp( token, "offset" ))
		{
			if( !ParseRandomVector( pfile, info->offset ))
			{
				ALERT( at_error, "hit EOF while parsing 'offset'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "velocity" ))
		{
			if( !ParseRandomVector( pfile, info->velocity ))
			{
				ALERT( at_error, "hit EOF while parsing 'velocity'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "accel" ))
		{
			if( !ParseRandomVector( pfile, info->accel ))
			{
				ALERT( at_error, "hit EOF while parsing 'accel'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "color" ))
		{
			if( !ParseRandomVector( pfile, info->color ))
			{
				ALERT( at_error, "hit EOF while parsing 'color'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "colorVelocity" ))
		{
			if( !ParseRandomVector( pfile, info->colorVel ))
			{
				ALERT( at_error, "hit EOF while parsing 'colorVelocity'\n" );
				break;
			}
		}
		else if( !Q_stricmp( token, "alpha" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'alpha'\n" );
				break;
			}

			info->alpha = RandomRange( token );
		}
		else if( !Q_stricmp( token, "alphaVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'alphaVelocity'\n" );
				break;
			}

			info->alphaVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "radius" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'radius'\n" );
				break;
			}

			info->radius = RandomRange( token );
		}
		else if( !Q_stricmp( token, "radiusVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'radiusVelocity'\n" );
				break;
			}

			info->radiusVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "length" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'length'\n" );
				break;
			}

			info->length = RandomRange( token );
		}
		else if( !Q_stricmp( token, "lengthVelocity" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'lengthVelocity'\n" );
				break;
			}

			info->lengthVel = RandomRange( token );
		}
		else if( !Q_stricmp( token, "rotation" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'rotation'\n" );
				break;
			}

			info->rotation = RandomRange( token );
		}
		else if( !Q_stricmp( token, "bounceFactor" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'bounceFactor'\n" );
				break;
			}

			info->bounce = RandomRange( token );
		}
		else if( !Q_stricmp( token, "frame" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'frame'\n" );
				break;
			}

			info->frame = RandomRange( token );
		}
		else if( !Q_stricmp( token, "count" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'count'\n" );
				break;
			}

			info->count = RandomRange( token );
		}
		else if( !Q_stricmp( token, "flags" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'flags'\n" );
				break;
			}

			info->flags = ParseParticleFlags( token );
		}
		else if( !Q_stricmp( token, "useNormal" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "hit EOF while parsing 'useNormal'\n" );
				break;
			}

			if( !Q_stricmp( token, "ignore" ))
				info->normal = NORMAL_IGNORE;
			else if( !Q_stricmp( token, "ofs" ))
				info->normal = NORMAL_OFFSET;
			else if( !Q_stricmp( token, "dir" ))
				info->normal = NORMAL_DIRECTION;
			else if( !Q_stricmp( token, "ofs+dir" ))
				info->normal = NORMAL_OFS_DIR;				
			else if( !Q_stricmp( token, "dir+ofs" ))
				info->normal = NORMAL_OFS_DIR;
			else ALERT( at_warning, "Unknown 'useNormal' key '%s'\n", token );
		}
		else ALERT( at_warning, "Unknown effects token %s\n", token );
	}

	return true;
}

void CQuakePartSystem :: FreeParticle( CQuakePart *pCur )
{
	pCur->pNext = m_pFreeParticles;
	m_pFreeParticles = pCur;
}

CQuakePart *CQuakePartSystem :: AllocParticle( void )
{
	CQuakePart *pCur;

	if( !m_pFreeParticles )
	{
		ALERT( at_console, "Overflow %d particles\n", MAX_PARTICLES );
		return NULL;
	}

	if( m_pParticleLod->value > 1.0f )
	{
		if( !( RANDOM_LONG( 0, 1 ) % (int)m_pParticleLod->value ))
			return NULL;
	}

	pCur = m_pFreeParticles;
	m_pFreeParticles = pCur->pNext;
	pCur->pNext = m_pActiveParticles;
	m_pActiveParticles = pCur;

	return pCur;
}
	
void CQuakePartSystem :: Update( void )
{
	CQuakePart *pCur, *pNext;
	CQuakePart *pActive = NULL, *pTail = NULL;

	if( !m_pAllowParticles->value )
		return;

	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	GL_DepthMask( GL_FALSE );

	float gravity = tr.frametime * tr.gravity;

	for( pCur = m_pActiveParticles; pCur != NULL; pCur = pNext )
	{
		// grab next now, so if the particle is freed we still have it
		pNext = pCur->pNext;

		if( !pCur->Evaluate( gravity ))
		{
			FreeParticle( pCur );
			continue;
		}

		pCur->pNext = NULL;

		if( !pTail )
		{
			pActive = pTail = pCur;
		}
		else
		{
			pTail->pNext = pCur;
			pTail = pCur;
		}
	}

	m_pActiveParticles = pActive;
}

bool CQuakePartSystem :: AddParticle( CQuakePart *src, int texture, int flags )
{
	if( !src ) return false;

	CQuakePart *dst = AllocParticle();

	if( !dst ) return false;

	if( texture ) dst->m_hTexture = texture;
	else dst->m_hTexture = m_hDefaultParticle;
	dst->m_flTime = tr.time;
	dst->m_iFlags = flags;

	dst->m_vecOrigin = src->m_vecOrigin;
	dst->m_vecVelocity = src->m_vecVelocity;
	dst->m_vecAccel = src->m_vecAccel; 
	dst->m_vecColor = src->m_vecColor;
	dst->m_vecColorVelocity = src->m_vecColorVelocity;
	dst->m_flAlpha = src->m_flAlpha;

	dst->m_flRadius = src->m_flRadius;
	dst->m_flLength = src->m_flLength;
	dst->m_flRotation = src->m_flRotation;
	dst->m_flAlphaVelocity = src->m_flAlphaVelocity;
	dst->m_flRadiusVelocity = src->m_flRadiusVelocity;
	dst->m_flLengthVelocity = src->m_flLengthVelocity;
	dst->m_flBounceFactor = src->m_flBounceFactor;

	// needs to save old origin
	if( FBitSet( flags, ( FPART_BOUNCE|FPART_FRICTION )))
		dst->m_vecLastOrg = dst->m_vecOrigin;

	return true;
}

/*
=================
CL_ExplosionParticles
=================
*/
void CQuakePartSystem :: ExplosionParticles( const Vector &pos )
{
	CQuakePart src;
	int flags;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	int i;
	for( i = 0; i < 384; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_LONG( -16, 16 );
		src.m_vecOrigin.y = pos.y + RANDOM_LONG( -16, 16 );
		src.m_vecOrigin.z = pos.z + RANDOM_LONG( -16, 16 );
		src.m_vecVelocity.x = RANDOM_LONG( -256, 256 );
		src.m_vecVelocity.y = RANDOM_LONG( -256, 256 );
		src.m_vecVelocity.z = RANDOM_LONG( -256, 256 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -60 + RANDOM_FLOAT( -30, 30 );
		src.m_vecColor = Vector( 1, 1, 1 );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlpha = 1.0;
		src.m_flAlphaVelocity = -3.0;
		src.m_flRadius = 0.5 + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flRadiusVelocity = 0;
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flRotation = 0;
		src.m_flBounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

	for( i = 0; i < 5; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.m_vecAccel = Vector( 0, 0, 0 );
		src.m_vecColor = Vector( 0, 0, 0 );
		src.m_vecColorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.m_flRadius = 30 + RANDOM_FLOAT( -15, 15 );
		src.m_flRadiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.m_flLength = 1;
		src.m_flLengthVelocity = 0;
		src.m_flRotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CQuakePartSystem :: SparkParticles( const Vector &org, const Vector &dir )
{
	CQuakePart src;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_vecColor = Vector( 1.0, 1.0f, 1.0f );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlpha = 1.0;
		src.m_flAlphaVelocity = -8.0;
		src.m_flRadius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flRadiusVelocity = 0;
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flRotation = 0;
		src.m_flBounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

/*
=================
CL_RicochetSparks
=================
*/
void CQuakePartSystem :: RicochetSparks( const Vector &org, float scale )
{
	CQuakePart src;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.m_vecOrigin.x = org[0] + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_vecColor = Vector( 1.0, 1.0f, 1.0f );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlpha = 1.0;
		src.m_flAlphaVelocity = -8.0;
		src.m_flRadius = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flRadiusVelocity = 0;
		src.m_flLength = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flLengthVelocity = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flRotation = 0;
		src.m_flBounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

void CQuakePartSystem :: SmokeParticles( const Vector &pos, int count )
{
	CQuakePart src;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	// smoke
	int flags = FPART_VERTEXLIGHT;

	for( int i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -10, 10 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.m_vecAccel = Vector( 0, 0, 0 );
		src.m_vecColor = Vector( 0, 0, 0 );
		src.m_vecColorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.1, -0.15 );
		src.m_flRadius = 30 + RANDOM_FLOAT( -15, 15 );
		src.m_flRadiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.m_flLength = 1;
		src.m_flLengthVelocity = 0;
		src.m_flRotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

void CQuakePartSystem :: GunSmoke( const Vector &pos, int count )
{
	CQuakePart src;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	// smoke
	int flags = FPART_VERTEXLIGHT;

	for( int i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = pos.x + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecOrigin.y = pos.y + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecOrigin.z = pos.z + RANDOM_FLOAT( -0.1f, 0.1f );
		src.m_vecVelocity.x = RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_vecVelocity.y = RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_vecVelocity.z = RANDOM_FLOAT( -5.1f, 5.1f );
		src.m_vecAccel = Vector( 0, 0, 0 );
		src.m_vecColor = Vector( 1.0f, 1.0f, 1.0f );
		src.m_vecColorVelocity = g_vecZero;
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = RANDOM_FLOAT( -0.2f, -0.4f );
		src.m_flRadius = RANDOM_FLOAT( 4.0f, 6.0f );
		src.m_flRadiusVelocity = 2.0f + RANDOM_FLOAT( -0.5, 0.5 );
		src.m_flLength = 1;
		src.m_flLengthVelocity = 0;
		src.m_flRotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CQuakePartSystem :: BulletParticles( const Vector &org, const Vector &dir )
{
	CQuakePart src;
	int cnt, count;

	if( !g_fRenderInitialized || !m_pAllowParticles->value )
		return;

	count = RANDOM_LONG( 3, 8 );
	cnt = POINT_CONTENTS( (float *)&org );

	if( cnt == CONTENTS_WATER )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION|FPART_ADDITIVE);

	int i;
	for( i = 0; i < count; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecVelocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.m_vecAccel.x = src.m_vecAccel.y = 0;
		src.m_vecAccel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.m_vecColor = Vector( 1.0, 1.0f, 1.0f );
		src.m_vecColorVelocity = Vector( 0, 0, 0 );
		src.m_flAlpha = 1.0;
		src.m_flAlphaVelocity = -8.0;
		src.m_flRadius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.m_flRadiusVelocity = 0;
		src.m_flLength = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flLengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.m_flRotation = 0;
		src.m_flBounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

	for( i = 0; i < 3; i++ )
	{
		src.m_vecOrigin.x = org[0] + dir[0] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.y = org[1] + dir[1] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecOrigin.z = org[2] + dir[2] * 5 + RANDOM_FLOAT( -1, 1 );
		src.m_vecVelocity.x = RANDOM_FLOAT( -2.5, 2.5 );
		src.m_vecVelocity.y = RANDOM_FLOAT( -2.5, 2.5 );
		src.m_vecVelocity.z = RANDOM_FLOAT( -2.5, 2.5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.m_vecAccel = Vector( 0, 0, 0 );
		src.m_vecColor = Vector( 0.4, 0.4, 0.4 );
		src.m_vecColorVelocity = Vector( 0.2, 0.2, 0.2 );
		src.m_flAlpha = 0.5;
		src.m_flAlphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.m_flRadius = 3 + RANDOM_FLOAT( -1.5, 1.5 );
		src.m_flRadiusVelocity = 5 + RANDOM_FLOAT( -2.5, 2.5 );
		src.m_flLength = 1;
		src.m_flLengthVelocity = 0;
		src.m_flRotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}