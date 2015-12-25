#include "csg.h"

vec3_t          g_hull_size[NUM_HULLS][2] = 
{
    {// 0x0x0
        {0, 0, 0},          {0, 0, 0}
    }
    ,
    {// 32x32x72
        {-16, -16, -36},    {16, 16, 36}
    }
    ,                                                      
    {// 64x64x64
        {-32, -32, -32},    {32, 32, 32}
    }
    ,                                                      
    {// 32x32x36
        {-16, -16, -18},    {16, 16, 18}
    }                                                     
};

void        LoadHullfile(const char* filename)
{
    if (filename == NULL)
    {
        return;
    }

    if (q_exists(filename))
    {
        Log("Loading hull definitions from '%s'\n", filename);
    }
    else
    {
        Error("Could not find hull definition file '%s'\n", filename);
        return;
    }

    float x1,y1,z1;
	float x2,y2,z2;
	char magic;

    FILE* file = fopen(filename, "r");

    int count;
    int i;

	magic = (char)fgetc(file);
	rewind(file);

	if(magic == '(')
	{   // Test for old-style hull-file

		for (i=0; i < NUM_HULLS; i++)
		{
			count = fscanf(file, "( %f %f %f ) ( %f %f %f )\n", &x1, &y1, &z1, &x2, &y2, &z2);
			if (count != 6)
			{
				Error("Could not parse old hull definition file '%s' (%d, %d)\n", filename, i, count);
			}

			g_hull_size[i][0][0] = x1;
			g_hull_size[i][0][1] = y1;
			g_hull_size[i][0][2] = z1;

			g_hull_size[i][1][0] = x2;
			g_hull_size[i][1][1] = y2;
			g_hull_size[i][1][2] = z2;			

		}
		
	}
	else
	{
		// Skip hull 0 (visibile polygons)
		for (i=1; i<NUM_HULLS; i++)
		{
			count = fscanf(file, "%f %f %f\n", &x1, &y1, &z1);
			if (count != 3)
			{
				Error("Could not parse new hull definition file '%s' (%d, %d)\n", filename, i, count);
			}
			x1 *= 0.5;
			y1 *= 0.5;
			z1 *= 0.5;

			g_hull_size[i][0][0] = -x1;
			g_hull_size[i][0][1] = -y1;
			g_hull_size[i][0][2] = -z1;

			g_hull_size[i][1][0] = x1;
			g_hull_size[i][1][1] = y1;
			g_hull_size[i][1][2] = z1;
		}
    }

    fclose(file);
}
