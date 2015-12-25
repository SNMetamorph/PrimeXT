#include "qrad.h"

funcCheckVisBit g_CheckVisBit = NULL;

#ifdef ZHLT_64BIT_FIX
size_t          g_total_transfer = 0;
size_t          g_transfer_index_bytes = 0;
size_t          g_transfer_data_bytes = 0;
#else
unsigned        g_total_transfer = 0;
unsigned        g_transfer_index_bytes = 0;
unsigned        g_transfer_data_bytes = 0;
#endif

#define COMPRESSED_TRANSFERS
//#undef  COMPRESSED_TRANSFERS

int             FindTransferOffsetPatchnum(transfer_index_t* tIndex, const patch_t* const patch, const unsigned patchnum)
{
    //
    // binary search for match
    //
    int             low = 0;
    int             high = patch->iIndex - 1;
    unsigned        offset;

    while (1)
    {
        offset = (low + high) / 2;

        if ((tIndex[offset].index + tIndex[offset].size) < patchnum)
        {
            low = offset + 1;
        }
        else if (tIndex[offset].index > patchnum)
        {
            high = offset - 1;
        }
        else
        {
            unsigned        x;
            unsigned int    rval = 0;
            transfer_index_t* pIndex = tIndex;

            for (x = 0; x < offset; x++, pIndex++)
            {
                rval += pIndex->size + 1;
            }
            rval += patchnum - tIndex[offset].index;
            return rval;
        }
        if (low > high)
        {
            return -1;
        }
    }
}

#ifdef COMPRESSED_TRANSFERS

static unsigned GetLengthOfRun(const transfer_raw_index_t* raw, const transfer_raw_index_t* const end)
{
    unsigned        run_size = 0;

    while (raw < end)
    {
        if (((*raw) + 1) == (*(raw + 1)))
        {
            raw++;
            run_size++;

            if (run_size >= MAX_COMPRESSED_TRANSFER_INDEX_SIZE)
            {
                return run_size;
            }
        }
        else
        {
            return run_size;
        }
    }
    return run_size;
}

static transfer_index_t* CompressTransferIndicies(transfer_raw_index_t* tRaw, const unsigned rawSize, unsigned* iSize)
{
    unsigned        x;
    unsigned        size = rawSize;
    unsigned        compressed_count = 0;

    transfer_raw_index_t* raw = tRaw;
    transfer_raw_index_t* end = tRaw + rawSize - 1;        // -1 since we are comparing current with next and get errors when bumping into the 'end'

#ifdef HLRAD_MORE_PATCHES
    unsigned        compressed_count_1 = 0;

	for (x = 0; x < rawSize; x++)
	{
		x += GetLengthOfRun (tRaw + x, tRaw + rawSize - 1);
		compressed_count_1++;
	}

	if (!compressed_count_1)
	{
		return NULL;
	}

	transfer_index_t* CompressedArray = (transfer_index_t*)AllocBlock(sizeof(transfer_index_t) * compressed_count_1);
#else
    transfer_index_t CompressedArray[MAX_PATCHES];         // somewhat big stack object (1 Mb with 256k patches)
#endif
    transfer_index_t* compressed = CompressedArray;

    for (x = 0; x < size; x++, raw++, compressed++)
    {
        compressed->index = (*raw);
        compressed->size = GetLengthOfRun(raw, end);       // Zero based (count 0 still implies 1 item in the list, so 256 max entries result)
        raw += compressed->size;
        x += compressed->size;
        compressed_count++;                                // number of entries in compressed table
    }

    *iSize = compressed_count;

#ifdef HLRAD_MORE_PATCHES
	if (compressed_count != compressed_count_1)
	{
		Error ("CompressTransferIndicies: internal error");
	}

	ThreadLock();
	g_transfer_index_bytes += sizeof(transfer_index_t) * compressed_count;
	ThreadUnlock();

	return CompressedArray;
#else
    if (compressed_count)
    {
        unsigned        compressed_array_size = sizeof(transfer_index_t) * compressed_count;
        transfer_index_t* rval = (transfer_index_t*)AllocBlock(compressed_array_size);

        ThreadLock();
        g_transfer_index_bytes += compressed_array_size;
        ThreadUnlock();

        memcpy(rval, CompressedArray, compressed_array_size);
        return rval;
    }
    else
    {
        return NULL;
    }
#endif
}

#else /*COMPRESSED_TRANSFERS*/

