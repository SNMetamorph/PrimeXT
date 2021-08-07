//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "utils.h"
#include "wrect.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "custom_alloc.h"
#include "com_model.h"
#include "gl_local.h"// buz

extern "C"
{
	void DLLEXPORT HUD_DrawNormalTriangles( void );
	void DLLEXPORT HUD_DrawTransparentTriangles( void );
};

//
//-----------------------------------------------------
//
/*
====================
buz:
Orthogonal polygons
====================
*/
// helper functions
void OrthoQuad(int x1, int y1, int x2, int y2)
{
	gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad

		gEngfuncs.pTriAPI->TexCoord2f(0.01, 0.01);
		gEngfuncs.pTriAPI->Vertex3f(x1, y1, 0);

		gEngfuncs.pTriAPI->TexCoord2f(0.01, 0.99);
		gEngfuncs.pTriAPI->Vertex3f(x1, y2, 0);

		gEngfuncs.pTriAPI->TexCoord2f(0.99, 0.99);
		gEngfuncs.pTriAPI->Vertex3f(x2, y2, 0);

		gEngfuncs.pTriAPI->TexCoord2f(0.99f, 0.01);
		gEngfuncs.pTriAPI->Vertex3f(x2, y1, 0);

	gEngfuncs.pTriAPI->End(); //end our list of vertexes
}

// buz: use triapi to draw sprites on screen
void DrawSpriteAsPoly( SpriteHandle hspr, wrect_t *rect, wrect_t *screenpos, int mode, float r, float g, float b, float a )
{
	if( !hspr ) return;

	const struct model_s *sprmodel = gEngfuncs.GetSpritePointer(hspr);
	gEngfuncs.pTriAPI->RenderMode(mode);
	gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 0);
	gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
	gEngfuncs.pTriAPI->Color4f( r, g, b, a );

	float x = rect->left / (float)SPR_Width(hspr, 0) + 0.01;
	float x2 = rect->right / (float)SPR_Width(hspr, 0) - 0.01;
	float y = rect->top / (float)SPR_Height(hspr, 0) + 0.01;
	float y2 = rect->bottom / (float)SPR_Height(hspr, 0) - 0.01;

	gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f(x, y);
		gEngfuncs.pTriAPI->Vertex3f(screenpos->left, screenpos->top, 0);

		gEngfuncs.pTriAPI->TexCoord2f(x, y2);
		gEngfuncs.pTriAPI->Vertex3f(screenpos->left, screenpos->bottom, 0);

		gEngfuncs.pTriAPI->TexCoord2f(x2, y2);
		gEngfuncs.pTriAPI->Vertex3f(screenpos->right, screenpos->bottom, 0);

		gEngfuncs.pTriAPI->TexCoord2f(x2, y);
		gEngfuncs.pTriAPI->Vertex3f(screenpos->right, screenpos->top, 0);
	gEngfuncs.pTriAPI->End();

	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

void OrthoDraw( SpriteHandle spr, int mode, float r, float g, float b, float a )
{
	if( !spr ) return;

	const struct model_s *sprmodel = gEngfuncs.GetSpritePointer(spr);
	gEngfuncs.pTriAPI->RenderMode(mode);

	int frames = SPR_Frames(spr);

	switch (frames)
	{
	case 1:
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 0);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(0, 0, ScreenWidth, ScreenHeight);
		break;

	case 2:
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 0);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(0, 0, ScreenWidth/2, ScreenHeight);

		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 1);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(ScreenWidth/2, 0, ScreenWidth, ScreenHeight);
		break;

	case 4:
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 0);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(0, 0, ScreenWidth/2, ScreenHeight/2);

		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 1);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(ScreenWidth/2, 0, ScreenWidth, ScreenHeight/2);

		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 2);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(0, ScreenHeight/2, ScreenWidth/2, ScreenHeight);

		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *) sprmodel, 3);
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); //no culling
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		OrthoQuad(ScreenWidth/2, ScreenHeight/2, ScreenWidth, ScreenHeight);
		break;

	default:
		gEngfuncs.Con_Printf("ERROR: illegal number of frames in ortho sprite (%i)\n", frames);
		break;
	}

	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

void OrthoScope( void )
{
	// scope base
	SpriteHandle spr1 = SPR_Load("sprites/sniper1.spr");
	if (!spr1)
	{
		gEngfuncs.Con_Printf("ERROR: Can't load sprites/sniper1.spr\n");
		return;
	}

	// dirt decal
	SpriteHandle spr2 = SPR_Load("sprites/sniper2.spr");
	if (!spr2)
	{
		gEngfuncs.Con_Printf("ERROR: Can't load sprites/sniper2.spr\n");
		return;
	}

	// transparent glass
	SpriteHandle spr3 = SPR_Load("sprites/sniper3.spr");
	if (!spr3)
	{
		gEngfuncs.Con_Printf("ERROR: Can't load sprites/sniper3.spr\n");
		return;
	}

	OrthoDraw( spr3, kRenderTransAdd, tr.ambientLight.x * 0.5f, tr.ambientLight.y * 0.5f, tr.ambientLight.z * 0.5f, 1.0f );
	OrthoDraw( spr2, kRenderTransColor, tr.ambientLight.x, tr.ambientLight.y, tr.ambientLight.z, 1.0f );
	OrthoDraw( spr1, kRenderNormal, tr.ambientLight.x * 0.5f, tr.ambientLight.y * 0.5f, tr.ambientLight.z * 0.5f, 1.0f );

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal); //return to normal
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles( void )
{
}

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
}
