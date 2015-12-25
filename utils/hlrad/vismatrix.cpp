#include "qrad.h"

////////////////////////////
// begin old vismat.c
//
#define	HALFBIT

// =====================================================================================
//
//      VISIBILITY MATRIX
//      Determine which patches can see each other
//      Use the PVS to accelerate if available
//
// =====================================================================================

static byte*    s_vismatrix;



#ifndef HLRAD_TRANSPARENCY_CPP
#ifdef HLRAD_HULLU
// =====================================================================================
//      OPACITY ARRAY
// =====================================================================================

typedef struct {
	unsigned bitpos;
	vec3_t transparency;
} transparency_t;

static transparency_t *s_transparency_list = NULL;
static unsigned long s_transparency_count = 0;
static unsigned long s_max_transparency_count=0;

static void FindOpacity(const unsigned bitpos, vec3_t &out)
{
	for(unsigned long i = 0; i < s_transparency_count; i++)
	{
		if( s_transparency_list[i].bitpos == bitpos )
		{
			VectorCopy(s_transparency_list[i].transparency, out);
			return;
		}
	}
	VectorFill(out, 1.0);
}

#endif /*HLRAD_HULLU*/
#endif

// =====================================================================================
//  TestPatchToFace
//      Sets vis bits for all patches in the face
// =====================================================================================
static void     TestPatchToFace(const unsigned patchnum, const int facenum, const int head, const unsigned int bitpos
#ifdef HLRAD_ENTITYBOUNCE_FIX
								, byte *pvs
#endif
								)
{
    patch_t*        patch = &g_patches[patchnum];
    patch_t*        patch2 = g_face_patches[facenum];

    // if emitter is behind that face plane, skip all patches

    if (patch2)
    {
        const dplane_t* plane2 = getPlaneFromFaceNumber(facenum);

#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
		if (DotProduct (patch->origin, plane2->normal) > PatchPlaneDist (patch2) + ON_EPSILON - patch->emitter_range)
#else
        if (DotProduct(patch->origin, plane2->normal) > (PatchPlaneDist(patch2) + MINIMUM_PATCH_DISTANCE))
#endif
        {
            // we need to do a real test
            const dplane_t* plane = getPlaneFromFaceNumber(patch->faceNumber);

            for (; patch2; patch2 = patch2->next)
            {
                unsigned        m = patch2 - g_patches;

#ifdef HLRAD_HULLU
				vec3_t 		transparency = {1.0, 1.0, 1.0};
#endif
#ifdef HLRAD_OPAQUE_STYLE
				int opaquestyle = -1;
#endif
		
                // check vis between patch and patch2
                // if bit has not already been set
                //  && v2 is not behind light plane
                //  && v2 is visible from v1
                if (m > patchnum)
				{
#ifdef HLRAD_ENTITYBOUNCE_FIX
					if (patch2->leafnum == 0 || !(pvs[(patch2->leafnum - 1) >> 3] & (1 << ((patch2->leafnum - 1) & 7))))
					{
						continue;
					}
#endif
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
						continue;
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
						continue;
					}
#else
					if (DotProduct(patch2->origin, plane->normal) <= (PatchPlaneDist(patch) + MINIMUM_PATCH_DISTANCE))
					{
						continue;
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
                    if (TestLine_r(head, 
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
						origin1, origin2
	#else
						patch->origin, patch2->origin
	#endif
						) != CONTENTS_EMPTY)
#endif
					{
						continue;
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
						continue;
					}

#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
					if (opaquestyle != -1)
					{
						AddStyleToStyleArray (m, patchnum, opaquestyle);
						AddStyleToStyleArray (patchnum, m, opaquestyle);
					}
#endif
                    //Log("SDF::3\n");

                    // patchnum can see patch m
                    unsigned        bitset = bitpos + m;

#ifdef HLRAD_HULLU
	#ifdef HLRAD_TRANSPARENCY_CPP
                    if(g_customshadow_with_bouncelight && !VectorCompare(transparency, vec3_one))
					// zhlt3.4: if(g_customshadow_with_bouncelight && VectorCompare(transparency, vec3_one)) . --vluzacn
                    {
						AddTransparencyToRawArray(patchnum, m, transparency);
                    }
	#else
                    // transparency face fix table
                    // TODO: this method makes MakeScale extreamly slow.. find new one
                    if(g_customshadow_with_bouncelight && fabs(VectorAvg(transparency) - 1.0) < 0.001)
						//wrong? (and in sparse) --vluzacn
                    {
                    	while(s_transparency_count >= s_max_transparency_count)
                    	{
                    	    //new size
                    	    unsigned long old_max = s_max_transparency_count;
                    	    s_max_transparency_count += 128;
                    	    
                    	    //realloc
                    	    s_transparency_list = (transparency_t*)realloc(s_transparency_list, s_max_transparency_count * sizeof(transparency_t));
                    	    
                    	    // clean new memory
                            memset(&s_transparency_list[old_max], 0, sizeof(transparency_t) * 128);
                    	}
                    	
                    	//add to array
                    	VectorCopy(transparency, s_transparency_list[s_transparency_count].transparency);
                    	s_transparency_list[s_transparency_count].bitpos = bitset;

                    	s_transparency_count++;
                    }
	#endif
#endif /*HLRAD_HULLU*/

					ThreadLock (); //--vluzacn
                    s_vismatrix[bitset >> 3] |= 1 << (bitset & 7);
					ThreadUnlock (); //--vluzacn
                }
            }
        }
    }
}