static transfer_index_t* CompressTransferIndicies(const transfer_raw_index_t* tRaw, const unsigned rawSize, unsigned* iSize)
{
    unsigned        x;
    unsigned        size = rawSize;
    unsigned        compressed_count = 0;

    transfer_raw_index_t* raw = tRaw;
    transfer_raw_index_t* end = tRaw + rawSize;

#ifdef HLRAD_MORE_PATCHES
	if (!size)
	{
		return NULL;
	}

	transfer_index_t CompressedArray = (transfer_index_t*)AllocBlock(sizeof(transfer_index_t) * size);
#else
    transfer_index_t CompressedArray[MAX_PATCHES];         // somewhat big stack object (1 Mb with 256k patches)
#endif
    transfer_index_t* compressed = CompressedArray;

    for (x = 0; x < size; x++, raw++, compressed++)
    {
        compressed->index = (*raw);
        compressed->size = 0;
        compressed_count++;                                // number of entries in compressed table
    }

    *iSize = compressed_count;

#ifdef HLRAD_MORE_PATCHES
	ThreadLock();
	g_transfer_index_bytes += sizeof(transfer_index_t) * size;
	ThreadUnlock();

	return CompressedArray;
#else
    if (compressed_count)
    {
        unsigned        compressed_array_size = sizeof(transfer_index_t) * compressed_count;
        transfer_index_t* rval = AllocBlock(compressed_array_size);

        ThreadLock();
        g_transfer_index_bytes += compressed_array_size;
        ThreadUnlock();

        memcpy(rval, CompressedArray, compressed_array_size);
        return rval;
    }
    else
    {
        return NULL;
    }
#endif
}
#endif /*COMPRESSED_TRANSFERS*/

/*
 * =============
 * MakeScales
 * 
 * This is the primary time sink.
 * It can be run multi threaded.
 * =============
 */
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif
void            MakeScales(const int threadnum)
{
    unsigned        i;
    unsigned        j;
    vec3_t          delta;
    vec_t           dist;
    int             count;
    float           trans;
    patch_t*        patch;
    patch_t*        patch2;
    vec3_t          origin;
    vec_t           area;
    const vec_t*    normal1;
    const vec_t*    normal2;

#ifdef HLRAD_HULLU
#ifdef HLRAD_TRANSPARENCY_CPP
    unsigned int    fastfind_index = 0;
#endif
#endif

    vec_t           total;

#ifdef HLRAD_TRANSFERDATA_COMPRESS
    transfer_raw_index_t* tIndex;
    float* tData;

#ifdef HLRAD_MORE_PATCHES
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * (g_num_patches + 1));
    float* tData_All = (float*)AllocBlock(sizeof(float) * (g_num_patches + 1));
#else
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * MAX_PATCHES);
    float* tData_All = (float*)AllocBlock(sizeof(float) * MAX_PATCHES);
#endif
#else
    transfer_raw_index_t* tIndex;
    transfer_data_t* tData;

#ifdef HLRAD_MORE_PATCHES
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * (g_num_patches + 1));
    transfer_data_t* tData_All = (transfer_data_t*)AllocBlock(sizeof(transfer_data_t) * (g_num_patches + 1));
#else
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * MAX_PATCHES);
    transfer_data_t* tData_All = (transfer_data_t*)AllocBlock(sizeof(transfer_data_t) * MAX_PATCHES);
