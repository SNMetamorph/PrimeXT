/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// detail.c

#include "bsp5.h"

side_t *AllocSide( void )
{
	return (side_t *)Mem_Alloc( sizeof( side_t ), C_BRUSHSIDE );
}

void FreeSide( side_t *s )
{
	if( s->w ) FreeWinding( s->w );
	Mem_Free( s, C_BRUSHSIDE );
}

side_t *NewSideFromSide( const side_t *s )
{
	side_t	*news;

	news = AllocSide ();
	news->plane = s->plane;
	news->w = CopyWinding( s->w );

	return news;
}

brush_t *AllocBrush( void )
{
	return (brush_t *)Mem_Alloc( sizeof( brush_t ), C_BSPBRUSH );
}

void FreeBrush( brush_t *b )
{
	if( b->sides )
	{
		for( side_t *s = b->sides, *next = NULL; s != NULL; s = next )
		{
			next = s->next;
			FreeSide( s );
		}
	}

	Mem_Free( b, C_BSPBRUSH );
}

brush_t *NewBrushFromBrush( const brush_t *b )
{
	brush_t	*newb = AllocBrush ();

	for( side_t *s = b->sides, **pnews = &newb->sides; s != NULL; s = s->next, pnews = &(*pnews)->next )
		*pnews = NewSideFromSide( s );

	return newb;
}

void ClipBrush( brush_t **b, const plane_t *split, vec_t epsilon )
{
	side_t	*s, **pnext;
	winding_t	*w;

	for( pnext = &(*b)->sides, s = *pnext; s; s = *pnext )
	{
		if( ChopWindingInPlace( &s->w, split->normal, split->dist, epsilon, false ))
		{
			pnext = &s->next;
		}
		else
		{
			*pnext = s->next;
			FreeSide( s );
		}
	}

	if( !(*b)->sides )
	{
		// empty brush
		FreeBrush( *b );
		*b = NULL;
		return;
	}

	w = BaseWindingForPlane( split->normal, split->dist );

	for( s = (*b)->sides; s; s = s->next )
	{
		if( !ChopWindingInPlace( &w, s->plane.normal, s->plane.dist, epsilon, false ))
		{
			break;
		}
	}

	if( w->numpoints == 0 )
	{
		FreeWinding( w );
	}
	else
	{
		s = AllocSide();
		s->plane = *split;
		s->w = CopyWinding( w );
		s->next = (*b)->sides;
		(*b)->sides = s;
	}
}

void SplitBrush( brush_t *in, const plane_t *split, brush_t **front, brush_t **back )
{
	bool	onfront = false;
	bool	onback = false;
	side_t	*s;

	in->next = NULL;

	for( s = in->sides; s != NULL; s = s->next )
	{
		switch( WindingOnPlaneSide( s->w, split->normal, split->dist, ON_EPSILON * 2 ))
		{
		case SIDE_CROSS:
			onfront = true;
			onback = true;
			break;
		case SIDE_FRONT:
			onfront = true;
			break;
		case SIDE_BACK:
			onback = true;
			break;
		case SIDE_ON:
			break;
		}

		if( onfront && onback )
			break;
	}

	if( !onfront && !onback )
	{
		FreeBrush( in );
		*front = NULL;
		*back = NULL;
		return;
	}

	if( !onfront )
	{
		*front = NULL;
		*back = in;
		return;
	}

	if( !onback )
	{
		*front = in;
		*back = NULL;
		return;
	}

	*front = in;
	*back = NewBrushFromBrush( in );

	plane_t frontclip = *split;
	plane_t backclip = *split;

	VectorNegate( backclip.normal, backclip.normal );
	backclip.dist = -backclip.dist;

	ClipBrush( front, &frontclip, NORMAL_EPSILON );
	ClipBrush( back, &backclip, NORMAL_EPSILON );
}

brush_t *BrushFromBox( const vec3_t mins, const vec3_t maxs )
{
	brush_t	*b = AllocBrush ();
	plane_t	planes[6];
	int	k;

	for( k = 0; k < 3; k++ )
	{
		VectorClear( planes[k].normal );
		planes[k].normal[k] = 1.0;
		planes[k].dist = mins[k];
		VectorClear( planes[k+3].normal );
		planes[k+3].normal[k] = -1.0;
		planes[k+3].dist = -maxs[k];
	}

	b->sides = AllocSide();
	b->sides->plane = planes[0];
	b->sides->w = BaseWindingForPlane( planes[0].normal, planes[0].dist );

	for( k = 1; k < 6; k++ )
	{
		ClipBrush( &b, &planes[k], NORMAL_EPSILON );
		if( b == NULL ) break;
	}

	return b;
}

void CalcBrushBounds( const brush_t *b, vec3_t &mins, vec3_t &maxs )
{
	vec3_t	windingmins, windingmaxs;

	ClearBounds( mins, maxs );

	for( side_t *s = b->sides; s; s = s->next )
	{
		WindingBounds( s->w, windingmins, windingmaxs );
		AddPointToBounds( windingmins, mins, maxs ); 
		AddPointToBounds( windingmaxs, mins, maxs ); 
	}
}

brush_t *ReadBrushes( FILE *file )
{
	brush_t	*brushes = NULL;
	int	planenum, numpoints;
	int	r, brushinfo;

	while( 1 )
	{
		r = fscanf( file, "%i\n", &brushinfo );

		if( r == 0 || r == -1 )
		{
			if( brushes == NULL )
				COM_FatalError( "ReadBrushes: no more models\n" );
			else COM_FatalError( "ReadBrushes: file end\n" );
		}

		if( brushinfo == -1 )
			break; // end of detail brushes list

		brush_t *b;
		b = AllocBrush ();
		b->next = brushes;
		brushes = b;
		side_t **psn;
		psn = &b->sides;

		while( 1 )
		{

			r = fscanf( file, "%i %u\n", &planenum, &numpoints );
			if( r != 2 ) COM_FatalError( "ReadBrushes: get side failed\n" );

			if( planenum == -1 )
				break; // end of brushes description

			side_t *s = AllocSide();
			s->plane = g_mapplanes[planenum ^ 1];
			s->w = AllocWinding( numpoints );
			s->w->numpoints = numpoints;

			for( int x = 0; x < numpoints; x++ )
			{
				double v[3];
				r = fscanf( file, "%lf %lf %lf\n", &v[0], &v[1], &v[2] );
				if( r != 3 ) COM_FatalError( "ReadBrushes: get point failed\n" );
				VectorCopy( v, s->w->p[numpoints - 1 - x] );
			}

			s->next = NULL;
			*psn = s;
			psn = &s->next;
		}
	}

	return brushes;
}