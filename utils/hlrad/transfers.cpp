#include "qrad.h"

#ifdef SYSTEM_WIN32
#include <sys/stat.h>
#include <fcntl.h>
#include "win32fix.h"
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

/*
 * =============
 * writetransfers
 * =============
 */

void            writetransfers(const char* const transferfile, const long total_patches)
{
    FILE           *file;

    file = fopen(transferfile, "w+b");
    if (file != NULL)
    {
        unsigned        amtwritten;
        patch_t*        patch;

        Log("Writing transfers file [%s]\n", transferfile);

        amtwritten = fwrite(&total_patches, sizeof(total_patches), 1, file);
        if (amtwritten != 1)
        {
            goto FailedWrite;
        }

        long patchcount = total_patches;
        for (patch = g_patches; patchcount-- > 0; patch++)
        {
            amtwritten = fwrite(&patch->iIndex, sizeof(patch->iIndex), 1, file);
            if (amtwritten != 1)
            {
                goto FailedWrite;
            }

            if (patch->iIndex)
            {
                amtwritten = fwrite(patch->tIndex, sizeof(transfer_index_t), patch->iIndex, file);
                if (amtwritten != patch->iIndex)
                {
                    goto FailedWrite;
                }
            }

            amtwritten = fwrite(&patch->iData, sizeof(patch->iData), 1, file);
            if (amtwritten != 1)
            {
                goto FailedWrite;
            }
            if (patch->iData)
            {
#ifdef HLRAD_HULLU
				if(g_rgb_transfers)
				{
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
			amtwritten = fwrite(patch->tRGBData, vector_size[g_rgbtransfer_compress_type], patch->iData, file);
	#else
			amtwritten = fwrite(patch->tRGBData, sizeof(rgb_transfer_data_t), patch->iData, file);
	#endif
				}
				else
				{
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
			amtwritten = fwrite(patch->tData, float_size[g_transfer_compress_type], patch->iData, file);
	#else
			amtwritten = fwrite(patch->tData, sizeof(transfer_data_t), patch->iData, file);
	#endif
				}
#else
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
				amtwritten = fwrite(patch->tData, float_size[g_transfer_compress_type], patch->iData, file);
	#else
                amtwritten = fwrite(patch->tData, sizeof(transfer_data_t), patch->iData, file);
	#endif
#endif
                if (amtwritten != patch->iData)
                {
                    goto FailedWrite;
                }
            }
        }

        fclose(file);
    }
    else
    {
        Error("Failed to open incremenetal file [%s] for writing\n", transferfile);
    }
    return;

  FailedWrite:
    fclose(file);
    unlink(transferfile);
    //Warning("Failed to generate incremental file [%s] (probably ran out of disk space)\n");
    Warning("Failed to generate incremental file [%s] (probably ran out of disk space)\n", transferfile); //--vluzacn
}

/*
 * =============
 * readtransfers
 * =============
 */

bool            readtransfers(const char* const transferfile, const long numpatches)
{
    FILE*           file;
    long            total_patches;

    file = fopen(transferfile, "rb");
    if (file != NULL)
    {
        unsigned        amtread;
        patch_t*        patch;

        Log("Reading transfers file [%s]\n", transferfile);

        amtread = fread(&total_patches, sizeof(total_patches), 1, file);
        if (amtread != 1)
        {
            goto FailedRead;
        }
        if (total_patches != numpatches)
        {
            goto FailedRead;
        }

        long patchcount = total_patches;
        for (patch = g_patches; patchcount-- > 0; patch++)
        {
            amtread = fread(&patch->iIndex, sizeof(patch->iIndex), 1, file);
            if (amtread != 1)
            {
                goto FailedRead;
            }
            if (patch->iIndex)
            {
                patch->tIndex = (transfer_index_t*)AllocBlock(patch->iIndex * sizeof(transfer_index_t *));
                hlassume(patch->tIndex != NULL, assume_NoMemory);
                amtread = fread(patch->tIndex, sizeof(transfer_index_t), patch->iIndex, file);
                if (amtread != patch->iIndex)
                {
                    goto FailedRead;
                }
            }

            amtread = fread(&patch->iData, sizeof(patch->iData), 1, file);
            if (amtread != 1)
            {
                goto FailedRead;
            }
            if (patch->iData)
            {
#ifdef HLRAD_HULLU
				if(g_rgb_transfers)
				{
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                    patch->tRGBData = (rgb_transfer_data_t*)AllocBlock(patch->iData * vector_size[g_rgbtransfer_compress_type] + unused_size);
	#else
                    patch->tRGBData = (rgb_transfer_data_t*)AllocBlock(patch->iData * sizeof(rgb_transfer_data_t *)); //wrong? --vluzacn
	#endif
                    hlassume(patch->tRGBData != NULL, assume_NoMemory);
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                    amtread = fread(patch->tRGBData, vector_size[g_rgbtransfer_compress_type], patch->iData, file);		    
	#else
                    amtread = fread(patch->tRGBData, sizeof(rgb_transfer_data_t), patch->iData, file);		    
	#endif
				}
				else
				{
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                    patch->tData = (transfer_data_t*)AllocBlock(patch->iData * float_size[g_transfer_compress_type] + unused_size);
	#else
                    patch->tData = (transfer_data_t*)AllocBlock(patch->iData * sizeof(transfer_data_t *));
	#endif
                    hlassume(patch->tData != NULL, assume_NoMemory);
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                    amtread = fread(patch->tData, float_size[g_transfer_compress_type], patch->iData, file);		    
	#else
                    amtread = fread(patch->tData, sizeof(transfer_data_t), patch->iData, file);		    
	#endif
				}
#else
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                patch->tData = (transfer_data_t*)AllocBlock(patch->iData * float_size[g_transfer_compress_type] + unused_size);
	#else
                patch->tData = (transfer_data_t*)AllocBlock(patch->iData * sizeof(transfer_data_t *));
	#endif
                hlassume(patch->tData != NULL, assume_NoMemory);
	#ifdef HLRAD_TRANSFERDATA_COMPRESS
                amtread = fread(patch->tData, float_size[g_transfer_compress_type], patch->iData, file);
	#else
                amtread = fread(patch->tData, sizeof(transfer_data_t), patch->iData, file);
	#endif
#endif
                if (amtread != patch->iData)
                {
                    goto FailedRead;
                }
            }
        }

        fclose(file);
        //Warning("Finished reading transfers file [%s] %d\n", transferfile);
        Warning("Finished reading transfers file [%s]\n", transferfile); //--vluzacn
        return true;
    }
    Warning("Failed to open transfers file [%s]\n", transferfile);
    return false;

  FailedRead:
    {
        unsigned        x;
        patch_t*        patch = g_patches;

        for (x = 0; x < g_num_patches; x++, patch++)
        {
            FreeBlock(patch->tData);
            FreeBlock(patch->tIndex);
            patch->iData = 0;
            patch->iIndex = 0;
            patch->tData = NULL;
            patch->tIndex = NULL;
        }
    }
    fclose(file);
    unlink(transferfile);
    return false;
}