#endif
#endif

    count = 0;

    while (1)
    {
        i = GetThreadWork();
        if (i == -1)
            break;

        patch = g_patches + i;
        patch->iIndex = 0;
        patch->iData = 0;

#ifndef HLRAD_TRANSNONORMALIZE
        total = 0.0;
#endif

        tIndex = tIndex_All;
        tData = tData_All;

        VectorCopy(patch->origin, origin);
        normal1 = getPlaneFromFaceNumber(patch->faceNumber)->normal;

        area = patch->area;
#ifdef HLRAD_TRANSLUCENT
		vec3_t backorigin;
		vec3_t backnormal;
		if (patch->translucent_b)
		{
			VectorMA (patch->origin, -(g_translucentdepth + 2*PATCH_HUNT_OFFSET), normal1, backorigin);
			VectorSubtract (vec3_origin, normal1, backnormal);
		}
#endif
#ifdef HLRAD_DIVERSE_LIGHTING
		bool lighting_diversify;
		vec_t lighting_power;
		vec_t lighting_scale;
		int miptex = g_texinfo[g_dfaces[patch->faceNumber].texinfo].miptex;
		lighting_power = g_lightingconeinfo[miptex][0];
		lighting_scale = g_lightingconeinfo[miptex][1];
		lighting_diversify = (lighting_power != 1.0 || lighting_scale != 1.0);
#endif

        // find out which patch2's will collect light
        // from patch
		// HLRAD_NOSWAP: patch collect light from patch2

        for (j = 0, patch2 = g_patches; j < g_num_patches; j++, patch2++)
        {
            vec_t           dot1;
            vec_t           dot2;

#ifdef HLRAD_HULLU
            vec3_t          transparency = {1.0,1.0,1.0};
#endif
#ifdef HLRAD_TRANSLUCENT
			bool useback;
			useback = false;
#endif

            if (!g_CheckVisBit(i, j
#ifdef HLRAD_HULLU
				, transparency
#ifdef HLRAD_TRANSPARENCY_CPP
				, fastfind_index
#endif
#endif
				) || (i == j))
            {
#ifdef HLRAD_TRANSLUCENT
				if (patch->translucent_b)
				{
					if ((i == j) ||
						!CheckVisBitBackwards(i, j, backorigin, backnormal
	#ifdef HLRAD_HULLU
						, transparency
	#endif
						))
					{
						continue;
					}
					useback = true;
				}
				else
				{
					continue;
				}
#else
                continue;
#endif
            }

            normal2 = getPlaneFromFaceNumber(patch2->faceNumber)->normal;

            // calculate transferemnce
            VectorSubtract(patch2->origin, origin, delta);
#ifdef HLRAD_TRANSLUCENT
			if (useback)
			{
				VectorSubtract (patch2->origin, backorigin, delta);
			}
#endif
#ifdef HLRAD_ACCURATEBOUNCE
			// move emitter back to its plane
			VectorMA (delta, -PATCH_HUNT_OFFSET, normal2, delta);
#endif

            dist = VectorNormalize(delta);
            dot1 = DotProduct(delta, normal1);
#ifdef HLRAD_TRANSLUCENT
			if (useback)
			{
				dot1 = DotProduct (delta, backnormal);
			}
#endif
            dot2 = -DotProduct(delta, normal2);
#ifdef HLRAD_ACCURATEBOUNCE
#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			bool light_behind_surface = false;
			if (dot1 <= NORMAL_EPSILON)
			{
				light_behind_surface = true;
			}
#else
			if (dot1 <= NORMAL_EPSILON)
			{
				continue;
			}
#endif
			if (dot2 * dist <= MINIMUM_PATCH_DISTANCE)
			{
				continue;
			}
#endif

#ifdef HLRAD_DIVERSE_LIGHTING
			if (lighting_diversify
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				&& !light_behind_surface
	#endif
				)
			{
				dot1 = lighting_scale * pow (dot1, lighting_power);
			}
#endif
            trans = (dot1 * dot2) / (dist * dist);         // Inverse square falloff factoring angle between patch normals
#ifdef HLRAD_TRANSWEIRDFIX
#ifdef HLRAD_NOSWAP
            if (trans * patch2->area > 0.8f)
				trans = 0.8f / patch2->area;
#else
			// HLRAD_TRANSWEIRDFIX:
			// we should limit "trans(patch2receive) * patch1area" 
			// instead of "trans(patch2receive) * patch2area".
			// also raise "0.4f" to "0.8f" ( 0.8/Q_PI = 1/4).
            if (trans * area > 0.8f)
                trans = 0.8f / area;
#endif
#endif
#ifdef HLRAD_ACCURATEBOUNCE
			if (dist < patch2->emitter_range - ON_EPSILON)
			{
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				if (light_behind_surface)
				{
					trans = 0.0;
				}
	#endif
				vec_t sightarea;
				const vec_t *receiver_origin;
				const vec_t *receiver_normal;
				const Winding *emitter_winding;
				receiver_origin = origin;
				receiver_normal = normal1;
	#ifdef HLRAD_TRANSLUCENT
				if (useback)
				{
					receiver_origin = backorigin;
					receiver_normal = backnormal;
				}
	#endif
				emitter_winding = patch2->winding;
				sightarea = CalcSightArea (receiver_origin, receiver_normal, emitter_winding, patch2->emitter_skylevel
	#ifdef HLRAD_DIVERSE_LIGHTING
					, lighting_power, lighting_scale
	#endif
					);
				
				vec_t frac;
				frac = dist / patch2->emitter_range;
				frac = (frac - 0.5f) * 2.0f; // make a smooth transition between the two methods
				frac = qmax (0, qmin (frac, 1));
				trans = frac * trans + (1 - frac) * (sightarea / patch2->area); // because later we will multiply this back
			}
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			else
			{
				if (light_behind_surface)
				{
					continue;
				}
			}
	#endif
#endif

#ifdef HLRAD_ACCURATEBOUNCE_REDUCEAREA
			trans *= patch2->exposure;
#endif
#ifdef HLRAD_HULLU
            trans = trans * VectorAvg(transparency); //hullu: add transparency effect
#endif
#ifdef HLRAD_TRANSLUCENT
			if (patch->translucent_b)
			{
				if (useback)
				{
					trans *= VectorAvg (patch->translucent_v);
				}
				else
				{
					trans *= 1 - VectorAvg (patch->translucent_v);
				}
			}
#endif

#ifndef HLRAD_ACCURATEBOUNCE
            if (trans >= 0)
#endif
            {
#ifndef HLRAD_TRANSWEIRDFIX
		float send;
#ifdef HLRAD_NOSWAP
				send = trans * area;
#else
                send = trans * patch2->area;
#endif

                // Caps light from getting weird
                if (send > 0.4f)
                {
#ifdef HLRAD_NOSWAP
					trans = 0.4f / area;
#else
                    trans = 0.4f / patch2->area;
#endif
                    send = 0.4f;
                }
#endif /*HLRAD_TRANSWEIRDFIX*/

#ifndef HLRAD_TRANSNONORMALIZE
                total += send;
#endif

#ifdef HLRAD_TRANSFERDATA_COMPRESS
				trans = trans * patch2->area;
#else
                // scale to 16 bit (black magic)
				// BUG: (in MakeRGBScales) convert to integer will lose data. --vluzacn
#ifdef HLRAD_NOSWAP
                trans = trans * patch2->area * INVERSE_TRANSFER_SCALE;
#else
                trans = trans * area * INVERSE_TRANSFER_SCALE;
#endif /*HLRAD_NOSWAP*/
                if (trans >= TRANSFER_SCALE_MAX)
                {
                    trans = TRANSFER_SCALE_MAX;
                }
#endif
            }
#ifndef HLRAD_ACCURATEBOUNCE
            else
            {
#if 0
                Warning("transfer < 0 (%f): dist=(%f)\n"
                        "   dot1=(%f) patch@(%4.3f %4.3f %4.3f) normal(%4.3f %4.3f %4.3f)\n"
                        "   dot2=(%f) patch@(%4.3f %4.3f %4.3f) normal(%4.3f %4.3f %4.3f)\n",
                        trans, dist,
                        dot1, patch->origin[0], patch->origin[1], patch->origin[2], patch->normal[0], patch->normal[1],
                        patch->normal[2], dot2, patch2->origin[0], patch2->origin[1], patch2->origin[2],
                        patch2->normal[0], patch2->normal[1], patch2->normal[2]);
#endif
                trans = 0.0;
            }
#endif
#ifdef HLRAD_ACCURATEBOUNCE
			if (trans <= 0.0)
			{
				continue;
			}
#endif

            *tData = trans;
            *tIndex = j;
            tData++;
            tIndex++;
            patch->iData++;
            count++;
        }

        // copy the transfers out
        if (patch->iData)
        {
#ifdef HLRAD_TRANSFERDATA_COMPRESS
			unsigned	data_size = patch->iData * float_size[g_transfer_compress_type] + unused_size;
#else
            unsigned        data_size = patch->iData * sizeof(transfer_data_t);
#endif

            patch->tData = (transfer_data_t*)AllocBlock(data_size);
            patch->tIndex = CompressTransferIndicies(tIndex_All, patch->iData, &patch->iIndex);

            hlassume(patch->tData != NULL, assume_NoMemory);
            hlassume(patch->tIndex != NULL, assume_NoMemory);

            ThreadLock();
            g_transfer_data_bytes += data_size;
            ThreadUnlock();

#ifdef HLRAD_REFLECTIVITY
			total = 1 / Q_PI;
#else
#ifdef HLRAD_TRANSNONORMALIZE
	#ifdef HLRAD_TRANSTOTAL_HACK
			total = g_transtotal_hack / Q_PI;
	#else
			total = 0.5 / Q_PI;
	#endif
#else // BAD assumption when there is SKY.
            //
            // normalize all transfers so exactly 50% of the light
            // is transfered to the surroundings
            //

            total = 0.5 / total;
#endif
#endif
            {
#ifdef HLRAD_TRANSFERDATA_COMPRESS
                unsigned        x;
                transfer_data_t* t1 = patch->tData;
                float* t2 = tData_All;

				float	f;
				for (x = 0; x < patch->iData; x++, t1+=float_size[g_transfer_compress_type], t2++)
				{
					f = (*t2) * total;
					float_compress (g_transfer_compress_type, t1, &f);
				}
#else
                unsigned        x;
                transfer_data_t* t1 = patch->tData;
                transfer_data_t* t2 = tData_All;

                for (x = 0; x < patch->iData; x++, t1++, t2++)
                {
                    (*t1) = (*t2) * total;
                }
#endif
            }
        }
    }

    FreeBlock(tIndex_All);
    FreeBlock(tData_All);

    ThreadLock();
    g_total_transfer += count;
    ThreadUnlock();
}

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

