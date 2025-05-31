//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include "gl_shader.h"
#include "gl_cvars.h"
#include "gl_debug.h"

#define MAX_CLIP_VERTS	128	// skybox clip vertices

static const int		r_skyTexOrder[6] = { 0, 2, 1, 3, 4, 5 };

static const Vector	skyclip[6] = 
{
Vector(  1,  1,  0 ),
Vector(  1, -1,  0 ),
Vector(  0, -1,  1 ),
Vector(  0,  1,  1 ),
Vector(  1,  0,  1 ),
Vector( -1,  0,  1 ) 
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3] =
{
{  3, -1,  2 },
{ -3,  1,  2 },
{  1,  3,  2 },
{ -1, -3,  2 },
{ -2, -1,  3 },  // 0 degrees yaw, look straight up
{  2, -1, -3 }   // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] =
{
{ -2,  3,  1 },
{  2,  3, -1 },
{  1,  3,  2 },
{ -1,  3, -2 },
{ -2, -1,  3 },
{ -2,  1, -3 }
};

static void DrawSkyPolygon( int nump, Vector vecs[] )
{
	int	i, j, axis;
	float	s, t, dv;
	Vector	v, av;

	// decide which face it maps to
	v = g_vecZero;

	for( i = 0; i < nump; i++ )
		v += vecs[i];

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	if( av[0] > av[1] && av[0] > av[2] )
		axis = (v[0] < 0) ? 1 : 0;
	else if( av[1] > av[2] && av[1] > av[0] )
		axis = (v[1] < 0) ? 3 : 2;
	else axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for( i = 0; i < nump; i++ )
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? (vecs[i])[j-1] : -(vecs[i])[-j-1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[i][-j-1] / dv : (vecs[i])[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -(vecs[i])[-j-1] / dv : (vecs[i])[j-1] / dv;

		if( s < RI->view.skyMins[0][axis] ) RI->view.skyMins[0][axis] = s;
		if( t < RI->view.skyMins[1][axis] ) RI->view.skyMins[1][axis] = t;
		if( s > RI->view.skyMaxs[0][axis] ) RI->view.skyMaxs[0][axis] = s;
		if( t > RI->view.skyMaxs[1][axis] ) RI->view.skyMaxs[1][axis] = t;
	}
}

/*
==============
ClipSkyPolygon
==============
*/
static void ClipSkyPolygon( int nump, Vector vecs[], int stage )
{
	bool	front, back;
	float	dists[MAX_CLIP_VERTS + 1];
	int	sides[MAX_CLIP_VERTS + 1];
	Vector	newv[2][MAX_CLIP_VERTS + 1];
	int	newc[2];
	float	d, e;
	int	i;

	if( nump > MAX_CLIP_VERTS )
		HOST_ERROR( "ClipSkyPolygon: MAX_CLIP_VERTS\n" );
loc1:
	if( stage == 6 )
	{	
		// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	const Vector &norm = skyclip[stage];

	for( i = 0; i < nump; i++ )
	{
		d = DotProduct( vecs[i], norm );
		if( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if( d < -ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if( !front || !back )
	{	
		// not clipped
		stage++;
		goto loc1;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	vecs[i] = vecs[0];
	newc[0] = newc[1] = 0;

	for( i = 0; i < nump; i++ )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			newv[0][newc[0]] = vecs[i];
			newc[0]++;
			break;
		case SIDE_BACK:
			newv[1][newc[1]] = vecs[i];
			newc[1]++;
			break;
		case SIDE_ON:
			newv[0][newc[0]] = vecs[i];
			newc[0]++;
			newv[1][newc[1]] = vecs[i];
			newc[1]++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i+1] );

		for( int j = 0; j < 3; j++ )
		{
			e = (vecs[i])[j] + d * ( (vecs[i+1])[j] - (vecs[i])[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1], stage + 1 );
}

static void MakeSkyVec( float s, float t, int axis )
{
	int	j, k, farclip;
	Vector	v, b;

	farclip = RI->view.farClip;

	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += GetVieworg()[j];
	}

	// avoid bilerp seam
	s = (s + 1.0f) * 0.5f;
	t = (t + 1.0f) * 0.5f;

	if( s < ( 1.0f / 512.0f ))
		s = (1.0f / 512.0f);
	else if( s > ( 511.0f / 512.0f ))
		s = (511.0f / 512.0f);
	if( t < ( 1.0f / 512.0f ))
		t = (1.0f / 512.0f);
	else if( t > ( 511.0f / 512.0f ))
		t = (511.0f / 512.0f);

	t = 1.0f - t;

	pglTexCoord2f( s, t );
	pglVertex3fv( v );
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface( msurface_t *fa )
{
	Vector verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	for( glpoly_t *p = fa->polys; p; p = p->next )
	{
		if( p->numverts >= MAX_CLIP_VERTS )
			HOST_ERROR( "R_AddSkyBoxSurface: numverts >= MAX_CLIP_VERTS\n" );

		for( int i = 0; i < p->numverts; i++ )
		{
			verts[i][0] = p->verts[i][0] - GetVieworg().x;
			verts[i][1] = p->verts[i][1] - GetVieworg().y;
			verts[i][2] = p->verts[i][2] - GetVieworg().z;
                    }

		ClipSkyPolygon( p->numverts, verts, 0 );
	}
}

static void GL_DrawSkySide( int skyside )
{
	pglBegin( GL_QUADS );
		MakeSkyVec( RI->view.skyMins[0][skyside], RI->view.skyMins[1][skyside], skyside );
		MakeSkyVec( RI->view.skyMins[0][skyside], RI->view.skyMaxs[1][skyside], skyside );
		MakeSkyVec( RI->view.skyMaxs[0][skyside], RI->view.skyMaxs[1][skyside], skyside );
		MakeSkyVec( RI->view.skyMaxs[0][skyside], RI->view.skyMins[1][skyside], skyside );
	pglEnd();
}

static void GL_DrawSkySide( word hProgram, int skyside )
{
	if( hProgram <= 0 )
	{
		GL_BindShader( NULL );
		GL_Bind( GL_TEXTURE0, tr.skyboxTextures[r_skyTexOrder[skyside]] );
		GL_DrawSkySide( skyside );
		return; // old method
	}

	if( RI->currentshader != &glsl_programs[hProgram] )
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[hProgram] );
	}

	Vector sky_color = (tr.sun_light_enabled) ? tr.sun_diffuse : tr.sky_ambient * (1.0f/128.0f) * tr.diffuseFactor;
	Vector sky_vec = tr.sky_normal.Normalize();
	
	if (sky_vec.Length() < 0.01f) {
		// placeholder sun direction here
		sky_vec = Vector(0.f, 0.f, 1.f);
	}

	// setup specified uniforms (and texture bindings)
	glsl_program_t *shader = RI->currentshader;
	for (int i = 0; i < shader->numUniforms; i++)
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			u->SetValue( tr.skyboxTextures[r_skyTexOrder[skyside]].ToInt() );
			break;
		case UT_LIGHTDIR:
			u->SetValue( sky_vec.x, sky_vec.y, sky_vec.z );
			break;
		case UT_LIGHTDIFFUSE:
			u->SetValue( sky_color.x, sky_color.y, sky_color.z );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( GetVieworg().x, GetVieworg().y, GetVieworg().z );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity * 0.5f );
			break;
		case UT_ZFAR:
			u->SetValue( RI->view.farClip );
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}

	GL_DrawSkySide( skyside );
}

/*
==============
R_DrawSkybox
==============
*/
void R_DrawSkyBox()
{
	bool	drawSun = true;
	float	fogDenstity = tr.fogDensity;
	word	hSkyShader = 0;

	if (!FBitSet( RI->view.flags, RF_SKYVISIBLE ) || tr.ignore_2d_skybox)
		return;

	GL_DEBUG_SCOPE();
	GL_DepthMask( GL_FALSE );
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );

	// clipplane cut the sky if drawing through mirror. Disable it
	GL_ClipPlane( false );

	if( FBitSet( RI->params, RP_SKYVIEW ) || ( FBitSet( RI->params, RP_WATERPASS ) && CVAR_TO_BOOL( cv_specular )))
		drawSun = false;

	// disable sky fogging while underwater
	if( tr.waterlevel >= 3 || FBitSet( RI->params, RP_SKYVIEW ))
		fogDenstity = 0.0f;

	if (drawSun)
	{
		int type = tr.sun_light_enabled ? 1 : 0;
		hSkyShader = tr.skyboxEnv[type];
	}

	for (int i = 0; i < 6; i++)
	{
		if( RI->view.skyMins[0][i] >= RI->view.skyMaxs[0][i] || RI->view.skyMins[1][i] >= RI->view.skyMaxs[1][i] )
			continue;

		GL_DrawSkySide( hSkyShader, i );
	}

	GL_ClipPlane( true );
	GL_DepthMask( GL_TRUE );
}

void R_DrawSkyPortal(cl_entity_t *skyPortal)
{
	ZoneScoped;

	ref_viewpass_t rvp;
	const float skyFov = skyPortal->curstate.fuser2;
	const float skyScale = skyPortal->curstate.scale;

	GL_DEBUG_SCOPE();
	R_PushRefState();

	rvp.flags = RI->params | RP_SKYPORTALVIEW;
	ClearBits(rvp.flags, RP_CLIPPLANE);
	rvp.viewentity = 0;
	rvp.vieworigin = RI->view.origin;
	rvp.viewangles = RI->view.angles + skyPortal->curstate.angles; // angle offset

	RI->view.frustum.DisablePlane(FRUSTUM_FAR);
	RI->view.frustum.DisablePlane(FRUSTUM_NEAR);
	RI->view.pvspoint = skyPortal->curstate.origin;
	RI->view.port.WriteToArray(rvp.viewport);

	if (skyScale > 0.0f)
	{
		float scale = 1.0f / skyScale;
		rvp.vieworigin = RI->view.origin * scale;
		rvp.vieworigin += skyPortal->curstate.origin;
	}
	else {
		rvp.vieworigin = skyPortal->curstate.origin;
	}
	
	if (skyFov)
	{
		rvp.fov_x = skyFov;
		rvp.fov_y = V_CalcFov(RI->view.fov_x, RI->view.port.GetWidth(), RI->view.port.GetHeight());
	}
	else {
		rvp.fov_x = RI->view.fov_x;
		rvp.fov_y = RI->view.fov_y;
	}

	r_stats.c_sky_passes++;
	R_RenderScene(&rvp, (RefParams)rvp.flags);
	R_PopRefState();
}

void R_CheckSkyPortal(cl_entity_t *skyPortal)
{
	tr.ignore_2d_skybox = false;

	if (tr.sky_camera == NULL)
		return;

	if (!CVAR_TO_BOOL(r_allow_3dsky))
		return;

	if (FBitSet(RI->params, RP_DRAW_OVERVIEW))
		return;

	// don't allow recursive 3d sky
	if (FBitSet(RI->params, RP_SKYPORTALVIEW))
		return;

	if (FBitSet(RI->params, RP_MIRRORVIEW))
		tr.modelorg = RI->view.pvspoint;
	else 
		tr.modelorg = RI->view.origin;

	RI->currententity = GET_ENTITY(0);
	RI->currentmodel = RI->currententity->model;

	if (FBitSet(RI->view.flags, RF_SKYVISIBLE))
	{
		R_DrawSkyPortal(skyPortal);
		tr.ignore_2d_skybox = true;
	}
}
