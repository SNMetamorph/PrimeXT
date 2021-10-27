/*
optimize.cpp - studio model optimization (generate triangle strips as possible)
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "studio.h"
#include "studiomdl.h"

int	used[MAXSTUDIOTRIANGLES];
// the command list holds counts and s/t values that are valid for
// every frame
short	commands[MAXSTUDIOTRIANGLES * 13];
int	numcommands;
// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int	allverts, alltris, stripcount;
int	stripverts[MAXSTUDIOTRIANGLES+2];
int	striptris[MAXSTUDIOTRIANGLES+2];
int	neighbortri[MAXSTUDIOTRIANGLES][3];
int	neighboredge[MAXSTUDIOTRIANGLES][3];

s_trianglevert_t (*triangles)[3];
s_mesh_t *pmesh;

void FindNeighbor (int starttri, int startv)
{
	s_trianglevert_t			m1, m2;
	int			j;
	s_trianglevert_t	*last, *check;
	int			k;

	// used[starttri] |= (1 << startv);

	last = &triangles[starttri][0];

	m1 = last[(startv+1)%3];
	m2 = last[(startv+0)%3];

	for (j=starttri+1, check=&triangles[starttri+1][0] ; j<pmesh->numtris ; j++, check += 3)
	{
		if (used[j] == 7)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (memcmp(&check[k],&m1,sizeof(m1)))
				continue;
			if (memcmp(&check[ (k+1)%3 ],&m2,sizeof(m2)))
				continue;

			neighbortri[starttri][startv] = j;
			neighboredge[starttri][startv] = k;

			neighbortri[j][k] = starttri;
			neighboredge[j][k] = startv;

			used[starttri] |= (1 << startv);
			used[j] |= (1 << k);
			return;
		}
	}
}

int	StripLength (int starttri, int startv)
{
	int			j;
	int			k;

	used[starttri] = 2;

	stripverts[0] = (startv)%3;
	stripverts[1] = (startv+1)%3;
	stripverts[2] = (startv+2)%3;

	striptris[0] = starttri;
	striptris[1] = starttri;
	striptris[2] = starttri;
	stripcount = 3;

	while( 1 )
	{
		if (stripcount & 1)
		{
			j = neighbortri[starttri][(startv+1)%3];
			k = neighboredge[starttri][(startv+1)%3];
		}
		else
		{
			j = neighbortri[starttri][(startv+2)%3];
			k = neighboredge[starttri][(startv+2)%3];
		}
		if (j == -1 || used[j])
			goto done;

		stripverts[stripcount] = (k+2)%3;
		striptris[stripcount] = j;
		stripcount++;

		used[j] = 2;

		starttri = j;
		startv = k;
	}

done:

	// clear the temp used flags
	for (j=0 ; j<pmesh->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

int	FanLength (int starttri, int startv)
{
	int		j;
	int		k;

	used[starttri] = 2;

	stripverts[0] = (startv)%3;
	stripverts[1] = (startv+1)%3;
	stripverts[2] = (startv+2)%3;

	striptris[0] = starttri;
	striptris[1] = starttri;
	striptris[2] = starttri;
	stripcount = 3;

	while( 1 )
	{
		j = neighbortri[starttri][(startv+2)%3];
		k = neighboredge[starttri][(startv+2)%3];

		if (j == -1 || used[j])
			goto done;

		stripverts[stripcount] = (k+2)%3;
		striptris[stripcount] = j;
		stripcount++;

		used[j] = 2;

		starttri = j;
		startv = k;
	}

done:

	// clear the temp used flags
	for (j=0 ; j<pmesh->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

//Generate a list of trifans or strips for the model, which holds for all frames
int	numcommandnodes;

int BuildTris (s_trianglevert_t (*x)[3], s_mesh_t *y, byte **ppdata )
{
	int		i, j, k, m;
	int		startv;
	int		len, bestlen, besttype;
	static int	bestverts[MAXSTUDIOTRIANGLES];
	static int	besttris[MAXSTUDIOTRIANGLES];
	static int	peak[MAXSTUDIOTRIANGLES];
	int		type;
	int		total = 0;
	double		t;
	int		maxlen;

	triangles = x;
	pmesh = y;


	t = I_FloatTime();

	for (i=0 ; i<pmesh->numtris ; i++)
	{
		neighbortri[i][0] = neighbortri[i][1] = neighbortri[i][2] = -1;
		used[i] = 0;
		peak[i] = pmesh->numtris;
	}

	// printf("finding neighbors\n");
	for (i=0 ; i<pmesh->numtris; i++)
	{
		for (k = 0; k < 3; k++)
		{
			if (used[i] & (1 << k))
				continue;

			FindNeighbor( i, k );
		}
		// printf("%d", used[i] );
	}
	// printf("\n");

	//
	// build tristrips
	//
	numcommandnodes = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));

	for (i=0 ; i<pmesh->numtris ;)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
		{
			i++;
			continue;
		}

		maxlen = 9999;
		bestlen = 0;
		m = 0;
		for (k = i; k < pmesh->numtris && bestlen < 127; k++)
		{
			int localpeak = 0;

			if (used[k])
				continue;

			if (peak[k] <= bestlen)
				continue;

			m++;
			for (type = 0 ; type < 2 ; type++)
			{
				for (startv =0 ; startv < 3 ; startv++)
				{
					if (type == 1)
						len = FanLength (k, startv);
					else
						len = StripLength (k, startv);
					if (len > 127)
					{
						// skip these, they are too long to encode
					}
					else if (len > bestlen)
					{
						besttype = type;
						bestlen = len;
						for (j=0 ; j<bestlen ; j++)
						{
							besttris[j] = striptris[j];
							bestverts[j] = stripverts[j];
						}
						// printf("%d %d\n", k, bestlen );
					}
					if (len > localpeak)
						localpeak = len;
				}
			}
			peak[k] = localpeak;
			if (localpeak == maxlen)
				break;
		}
		total += (bestlen - 2);

		// printf("%d (%d) %d\n", bestlen, pmesh->numtris - total, i );

		maxlen = bestlen;

		// mark the tris on the best strip as used
		for (j=0 ; j<bestlen ; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = -bestlen;
		else
			commands[numcommands++] = bestlen;

		for (j=0 ; j<bestlen ; j++)
		{
			s_trianglevert_t *tri;

			tri = &triangles[besttris[j]][bestverts[j]];

			commands[numcommands++] = tri->vertindex;
			commands[numcommands++] = tri->normindex;

			if(( g_texture[pmesh->skinref].flags & STUDIO_NF_CHROME ) || !store_uv_coords )
			{
				commands[numcommands++] = tri->s;
				commands[numcommands++] = tri->t;
			}
			else
			{
#if 0
				float error_u = fabs( tri->u - HalfToFloat( FloatToHalf( tri->u )));
				float error_v = fabs( tri->v - HalfToFloat( FloatToHalf( tri->v )));
				Msg( "store coord: ( %.7f %.7f ), error: ( %.7f %.7f )\n", tri->u, tri->v, error_u, error_v );
#endif
				commands[numcommands++] = FloatToHalf( tri->u );
				commands[numcommands++] = FloatToHalf( tri->v );
			}
		}
		// printf("%d ", bestlen - 2 );
		numcommandnodes++;

		if( t != I_FloatTime())
		{
			// don't write progress to log
			printf( "%2d%%\r", (total * 100) / pmesh->numtris );
			t = I_FloatTime();
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	*ppdata = (byte *)commands;

	// printf("%d %d %d\n", numcommandnodes, numcommands, pmesh->numtris  );
	return numcommands * sizeof( short );
}