/*
 * =============
 * SwapTransfersTask
 * 
 * Change transfers from light sent out to light collected in.
 * In an ideal world, they would be exactly symetrical, but
 * because the form factors are only aproximated, then normalized,
 * they will actually be rather different.
 * =============
 */
#ifndef HLRAD_NOSWAP
void            SwapTransfers(const int patchnum)
{
    patch_t*        patch = &g_patches[patchnum];
    transfer_index_t* tIndex = patch->tIndex;
    transfer_data_t* tData = patch->tData;
    unsigned        x;

    for (x = 0; x < patch->iIndex; x++, tIndex++)
    {
        unsigned        size = (tIndex->size + 1);
        unsigned        patchnum2 = tIndex->index;
        unsigned        y;

        for (y = 0; y < size; y++, tData++, patchnum2++)
        {
            patch_t*        patch2 = &g_patches[patchnum2];

            if (patchnum2 > patchnum)
            {                                              // done with this list
                return;
            }
            else if (!patch2->iData)
            {                                              // Set to zero in this impossible case
                Log("patch2 has no iData\n");
                (*tData) = 0;
                continue;
            }
            else
            {
                transfer_index_t* tIndex2 = patch2->tIndex;
                transfer_data_t* tData2 = patch2->tData;
                int             offset = FindTransferOffsetPatchnum(tIndex2, patch2, patchnum);

                if (offset >= 0)
                {
                    transfer_data_t tmp = *tData;

                    *tData = tData2[offset];
                    tData2[offset] = tmp;
                }
                else
                {                                          // Set to zero in this impossible case
                    Log("FindTransferOffsetPatchnum returned -1 looking for patch %d in patch %d's transfer lists\n",
                        patchnum, patchnum2);
                    (*tData) = 0;
                    return;
                }
            }
        }
    }
}
#endif /*HLRAD_NOSWAP*/

