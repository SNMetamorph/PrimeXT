//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#ifndef R_VIEW_H
#define R_VIEW_H 

void V_Init( void );
void V_StartPitchDrift( void );
void V_StopPitchDrift( void );

void V_CalcFirstPersonRefdef( struct ref_params_s *pparams );

#endif//R_VIEW_H
