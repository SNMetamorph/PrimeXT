#include "cmdlib.h"
#include "compress.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const size_t unused_size = 3u; // located at the end of a block

const char *(float_type_string[float_type_count]) =
{
	"32bit",
	"16bit",
	"8bit"
};

const size_t float_size[float_type_count] =
{
	4u,
	2u,
	1u
};

const char *(vector_type_string[vector_type_count]) =
{
	"96bit",
	"48bit",
	"32bit",
	"24bit"
};

const size_t vector_size[vector_type_count] =
{
	12u,
	6u,
	4u,
	3u
};

void fail ()
{
	Error ("Compatability test failed. Please disable HLRAD_TRANSFERDATA_COMPRESS in cmdlib.h and recompile ZHLT.");
}

void compress_compatability_test ()
{
	unsigned char *v = (unsigned char *)malloc (16u);
	memset (v, 0, 16u);
	if (sizeof(char) !=1 || sizeof(unsigned int) != 4 || sizeof(float) != 4)
		fail ();
	*(float *)(v+1) = 0.123f;
	if (*(unsigned int *)v != 4226247936u || *(unsigned int *)(v+1) != 1039918957u)
		fail ();
	*(float *)(v+1) = -58;
	if (*(unsigned int *)v != 1744830464u || *(unsigned int *)(v+1) != 3261595648u)
		fail ();
	float f[5] = {0.123f, 1.f, 0.f, 0.123f, 0.f};
	memset (v, ~0, 16u);
	vector_compress (VECTOR24, v, &f[0], &f[1], &f[2]);
	float_compress (FLOAT16, v+6, &f[3]);
	float_compress (FLOAT16, v+4, &f[4]);
	if (((unsigned int *)v)[0] != 4286318595u || ((unsigned int *)v)[1] != 3753771008u)
		fail ();
	float_decompress (FLOAT16, v+6, &f[3]);
	float_decompress (FLOAT16, v+4, &f[4]);
	vector_decompress (VECTOR24, v, &f[0], &f[1], &f[2]);
	float ans[5] = {0.109375f,1.015625f,0.015625f,0.123001f,0.000000f};
	int i;
	for (i=0; i<5; ++i)
		if (f[i]-ans[i] > 0.00001f || f[i]-ans[i] < -0.00001f)
			fail ();
	free (v);
}