#ifdef HLRAD_HULLU
/*
 * =============
 * MakeScales
 * 
 * This is the primary time sink.
 * It can be run multi threaded.
 * =============
 */
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif
void            MakeRGBScales(const int threadnum)
{
    unsigned        i;
    unsigned        j;
    vec3_t          delta;
    vec_t           dist;
    int             count;
    float           trans[3];
    float           trans_one;
    patch_t*        patch;
    patch_t*        patch2;
    vec3_t          origin;
    vec_t           area;
    const vec_t*    normal1;
    const vec_t*    normal2;

#ifdef HLRAD_TRANSPARENCY_CPP
    unsigned int    fastfind_index = 0;
#endif
    vec_t           total;

#ifdef HLRAD_TRANSFERDATA_COMPRESS
    transfer_raw_index_t* tIndex;
    float* tRGBData;

#ifdef HLRAD_MORE_PATCHES
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * (g_num_patches + 1));
    float* tRGBData_All = (float*)AllocBlock(sizeof(float[3]) * (g_num_patches + 1));
#else
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * MAX_PATCHES);
    float* tRGBData_All = (float*)AllocBlock(sizeof(float[3]) * MAX_PATCHES);
#endif
#else
    transfer_raw_index_t* tIndex;
    rgb_transfer_data_t* tRGBData;

#ifdef HLRAD_MORE_PATCHES
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * (g_num_patches + 1));
    rgb_transfer_data_t* tRGBData_All = (rgb_transfer_data_t*)AllocBlock(sizeof(rgb_transfer_data_t) * (g_num_patches + 1));
#else
    transfer_raw_index_t* tIndex_All = (transfer_raw_index_t*)AllocBlock(sizeof(transfer_index_t) * MAX_PATCHES);
    rgb_transfer_data_t* tRGBData_All = (rgb_transfer_data_t*)AllocBlock(sizeof(rgb_transfer_data_t) * MAX_PATCHES);