#ifndef HLRAD_VISMATRIX_NOMARKSURFACES
// =====================================================================================
//  BuildVisRow
//      Calc vis bits from a single patch
// =====================================================================================
static void     BuildVisRow(const int patchnum, byte* pvs, const int head, const unsigned int bitpos)
{
    int             j, k, l;
    byte            face_tested[MAX_MAP_FACES];
    dleaf_t*        leaf;

    memset(face_tested, 0, g_numfaces);

    // leaf 0 is the solid leaf (skipped)
#ifdef HLRAD_VIS_FIX
    for (j = 1, leaf = g_dleafs + 1; j < 1 + g_dmodels[0].visleafs; j++, leaf++)
#else
    for (j = 1, leaf = g_dleafs + 1; j < g_numleafs; j++, leaf++)
#endif
    {
        if (!(pvs[(j - 1) >> 3] & (1 << ((j - 1) & 7))))
            continue;                                      // not in pvs
        for (k = 0; k < leaf->nummarksurfaces; k++)
        {
            l = g_dmarksurfaces[leaf->firstmarksurface + k];

            // faces can be marksurfed by multiple leaves, but
            // don't bother testing again
            if (face_tested[l])
                continue;
            face_tested[l] = 1;

            TestPatchToFace(patchnum, l, head, bitpos
#ifdef HLRAD_ENTITYBOUNCE_FIX
				, pvs
#endif
				);
        }
    }
}
#endif

