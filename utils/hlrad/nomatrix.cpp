#include "qrad.h"

// =====================================================================================
//  CheckVisBit
// =====================================================================================
static bool     CheckVisBitNoVismatrix(unsigned patchnum1, unsigned patchnum2
#ifdef HLRAD_HULLU
									   , vec3_t &transparency_out
#ifdef HLRAD_TRANSPARENCY_CPP
									   , unsigned int &
#endif
#endif
									   )
	// patchnum1=receiver, patchnum2=emitter. //HLRAD_CheckVisBitNoVismatrix_NOSWAP
{
#ifdef HLRAD_HULLU
#ifndef HLRAD_CheckVisBitNoVismatrix_NOSWAP
    // This fix was in vismatrix and sparse methods but not in nomatrix
    // Without this nomatrix causes SwapTransfers output lots of errors
    if (patchnum1 > patchnum2)
    {
        const unsigned a = patchnum1;
        const unsigned b = patchnum2;
        patchnum1 = b;
        patchnum2 = a;
    }
#endif
    
    if (patchnum1 > g_num_patches)
    {
        Warning("in CheckVisBit(), patchnum1 > num_patches");
    }
    if (patchnum2 > g_num_patches)
    {
        Warning("in CheckVisBit(), patchnum2 > num_patches");
    }
#endif
	
    patch_t*        patch = &g_patches[patchnum1];
    patch_t*        patch2 = &g_patches[patchnum2];

#ifdef HLRAD_HULLU
    VectorFill(transparency_out, 1.0);
#endif

    // if emitter is behind that face plane, skip all patches

    if (patch2)
    {
        const dplane_t* plane2 = getPlaneFromFaceNumber(patch2->faceNumber);

#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
		if (DotProduct (patch->origin, plane2->normal) > PatchPlaneDist (patch2) + ON_EPSILON - patch->emitter_range)
#else
        if (DotProduct(patch->origin, plane2->normal) > (PatchPlaneDist(patch2) + MINIMUM_PATCH_DISTANCE))
#endif
        {
            // we need to do a real test

            const dplane_t* plane = getPlaneFromFaceNumber(patch->faceNumber);

#ifdef HLRAD_HULLU
            vec3_t transparency = {1.0,1.0,1.0};
#endif
#ifdef HLRAD_OPAQUE_STYLE
			int opaquestyle = -1;
#endif

            // check vis between patch and patch2
            //  if v2 is not behind light plane
            //  && v2 is visible from v1
#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			vec3_t origin1, origin2;
			vec3_t delta;
			vec_t dist;
			VectorSubtract (patch->origin, patch2->origin, delta);
			dist = VectorLength (delta);
			if (dist < patch2->emitter_range - ON_EPSILON)
			{
				GetAlternateOrigin (patch->origin, plane->normal, patch2, origin2);
			}
			else
			{
				VectorCopy (patch2->origin, origin2);
			}
			if (DotProduct (origin2, plane->normal) <= PatchPlaneDist (patch) + MINIMUM_PATCH_DISTANCE)
			{
				return false;
			}
			if (dist < patch->emitter_range - ON_EPSILON)
			{
				GetAlternateOrigin (patch2->origin, plane2->normal, patch, origin1);
			}
			else
			{
				VectorCopy (patch->origin, origin1);
			}
			if (DotProduct (origin1, plane2->normal) <= PatchPlaneDist (patch2) + MINIMUM_PATCH_DISTANCE)
			{
				return false;
			}
#else
            if (DotProduct(patch2->origin, plane->normal) <= (PatchPlaneDist(patch) + MINIMUM_PATCH_DISTANCE))
			{
				return false;
			}
#endif
#ifdef HLRAD_WATERBLOCKLIGHT
            if (TestLine(
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				origin1, origin2
	#else
				patch->origin, patch2->origin
	#endif
				) != CONTENTS_EMPTY)
#else
            if (TestLine_r(0, 
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				origin1, origin2
	#else
				patch->origin, patch2->origin
	#endif
				) != CONTENTS_EMPTY)
#endif
			{
				return false;
			}
            if (TestSegmentAgainstOpaqueList(
#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				origin1, origin2
#else
				patch->origin, patch2->origin
#endif
#ifdef HLRAD_HULLU
				, transparency
#endif
#ifdef HLRAD_OPAQUE_STYLE
				, opaquestyle
#endif
				))
			{
				return false;
			}

            {
#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
				if (opaquestyle != -1)
				{
					AddStyleToStyleArray (patchnum1, patchnum2, opaquestyle);
				}
#endif
#ifdef HLRAD_HULLU            	
            	if(g_customshadow_with_bouncelight)
            	{
            		VectorCopy(transparency, transparency_out);
            	}
#endif
                return true;
            }
        }
    }

    return false;
}
#ifdef HLRAD_TRANSLUCENT
       bool     CheckVisBitBackwards(unsigned receiver, unsigned emitter, const vec3_t &backorigin, const vec3_t &backnormal
#ifdef HLRAD_HULLU
									   , vec3_t &transparency_out
#endif
									   )
{	
    patch_t*        patch = &g_patches[receiver];
    patch_t*        emitpatch = &g_patches[emitter];

#ifdef HLRAD_HULLU
    VectorFill(transparency_out, 1.0);
#endif

    if (emitpatch)
    {
        const dplane_t* emitplane = getPlaneFromFaceNumber(emitpatch->faceNumber);

        if (DotProduct(backorigin, emitplane->normal) > (PatchPlaneDist(emitpatch) + MINIMUM_PATCH_DISTANCE))
        {

#ifdef HLRAD_HULLU
            vec3_t transparency = {1.0,1.0,1.0};
#endif
#ifdef HLRAD_OPAQUE_STYLE
			int opaquestyle = -1;
#endif

#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			vec3_t emitorigin;
			vec3_t delta;
			vec_t dist;
			VectorSubtract (backorigin, emitpatch->origin, delta);
			dist = VectorLength (delta);
			if (dist < emitpatch->emitter_range - ON_EPSILON)
			{
				GetAlternateOrigin (backorigin, backnormal, emitpatch, emitorigin);
			}
			else
			{
				VectorCopy (emitpatch->origin, emitorigin);
			}
			if (DotProduct (emitorigin, backnormal) <= DotProduct (backorigin, backnormal) + MINIMUM_PATCH_DISTANCE)
			{
				return false;
			}
#else
            if (DotProduct(emitpatch->origin, backnormal) <= (DotProduct(backorigin, backnormal) + MINIMUM_PATCH_DISTANCE))
			{
				return false;
			}
#endif
#ifdef HLRAD_WATERBLOCKLIGHT
            if (TestLine(
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				backorigin, emitorigin
	#else
				backorigin, emitpatch->origin
	#endif
				) != CONTENTS_EMPTY)
#else
            if (TestLine_r(0, 
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				backorigin, emitorigin
	#else
				backorigin, emitpatch->origin
	#endif
				) != CONTENTS_EMPTY)
#endif
			{
				return false;
			}
            if (TestSegmentAgainstOpaqueList(
#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				backorigin, emitorigin
#else
				backorigin, emitpatch->origin
#endif
#ifdef HLRAD_HULLU
				, transparency
#endif
#ifdef HLRAD_OPAQUE_STYLE
				, opaquestyle
#endif
				))
			{
				return false;
			}

            {
#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
				if (opaquestyle != -1)
				{
					AddStyleToStyleArray (receiver, emitter, opaquestyle);
				}
#endif
#ifdef HLRAD_HULLU            	
            	if(g_customshadow_with_bouncelight)
            	{
            		VectorCopy(transparency, transparency_out);
            	}
#endif
                return true;
            }
        }
    }

    return false;
}
#endif