#endif
#endif

    count = 0;

    while (1)
    {
        i = GetThreadWork();
        if (i == -1)
            break;

        patch = g_patches + i;
        patch->iIndex = 0;
        patch->iData = 0;

#ifndef HLRAD_TRANSNONORMALIZE
        total = 0.0;
#endif

        tIndex = tIndex_All;
        tRGBData = tRGBData_All;

        VectorCopy(patch->origin, origin);
        normal1 = getPlaneFromFaceNumber(patch->faceNumber)->normal;

        area = patch->area;
#ifdef HLRAD_TRANSLUCENT
		vec3_t backorigin;
		vec3_t backnormal;
		if (patch->translucent_b)
		{
			VectorMA (patch->origin, -(g_translucentdepth + 2*PATCH_HUNT_OFFSET), normal1, backorigin);
			VectorSubtract (vec3_origin, normal1, backnormal);
		}
#endif
#ifdef HLRAD_DIVERSE_LIGHTING
		bool lighting_diversify;
		vec_t lighting_power;
		vec_t lighting_scale;
		int miptex = g_texinfo[g_dfaces[patch->faceNumber].texinfo].miptex;
		lighting_power = g_lightingconeinfo[miptex][0];
		lighting_scale = g_lightingconeinfo[miptex][1];
		lighting_diversify = (lighting_power != 1.0 || lighting_scale != 1.0);
#endif

        // find out which patch2's will collect light
        // from patch
		// HLRAD_NOSWAP: patch collect light from patch2

        for (j = 0, patch2 = g_patches; j < g_num_patches; j++, patch2++)
        {
            vec_t           dot1;
            vec_t           dot2;
            vec3_t          transparency = {1.0,1.0,1.0};
#ifdef HLRAD_TRANSLUCENT
			bool useback;
			useback = false;
#endif

            if (!g_CheckVisBit(i, j
				, transparency
#ifdef HLRAD_TRANSPARENCY_CPP
				, fastfind_index
#endif
				) || (i == j))
            {
#ifdef HLRAD_TRANSLUCENT
				if (patch->translucent_b)
				{
					if (!CheckVisBitBackwards(i, j, backorigin, backnormal
	#ifdef HLRAD_HULLU
						, transparency
	#endif
						) || (i==j))
					{
						continue;
					}
					useback = true;
				}
				else
				{
					continue;
				}
#else
                continue;
#endif
            }

            normal2 = getPlaneFromFaceNumber(patch2->faceNumber)->normal;

            // calculate transferemnce
            VectorSubtract(patch2->origin, origin, delta);
#ifdef HLRAD_TRANSLUCENT
			if (useback)
			{
				VectorSubtract (patch2->origin, backorigin, delta);
			}
#endif
#ifdef HLRAD_ACCURATEBOUNCE
			// move emitter back to its plane
			VectorMA (delta, -PATCH_HUNT_OFFSET, normal2, delta);
#endif

            dist = VectorNormalize(delta);
            dot1 = DotProduct(delta, normal1);
#ifdef HLRAD_TRANSLUCENT
			if (useback)
			{
				dot1 = DotProduct (delta, backnormal);
			}
#endif
            dot2 = -DotProduct(delta, normal2);
#ifdef HLRAD_ACCURATEBOUNCE
#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			bool light_behind_surface = false;
			if (dot1 <= NORMAL_EPSILON)
			{
				light_behind_surface = true;
			}
#else
			if (dot1 <= NORMAL_EPSILON)
			{
				continue;
			}
#endif
			if (dot2 * dist <= MINIMUM_PATCH_DISTANCE)
			{
				continue;
			}
#endif
			
#ifdef HLRAD_DIVERSE_LIGHTING
			if (lighting_diversify
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				&& !light_behind_surface
	#endif
				)
			{
				dot1 = lighting_scale * pow (dot1, lighting_power);
			}
#endif
            trans_one = (dot1 * dot2) / (dist * dist);         // Inverse square falloff factoring angle between patch normals
            
#ifdef HLRAD_TRANSWEIRDFIX
#ifdef HLRAD_NOSWAP
			if (trans_one * patch2->area > 0.8f)
			{
				trans_one = 0.8f / patch2->area;
			}
#else
			if (trans_one * area > 0.8f)
			{
				trans_one = 0.8f / area;
			}
#endif
#endif
#ifdef HLRAD_ACCURATEBOUNCE
			if (dist < patch2->emitter_range - ON_EPSILON)
			{
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
				if (light_behind_surface)
				{
					trans_one = 0.0;
				}
	#endif
				vec_t sightarea;
				const vec_t *receiver_origin;
				const vec_t *receiver_normal;
				const Winding *emitter_winding;
				receiver_origin = origin;
				receiver_normal = normal1;
	#ifdef HLRAD_TRANSLUCENT
				if (useback)
				{
					receiver_origin = backorigin;
					receiver_normal = backnormal;
				}
	#endif
				emitter_winding = patch2->winding;
				sightarea = CalcSightArea (receiver_origin, receiver_normal, emitter_winding, patch2->emitter_skylevel
	#ifdef HLRAD_DIVERSE_LIGHTING
					, lighting_power, lighting_scale
	#endif
					);
				
				vec_t frac;
				frac = dist / patch2->emitter_range;
				frac = (frac - 0.5f) * 2.0f; // make a smooth transition between the two methods
				frac = qmax (0, qmin (frac, 1));
				trans_one = frac * trans_one + (1 - frac) * (sightarea / patch2->area); // because later we will multiply this back
			}
	#ifdef HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN
			else
			{
				if (light_behind_surface)
				{
					continue;
				}
			}
	#endif
#endif
#ifdef HLRAD_ACCURATEBOUNCE_REDUCEAREA
			trans_one *= patch2->exposure;
#endif
            VectorFill(trans, trans_one);
            VectorMultiply(trans, transparency, trans); //hullu: add transparency effect
#ifdef HLRAD_TRANSLUCENT
			if (patch->translucent_b)
			{
				if (useback)
				{
					for (int x = 0; x < 3; x++)
					{
						trans[x] = patch->translucent_v[x] * trans[x];
					}
				}
				else
				{
					for (int x = 0; x < 3; x++)
					{
						trans[x] = (1 - patch->translucent_v[x]) * trans[x];
					}
				}
			}
#endif

#ifdef HLRAD_RGBTRANSFIX
#ifdef HLRAD_ACCURATEBOUNCE
			if (trans_one <= 0.0)
			{
				continue;
			}
#else
			if (trans_one >= 0)
#endif
			{
#ifndef HLRAD_TRANSWEIRDFIX
    float           send;
	#ifdef HLRAD_NOSWAP
				send = trans_one * area;
	#else
				send = trans_one * patch2->area;
	#endif
				if (send > 0.4f)
				{
	#ifdef HLRAD_NOSWAP
					trans_one = 0.4f / area;
	#else
                    trans_one = 0.4f / patch2->area;
	#endif
					send = 0.4f;
					VectorFill(trans, trans_one);
					VectorMultiply(trans, transparency, trans);
				}
#endif /*HLRAD_TRANSWEIRDFIX*/
	#ifndef HLRAD_TRANSNONORMALIZE
				total += send;
	#endif
#else /*HLRAD_RGBTRANSFIX*/
#ifdef HLRAD_ACCURATEBOUNCE
            if (VectorAvg(trans) <= 0.0)
			{
				continue;
			}
#else
            if (VectorAvg(trans) >= 0)
#endif
            {
            	/////////////////////////////////////////RED
                send = trans[0] * patch2->area;
                // Caps light from getting weird
                if (send > 0.4f) 
                {
                    trans[0] = 0.4f / patch2->area;
                    send = 0.4f;
                }
	#ifndef HLRAD_TRANSNONORMALIZE
                total += send / 3.0f;
	#endif
                
            	/////////////////////////////////////////GREEN
                send = trans[1] * patch2->area;
                // Caps light from getting weird
                if (send > 0.4f) 
                {
                    trans[1] = 0.4f / patch2->area;
                    send = 0.4f;
                }
	#ifndef HLRAD_TRANSNONORMALIZE
                total += send / 3.0f;
	#endif

            	/////////////////////////////////////////BLUE
                send = trans[2] * patch2->area;
                // Caps light from getting weird
                if (send > 0.4f) 
                {
                    trans[2] = 0.4f / patch2->area;
                    send = 0.4f;
                }
	#ifndef HLRAD_TRANSNONORMALIZE
                total += send / 3.0f;
	#endif
#endif /*HLRAD_RGBTRANSFIX*/

#ifdef HLRAD_TRANSFERDATA_COMPRESS
                VectorScale(trans, patch2 -> area, trans);
#else
                // scale to 16 bit (black magic)
#ifdef HLRAD_NOSWAP
                VectorScale(trans, patch2 -> area * INVERSE_TRANSFER_SCALE, trans);
#else
                VectorScale(trans, area * INVERSE_TRANSFER_SCALE, trans);
#endif /*HLRAD_NOSWAP*/

                if (trans[0] >= TRANSFER_SCALE_MAX)
                {
                    trans[0] = TRANSFER_SCALE_MAX;
                }
                if (trans[1] >= TRANSFER_SCALE_MAX)
                {
                    trans[1] = TRANSFER_SCALE_MAX;
                }
                if (trans[2] >= TRANSFER_SCALE_MAX)
                {
                    trans[2] = TRANSFER_SCALE_MAX;
                }
#endif
            }
#ifndef HLRAD_ACCURATEBOUNCE
            else
            {
#if 0
                Warning("transfer < 0 (%4.3f %4.3f %4.3f): dist=(%f)\n"
                        "   dot1=(%f) patch@(%4.3f %4.3f %4.3f) normal(%4.3f %4.3f %4.3f)\n"
                        "   dot2=(%f) patch@(%4.3f %4.3f %4.3f) normal(%4.3f %4.3f %4.3f)\n",
                        trans[0], trans[1], trans[2], dist,
                        dot1, patch->origin[0], patch->origin[1], patch->origin[2], patch->normal[0], patch->normal[1],
                        patch->normal[2], dot2, patch2->origin[0], patch2->origin[1], patch2->origin[2],
                        patch2->normal[0], patch2->normal[1], patch2->normal[2]);
#endif
                VectorFill(trans,0.0);
            }
#endif

#ifdef HLRAD_TRANSFERDATA_COMPRESS
			VectorCopy(trans, tRGBData);
            *tIndex = j;
            tRGBData+=3;
            tIndex++;
            patch->iData++;
#else
            VectorCopy(trans, *tRGBData);
            *tIndex = j;
            tRGBData++;
            tIndex++;
            patch->iData++;
#endif
            count++;
        }

        // copy the transfers out
        if (patch->iData)
        {
#ifdef HLRAD_TRANSFERDATA_COMPRESS
			unsigned	data_size = patch->iData * vector_size[g_rgbtransfer_compress_type] + unused_size;
#else
            unsigned data_size = patch->iData * sizeof(rgb_transfer_data_t);
#endif

            patch->tRGBData = (rgb_transfer_data_t*)AllocBlock(data_size);
            patch->tIndex = CompressTransferIndicies(tIndex_All, patch->iData, &patch->iIndex);

            hlassume(patch->tRGBData != NULL, assume_NoMemory);
            hlassume(patch->tIndex != NULL, assume_NoMemory);

            ThreadLock();
            g_transfer_data_bytes += data_size;
            ThreadUnlock();

#ifdef HLRAD_REFLECTIVITY
			total = 1 / Q_PI;
#else
#ifdef HLRAD_TRANSNONORMALIZE
	#ifdef HLRAD_TRANSTOTAL_HACK
			total = g_transtotal_hack / Q_PI;
	#else
			total = 0.5 / Q_PI;
	#endif
#else
            //
            // normalize all transfers so exactly 50% of the light
            // is transfered to the surroundings
            //
            total = 0.5 / total;
#endif
#endif
            {
#ifdef HLRAD_TRANSFERDATA_COMPRESS
                unsigned        x;
                rgb_transfer_data_t* t1 = patch->tRGBData;
				float* t2 = tRGBData_All;

				float f[3];
                for (x = 0; x < patch->iData; x++, t1+=vector_size[g_rgbtransfer_compress_type], t2+=3)
                {
                     VectorScale( t2, total, f );
					 vector_compress (g_rgbtransfer_compress_type, t1, &f[0], &f[1], &f[2]);
                }
#else
                unsigned        x;
                rgb_transfer_data_t* t1 = patch->tRGBData;
                rgb_transfer_data_t* t2 = tRGBData_All;

                for (x = 0; x < patch->iData; x++, t1++, t2++)
                {
                     VectorScale( *t2, total, *t1 );
                }
#endif
            }
        }
    }

    FreeBlock(tIndex_All);
    FreeBlock(tRGBData_All);

    ThreadLock();
    g_total_transfer += count;
    ThreadUnlock();
}

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

