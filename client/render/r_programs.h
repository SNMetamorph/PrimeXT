/*
r_programs.h - compiled CG programs
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
#ifndef R_PROGRAMS_H
#define R_PROGRAMS_H

const char fp_screen_source[] = 
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"PARAM c0 = {0.32000000, 0.59000000, 0.090000000, 0};\n"
"TEMP R0;\n"
"TXP R0, fragment.texcoord[0], texture[0], 2D;\n"
"DP3 result.color, c0, R0;\n"
"MOV result.color.w, fragment.color.w;\n"
"END";

const char fp_shadow_source[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"PARAM c[5] = {"
"{0, -0.001953125},"
"{-0.001953125, 0},"
"{0.001953125, 0},"
"{0, 0.001953125},"
"{5, 1}};"
"TEMP R0;"
"TEMP R1;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R0.w, R0, texture[2], SHADOW2D;"
"ADD R1.xyz, R0, c[0];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[1];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[2];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[3];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"RCP R1.w, c[4].x;"
"MUL R1.w, R0.w, R1.w;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL result.color.xyz, fragment.color.primary, R1;"
"MOV result.color.w, c[4].y;"
"END";

const char fp_decal0_source[] =
"!!ARBfp1.0"
"TEMP R0;"
"TEMP R1;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"TXP R1, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R0, R1;"
"MUL R1, fragment.color.primary, R1;"
"TEX R0, fragment.texcoord[3], texture[3], 2D;"
"MUL R0.xyz, R0, R0.w;"
"MUL result.color.xyz, R0, R1;"
"MOV result.color.w, 1.0;"
"END";

const char fp_decal1_source[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"TEMP R0;"
"TEMP R1;"
"TEMP R3;"
"TEMP R4;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R1.w, R0, texture[2], SHADOW2D;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL R3.xyz, fragment.color.primary, R1;"
"TEX R4, fragment.texcoord[3], texture[3], 2D;"
"MUL R4.xyz, R4, R4.w;"
"MUL result.color.xyz, R3, R4;"
"MOV result.color.w, 1.0;"
"END";

const char fp_decal2_source[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"PARAM c[5] = {"
"{0, -0.00390625},"
"{-0.00390625, 0},"
"{0.00390625, 0},"
"{0, 0.00390625},"
"{5, 1}};"
"TEMP R0;"
"TEMP R1;"
"TEMP R3;"
"TEMP R4;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R0.w, R0, texture[2], SHADOW2D;"
"ADD R1.xyz, R0, c[0];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[1];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[2];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[3];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"RCP R1.w, c[4].x;"
"MUL R1.w, R0.w, R1.w;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL R3.xyz, fragment.color.primary, R1;"
"TEX R4, fragment.texcoord[3], texture[3], 2D;"
"MUL R4.xyz, R4, R4.w;"
"MUL result.color.xyz, R3, R4;"
"MOV result.color.w, c[4].y;"
"END";

const char fp_decal3_source[] =
"!!ARBfp1.0"
"TEMP R0;"
"TEMP R1;"
"TXP R0, fragment.texcoord[0], texture[0], CUBE;"
"TXP R1, fragment.texcoord[1], texture[1], 3D;"
"MUL R1, R0, R1;"
"MUL R1, fragment.color.primary, R1;"
"TEX R0, fragment.texcoord[3], texture[3], 2D;"
"MUL R0.xyz, R0, R0.w;"
"MUL result.color.xyz, R0, R1;"
"MOV result.color.w, 1.0;"
"END";

const char fp_liquid_source[ ] =
"!!ARBfp1.0\n"
"TEMP R0;\n"
"TEX R0, fragment.texcoord[1], texture[1], 3D;\n"
"MAD R0, R0, 2.0, fragment.texcoord[0];\n"
"TXP result.color.xyz, R0, texture[0], 2D;\n"
"DP3 R0.w, fragment.texcoord[2], fragment.texcoord[2];\n"
"RSQ R0.w, R0.w;\n"
"MUL R0.xyz, R0.w, fragment.texcoord[2];\n"
"DP3 R0.w, R0, fragment.texcoord[3];\n"
"ADD R0.w, R0.w, 1.0;\n"
"MAD result.color.w, R0.w, 0.8, 0.1;\n"
"END\n";

#endif//R_PROGRAMS_H