//
// end old vismat.c
////////////////////////////

void            MakeScalesNoVismatrix()
{
    char            transferfile[_MAX_PATH];

    hlassume(g_num_patches < MAX_PATCHES, assume_MAX_PATCHES);

#ifdef ZHLT_DEFAULTEXTENSION_FIX
	safe_snprintf(transferfile, _MAX_PATH, "%s.inc", g_Mapname);
#else
    safe_strncpy(transferfile, g_source, _MAX_PATH);
    StripExtension(transferfile);
    DefaultExtension(transferfile, ".inc");
#endif

    if (!g_incremental || !readtransfers(transferfile, g_num_patches))
    {
        g_CheckVisBit = CheckVisBitNoVismatrix;
#ifndef HLRAD_HULLU
        NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);
#else
	if(g_rgb_transfers)
		{NamedRunThreadsOn(g_num_patches, g_estimate, MakeRGBScales);}
	else
		{NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);}
#endif

#ifndef HLRAD_NOSWAP
        // invert the transfers for gather vs scatter
#ifndef HLRAD_HULLU
        NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapTransfers);
#else
	if(g_rgb_transfers)
		{NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapRGBTransfers);}
	else
		{NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapTransfers);}
#endif
#endif /*HLRAD_NOSWAP*/
        if (g_incremental)
        {
            writetransfers(transferfile, g_num_patches);
        }
        else
        {
            unlink(transferfile);
        }
        DumpTransfersMemoryUsage();
#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
		CreateFinalStyleArrays ("dynamic shadow array");
#endif
    }
}