/*
 * =============
 * SwapTransfersTask
 * 
 * Change transfers from light sent out to light collected in.
 * In an ideal world, they would be exactly symetrical, but
 * because the form factors are only aproximated, then normalized,
 * they will actually be rather different.
 * =============
 */
#ifndef HLRAD_NOSWAP
void            SwapRGBTransfers(const int patchnum)
{
    patch_t*        		patch	= &g_patches[patchnum];
    transfer_index_t*		tIndex	= patch->tIndex;
    rgb_transfer_data_t* 	tRGBData= patch->tRGBData;
    unsigned        x;

    for (x = 0; x < patch->iIndex; x++, tIndex++)
    {
        unsigned        size = (tIndex->size + 1);
        unsigned        patchnum2 = tIndex->index;
        unsigned        y;

        for (y = 0; y < size; y++, tRGBData++, patchnum2++)
        {
            patch_t*        patch2 = &g_patches[patchnum2];

            if (patchnum2 > patchnum)
            {                                              // done with this list
                return;
            }
            else if (!patch2->iData)
            {                                              // Set to zero in this impossible case
                Log("patch2 has no iData\n");
                VectorFill(*tRGBData, 0);
                continue;
            }
            else
            {
                transfer_index_t* tIndex2 = patch2->tIndex;
                rgb_transfer_data_t* tRGBData2 = patch2->tRGBData;
                int             offset = FindTransferOffsetPatchnum(tIndex2, patch2, patchnum);

                if (offset >= 0)
                {
                    rgb_transfer_data_t tmp;
                    VectorCopy(*tRGBData, tmp)

                    VectorCopy(tRGBData2[offset], *tRGBData);
                    VectorCopy(tmp, tRGBData2[offset]);
                }
                else
                {                                          // Set to zero in this impossible case
                    Log("FindTransferOffsetPatchnum returned -1 looking for patch %d in patch %d's transfer lists\n",
                        patchnum, patchnum2);
                    VectorFill(*tRGBData, 0);
                    return;
                }
            }
        }
    }
}
#endif /*HLRAD_NOSWAP*/