// =====================================================================================
// BuildVisLeafs
//      This is run by multiple threads
// =====================================================================================
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif
static void     BuildVisLeafs(int threadnum)
{
    int             i;
    int             facenum, facenum2;
    byte            pvs[(MAX_MAP_LEAFS + 7) / 8];
    dleaf_t*        srcleaf;
    patch_t*        patch;
    int             head;
    unsigned        bitpos;
    unsigned        patchnum;

    while (1)
    {
        //
        // build a minimal BSP tree that only
        // covers areas relevent to the PVS
        //
        i = GetThreadWork();
        if (i == -1)
            break;
        i++;                                               // skip leaf 0
        srcleaf = &g_dleafs[i];
#ifdef HLRAD_WITHOUTVIS
        if (!g_visdatasize)
		{
	#ifdef ZHLT_DecompressVis_FIX
			memset (pvs, 255, (g_dmodels[0].visleafs + 7) / 8);
	#else
            memset(pvs, 255, (g_numleafs + 7) / 8);
	#endif
		}
		else
		{
#endif
#ifdef HLRAD_VIS_FIX
		if (srcleaf->visofs == -1)
		{
			Developer (DEVELOPER_LEVEL_ERROR, "Error: No visdata for leaf %d\n", i);
			continue;
		}
#endif
        DecompressVis(&g_dvisdata[srcleaf->visofs], pvs, sizeof(pvs));
#ifdef HLRAD_WITHOUTVIS
		}
#endif
        head = 0;

        //
        // go through all the faces inside the
        // leaf, and process the patches that
        // actually have origins inside
        //
#ifdef HLRAD_VISMATRIX_NOMARKSURFACES
		for (facenum = 0; facenum < g_numfaces; facenum++)
		{
			for (patch = g_face_patches[facenum]; patch; patch = patch->next)
			{
				if (patch->leafnum != i)
					continue;
				patchnum = patch - g_patches;
#ifdef HALFBIT
				bitpos = patchnum * g_num_patches - (patchnum * (patchnum + 1)) / 2;
#else
				bitpos = patchnum * g_num_patches;
#endif
				for (facenum2 = facenum + 1; facenum2 < g_numfaces; facenum2++)
					TestPatchToFace (patchnum, facenum2, head, bitpos, pvs);
			}
		}
#else
        for (int lface = 0; lface < srcleaf->nummarksurfaces; lface++)
        {
            facenum = g_dmarksurfaces[srcleaf->firstmarksurface + lface];
            for (patch = g_face_patches[facenum]; patch; patch = patch->next)
            {
#ifdef HLRAD_ENTITYBOUNCE_FIX
				if (patch->leafnum != i)
					continue;
#else
                dleaf_t *leaf = PointInLeaf(patch->origin);
                if (leaf != srcleaf)
                    continue;
#endif

                patchnum = patch - g_patches;

#ifdef HALFBIT
                bitpos = patchnum * g_num_patches - (patchnum * (patchnum + 1)) / 2;
#else
                bitpos = patchnum * g_num_patches;
#endif
                // build to all other world leafs
                BuildVisRow(patchnum, pvs, head, bitpos);

                // build to bmodel faces
                if (g_nummodels < 2)
                    continue;
                for (facenum2 = g_dmodels[1].firstface; facenum2 < g_numfaces; facenum2++)
                    TestPatchToFace(patchnum, facenum2, head, bitpos
#ifdef HLRAD_ENTITYBOUNCE_FIX
						, pvs
#endif
						);
            }
        }
#ifdef HLRAD_ENTITYBOUNCE_FIX
		if (g_nummodels >= 2)
		{
			for (facenum = g_dmodels[1].firstface; facenum < g_numfaces; facenum++)
			{
				for (patch = g_face_patches[facenum]; patch; patch = patch->next)
				{
					if (patch->leafnum != i)
						continue;
					patchnum = patch - g_patches;
	#ifdef HALFBIT
					bitpos = patchnum * g_num_patches - (patchnum * (patchnum + 1)) / 2;
	#else
					bitpos = patchnum * g_num_patches;
	#endif
					// skip BuildVisRow here because entity patchnums are always bigger than world patchnums.
					for (facenum2 = g_dmodels[1].firstface; facenum2 < g_numfaces; facenum2++)
						TestPatchToFace (patchnum, facenum2, head, bitpos, pvs);
				}
			}
		}
#endif
#endif

    }
}

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

// =====================================================================================
// BuildVisMatrix
// =====================================================================================
static void     BuildVisMatrix()
{
    int             c;

#ifdef HALFBIT
    c = ((g_num_patches + 1) * (g_num_patches + 1)) / 16;
	c += 1; //--vluzacn
#else
    c = g_num_patches * ((g_num_patches + 7) / 8);
#endif

    Log("%-20s: %5.1f megs\n", "visibility matrix", c / (1024 * 1024.0));

    s_vismatrix = (byte*)AllocBlock(c);

    if (!s_vismatrix)
    {
        Log("Failed to allocate s_vismatrix");
        hlassume(s_vismatrix != NULL, assume_NoMemory);
    }

#ifdef HLRAD_VIS_FIX
    NamedRunThreadsOn(g_dmodels[0].visleafs, g_estimate, BuildVisLeafs);
#else
    NamedRunThreadsOn(g_numleafs - 1, g_estimate, BuildVisLeafs);
#endif
}

static void     FreeVisMatrix()
{
    if (s_vismatrix)
    {
        if (FreeBlock(s_vismatrix))
        {
            s_vismatrix = NULL;
        }
        else
        {
            Warning("Unable to free s_vismatrix");
        }
    }

#ifndef HLRAD_TRANSPARENCY_CPP
#ifdef HLRAD_HULLU
    if(s_transparency_list)
    {
    	free(s_transparency_list);
    	s_transparency_list = NULL;
    }
    s_transparency_count = s_max_transparency_count = 0;
#endif
#endif

}

// =====================================================================================
// CheckVisBit
// =====================================================================================
#ifdef HLRAD_TRANSPARENCY_CPP
static bool     CheckVisBitVismatrix(unsigned p1, unsigned p2
#ifdef HLRAD_HULLU
									 , vec3_t &transparency_out
									 , unsigned int &next_index
#endif
									 )
{
    unsigned        bitpos;

#ifdef HLRAD_HULLU
    const unsigned a = p1;
    const unsigned b = p2;

    VectorFill(transparency_out, 1.0);
#endif

    if (p1 > p2)
    {
#ifndef HLRAD_HULLU
        const unsigned a = p1;
        const unsigned b = p2;
#endif
        p1 = b;
        p2 = a;
    }

    if (p1 > g_num_patches)
    {
        Warning("in CheckVisBit(), p1 > num_patches");
    }
    if (p2 > g_num_patches)
    {
        Warning("in CheckVisBit(), p2 > num_patches");
    }

#ifdef HALFBIT
    bitpos = p1 * g_num_patches - (p1 * (p1 + 1)) / 2 + p2;
#else
    bitpos = p1 * g_num_patches + p2;
#endif

    if (s_vismatrix[bitpos >> 3] & (1 << (bitpos & 7)))
    {
#ifdef HLRAD_HULLU    	
    	if(g_customshadow_with_bouncelight)
    	{
    	    GetTransparency( a, b, transparency_out, next_index );
    	}
#endif
        return true;
    }

    return false;
}
#else /*HLRAD_TRANSPARENCY_CPP*/
static bool     CheckVisBitVismatrix(unsigned p1, unsigned p2
#ifdef HLRAD_HULLU
									 , vec3_t &transparency_out
#endif
									 )
{
    unsigned        bitpos;

    if (p1 > p2)
    {
        const unsigned a = p1;
        const unsigned b = p2;
        p1 = b;
        p2 = a;
    }

    if (p1 > g_num_patches)
    {
        Warning("in CheckVisBit(), p1 > num_patches");
    }
    if (p2 > g_num_patches)
    {
        Warning("in CheckVisBit(), p2 > num_patches");
    }

#ifdef HALFBIT
    bitpos = p1 * g_num_patches - (p1 * (p1 + 1)) / 2 + p2;
#else
    bitpos = p1 * g_num_patches + p2;
#endif

    if (s_vismatrix[bitpos >> 3] & (1 << (bitpos & 7)))
    {
#ifdef HLRAD_HULLU    	
    	if(g_customshadow_with_bouncelight)
    	{
    	    vec3_t getvalue = {1.0, 1.0, 1.0};
    	    FindOpacity(bitpos, getvalue);
    	    VectorCopy(getvalue, transparency_out);
    	}
    	else
    	{
    	    VectorFill(transparency_out, 1.0);
    	}
#endif
        return true;
    }

#ifdef HLRAD_HULLU
    VectorFill(transparency_out, 0.0);
#endif

    return false;
}
#endif /*HLRAD_TRANSPARENCY_CPP*/

//
// end old vismat.c
////////////////////////////

// =====================================================================================
// MakeScalesVismatrix
// =====================================================================================
void            MakeScalesVismatrix()
{
    char            transferfile[_MAX_PATH];

    hlassume(g_num_patches < MAX_VISMATRIX_PATCHES, assume_MAX_PATCHES);

#ifdef ZHLT_DEFAULTEXTENSION_FIX
	safe_snprintf(transferfile, _MAX_PATH, "%s.inc", g_Mapname);
#else
    safe_strncpy(transferfile, g_source, _MAX_PATH);
    StripExtension(transferfile);
    DefaultExtension(transferfile, ".inc");
#endif

    if (!g_incremental || !readtransfers(transferfile, g_num_patches))
    {
        // determine visibility between g_patches
        BuildVisMatrix();
        g_CheckVisBit = CheckVisBitVismatrix;

#ifdef HLRAD_HULLU
	#ifdef HLRAD_TRANSPARENCY_CPP
        CreateFinalTransparencyArrays("custom shadow array");
	#else
        if((s_max_transparency_count*sizeof(transparency_t))>=(1024 * 1024))
        	Log("%-20s: %5.1f megs\n", "custom shadow array", (s_max_transparency_count*sizeof(transparency_t)) / (1024 * 1024.0));
        else if(s_transparency_count)
        	Log("%-20s: %5.1f kilos\n", "custom shadow array", (s_max_transparency_count*sizeof(transparency_t)) / 1024.0);
	#endif
#endif

#ifndef HLRAD_HULLU
        NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);
#else
	if(g_rgb_transfers)
		{NamedRunThreadsOn(g_num_patches, g_estimate, MakeRGBScales);}
	else
		{NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);}
#endif
        FreeVisMatrix();
#ifdef HLRAD_HULLU
	#ifdef HLRAD_TRANSPARENCY_CPP
        FreeTransparencyArrays();
	#endif
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
            writetransfers(transferfile, g_num_patches);
        else
            _unlink(transferfile);
        DumpTransfersMemoryUsage();
#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
		CreateFinalStyleArrays ("dynamic shadow array");
#endif
    }
}