#endif /*HLRAD_HULLU*/


#ifndef HLRAD_HULLU

void            DumpTransfersMemoryUsage()
{
#ifdef ZHLT_64BIT_FIX
    Log("Transfer Lists : %.0f transfers\n       Indices : %.0f bytes\n          Data : %.0f bytes\n",
        (double)g_total_transfer, (double)g_transfer_index_bytes, (double)g_transfer_data_bytes);
#else
    Log("Transfer Lists : %u transfers\n       Indices : %u bytes\n          Data : %u bytes\n",
        g_total_transfer, g_transfer_index_bytes, g_transfer_data_bytes);
#endif
}

#else

//More human readable numbers
void            DumpTransfersMemoryUsage()
{
#ifdef ZHLT_64BIT_FIX
	if(g_total_transfer > 1000*1000)
		Log("Transfer Lists : %11.0f : %8.2fM transfers\n", (double)g_total_transfer, (double)g_total_transfer/(1000.0f*1000.0f));
	else if(g_total_transfer > 1000)
		Log("Transfer Lists : %11.0f : %8.2fk transfers\n", (double)g_total_transfer, (double)g_total_transfer/1000.0f);
	else
		Log("Transfer Lists : %11.0f transfers\n", (double)g_total_transfer);
	
	if(g_transfer_index_bytes > 1024*1024)
		Log("       Indices : %11.0f : %8.2fM bytes\n", (double)g_transfer_index_bytes, (double)g_transfer_index_bytes/(1024.0f * 1024.0f));
	else if(g_transfer_index_bytes > 1024)
		Log("       Indices : %11.0f : %8.2fk bytes\n", (double)g_transfer_index_bytes, (double)g_transfer_index_bytes/1024.0f);
	else
		Log("       Indices : %11.0f bytes\n", (double)g_transfer_index_bytes);
	
	if(g_transfer_data_bytes > 1024*1024)
		Log("          Data : %11.0f : %8.2fM bytes\n", (double)g_transfer_data_bytes, (double)g_transfer_data_bytes/(1024.0f * 1024.0f));
	else if(g_transfer_data_bytes > 1024)
		Log("          Data : %11.0f : %8.2fk bytes\n", (double)g_transfer_data_bytes, (double)g_transfer_data_bytes/1024.0f);
	else
		Log("          Data : %11.0f bytes\n", (double)g_transfer_data_bytes);
#else
	if(g_total_transfer > 1000*1000)
		Log("Transfer Lists : %11u : %7.2fM transfers\n", g_total_transfer, g_total_transfer/(1000.0f*1000.0f));
	else if(g_total_transfer > 1000)
		Log("Transfer Lists : %11u : %7.2fk transfers\n", g_total_transfer, g_total_transfer/1000.0f);
	else
		Log("Transfer Lists : %11u transfers\n", g_total_transfer);
	
	if(g_transfer_index_bytes > 1024*1024)
		Log("       Indices : %11u : %7.2fM bytes\n", g_transfer_index_bytes, g_transfer_index_bytes/(1024.0f * 1024.0f));
	else if(g_transfer_index_bytes > 1024)
		Log("       Indices : %11u : %7.2fk bytes\n", g_transfer_index_bytes, g_transfer_index_bytes/1024.0f);
	else
		Log("       Indices : %11u bytes\n", g_transfer_index_bytes);
	
	if(g_transfer_data_bytes > 1024*1024)
		Log("          Data : %11u : %7.2fM bytes\n", g_transfer_data_bytes, g_transfer_data_bytes/(1024.0f * 1024.0f));
	else if(g_transfer_data_bytes > 1024)
		Log("          Data : %11u : %7.2fk bytes\n", g_transfer_data_bytes, g_transfer_data_bytes/1024.0f);
	else
		Log("          Data : %11u bytes\n", g_transfer_data_bytes); //--vluzacn
#endif
}

#endif


