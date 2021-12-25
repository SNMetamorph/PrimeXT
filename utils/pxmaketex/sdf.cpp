/*
imagelib.cpp - simple loader\serializer for TGA & BMP
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "conprint.h"
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include "cmdlib.h"
#include "stringlib.h"
#include "imagelib.h"
#include "filesystem.h"
#include "mathlib.h"

#define DISTAA( c, xc, yc, xi, yi )	( distaa3( img, gx, gy, w, c, xc, yc, xi, yi ))
#define SQRT2			1.4142136

/*
 * Compute the local gradient at edge pixels using convolution filters.
 * The gradient is computed only at edge pixels. At other places in the
 * image, it is never used, and it's mostly zero anyway.
*/
void computegradient( double *img, int w, int h, double *gx, double *gy )
{
	double	glength;

	for( int i = 1; i < h - 1; i++ )
	{
		// avoid edges where the kernels would spill over
		for( int j = 1; j < w - 1; j++ )
		{
			int	k = i*w + j;

			if(( img[k] > 0.0 ) && ( img[k] < 1.0 ))
			{
				// compute gradient for edge pixels only
				gx[k] = -img[k-w-1] - SQRT2 * img[k-1] - img[k+w-1] + img[k-w+1] + SQRT2 * img[k+1] + img[k+w+1];
				gy[k] = -img[k-w-1] - SQRT2 * img[k-w] - img[k+w-1] + img[k-w+1] + SQRT2 * img[k+w] + img[k+w+1];
				glength = gx[k] * gx[k] + gy[k] * gy[k];

				if( glength > 0.0 )
				{
					// avoid division by zero
					glength = sqrt( glength );
					gx[k] = gx[k] / glength;
					gy[k] = gy[k] / glength;
				}
			}
		}
	}
}

/*
 * A somewhat tricky function to approximate the distance to an edge in a
 * certain pixel, with consideration to either the local gradient (gx,gy)
 * or the direction to the pixel (dx,dy) and the pixel greyscale value a.
 * The latter alternative, using (dx,dy), is the metric used by edtaa2().
 * Using a local estimate of the edge gradient (gx,gy) yields much better
 * accuracy at and near edges, and reduces the error even at distant pixels
 * provided that the gradient direction is accurately estimated.
*/
double edgedf( double gx, double gy, double a )
{
	double	df, glength, temp, a1;

	if(( gx == 0 ) || ( gy == 0 ))
	{
		// Either A) gu or gv are zero, or B) both
		df = 0.5 - a;  // Linear approximation is A) correct or B) a fair guess
	}
	else
	{
		glength = sqrt( gx * gx + gy * gy );

		if( glength > 0 )
		{
			gx = gx / glength;
			gy = gy / glength;
		}

		/* Everything is symmetric wrt sign and transposition,
		* so move to first octant (gx>=0, gy>=0, gx>=gy) to
		* avoid handling all possible edge directions.
		*/

		gx = fabs(gx);
		gy = fabs(gy);

		if( gx < gy )
		{
			temp = gx;
			gx = gy;
			gy = temp;
		}

		a1 = 0.5 * gy / gx;

		if( a < a1 )
		{
			// 0 <= a < a1
			df = 0.5 * (gx + gy) - sqrt( 2.0 * gx * gy * a );
		}
		else if( a < ( 1.0 - a1 ))
		{
			// a1 <= a <= 1-a1
			df = (0.5 - a) * gx;
		}
		else
		{
			// 1 - a1 < a <= 1
			df = -0.5 * (gx + gy) + sqrt( 2.0 * gx * gy * ( 1.0 - a ));
		}
	}    

	return df;
}

double distaa3( double *img, double *gximg, double *gyimg, int w, int c, int xc, int yc, int xi, int yi )
{
	double	di, df, dx, dy, gx, gy, a;
	int	closest;
  
	closest = c - xc - yc * w;	// index to the edge pixel pointed to from c
	a = img[closest];		// grayscale value at the edge pixel
	gx = gximg[closest];	// X gradient component at the edge pixel
	gy = gyimg[closest];	// Y gradient component at the edge pixel

	a = bound( 0.0, a, 1.0 );	// clip grayscale values outside the range [0,1]

	if( a == 0.0 )
		return 1000000.0;	// not an object pixel, return "very far" ("don't know yet")

	dx = (double)xi;
	dy = (double)yi;
	di = sqrt( dx * dx + dy * dy );	// length of integer vector, like a traditional EDT

	if( di == 0 )
	{
		// use local gradient only at edges
		// Estimate based on local gradient only
		df = edgedf( gx, gy, a );
	}
	else
	{
		// estimate gradient based on direction to edge (accurate for large di)
		df = edgedf( dx, dy, a );
	}

	return di + df; // Same metric as edtaa2, except at edges (where di=0)
}

void edtaa3( double *img, double *gx, double *gy, int w, int h, short *distx, short *disty, double *dist )
{
	double	threshold = 1e-6;
	double	olddist, newdist;
	int	offset_u, offset_ur, offset_r, offset_rd, offset_d, offset_dl, offset_l, offset_lu;
	int	cdistx, cdisty, newdistx, newdisty;
	int	changed, x, y, i, c;

	// initialize index offsets for the current image width
	offset_u = -w;
	offset_ur = -w + 1.0;
	offset_r = 1.0;
	offset_rd = w + 1.0;
	offset_d = w;
	offset_dl = w - 1.0;
	offset_l = -1.0;
	offset_lu = -w - 1.0;

	// initialize the distance images
	for( i = 0; i < w * h; i++ )
	{
		distx[i] = 0; // at first, all pixels point to
		disty[i] = 0; // themselves as the closest known.

		if( img[i] <= 0.0 )
		{
			// big value, means "not set yet"
			dist[i] = 1000000.0;
		}
		else if( img[i] < 1.0 )
		{
			// gradient-assisted estimate
			dist[i] = edgedf( gx[i], gy[i], img[i] );
		}
		else
		{
			// inside the object
			dist[i]= 0.0;
		}
	}

	// Perform the transformation
	do
	{
		changed = 0;

		// scan rows, except first row
		for( y = 1; y < h; y++ )
		{
			// move index to leftmost pixel of current row
			i = y * w;

			// scan right, propagate distances from above & left

			// leftmost pixel is special, has no left neighbors
			olddist = dist[i];

			// if non-zero distance or not set yet
			if( olddist > 0 )
			{
				c = i + offset_u;	// index of candidate for testing
				cdistx = distx[c];
				cdisty = disty[c];
				newdistx = cdistx;
				newdisty = cdisty+1;
				newdist = DISTAA( c, cdistx, cdisty, newdistx, newdisty );

				if( newdist < ( olddist - threshold ))
				{
					distx[i] = newdistx;
					disty[i] = newdisty;
					dist[i] = newdist;
					olddist = newdist;
					changed = 1;
				}

				c = i + offset_ur;
				cdistx = distx[c];
				cdisty = disty[c];
				newdistx = cdistx - 1;
				newdisty = cdisty + 1;
				newdist = DISTAA( c, cdistx, cdisty, newdistx, newdisty );

				if( newdist < ( olddist - threshold ))
				{
					distx[i] = newdistx;
					disty[i] = newdisty;
					dist[i] = newdist;
					changed = 1;
				}
			}
			i++;

			// middle pixels have all neighbors
			for( x = 1; x < w - 1; x++, i++ )
			{
				olddist = dist[i];

				if( olddist <= 0 )
					continue; // no need to update further

				c = i + offset_l;
				cdistx = distx[c];
				cdisty = disty[c];
				newdistx = cdistx + 1.0;
				newdisty = cdisty;
				newdist = DISTAA( c, cdistx, cdisty, newdistx, newdisty );

				if( newdist < ( olddist - threshold ))
				{
					distx[i] = newdistx;
					disty[i] = newdisty;
					dist[i] = newdist;
					olddist = newdist;
					changed = 1;
				}

				c = i + offset_lu;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty+1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_u;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx;
                newdisty = cdisty+1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_ur;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty+1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }

            /* Rightmost pixel of row is special, has no right neighbors */
            olddist = dist[i];
            if(olddist > 0) // If not already zero distance
            {
                c = i+offset_l;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_lu;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty+1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_u;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx;
                newdisty = cdisty+1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }

            /* Move index to second rightmost pixel of current row. */
            /* Rightmost pixel is skipped, it has no right neighbor. */
            i = y*w + w-2;

            /* scan left, propagate distance from right */
            for(x=w-2; x>=0; x--, i--)
            {
                olddist = dist[i];
                if(olddist <= 0) continue; // Already zero distance

                c = i+offset_r;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }
        }
      
        /* Scan rows in reverse order, except last row */
        for(y=h-2; y>=0; y--)
        {
            /* move index to rightmost pixel of current row */
            i = y*w + w-1;

            /* Scan left, propagate distances from below & right */

            /* Rightmost pixel is special, has no right neighbors */
            olddist = dist[i];
            if(olddist > 0) // If not already zero distance
            {
                c = i+offset_d;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_dl;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }
            i--;

            /* Middle pixels have all neighbors */
            for(x=w-2; x>0; x--, i--)
            {
                olddist = dist[i];
                if(olddist <= 0) continue; // Already zero distance

                c = i+offset_r;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_rd;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_d;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_dl;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }
            /* Leftmost pixel is special, has no left neighbors */
            olddist = dist[i];
            if(olddist > 0) // If not already zero distance
            {
                c = i+offset_r;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_rd;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx-1;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    olddist=newdist;
                    changed = 1;
                }

                c = i+offset_d;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx;
                newdisty = cdisty-1;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }

            /* Move index to second leftmost pixel of current row. */
            /* Leftmost pixel is skipped, it has no left neighbor. */
            i = y*w + 1;
            for(x=1; x<w; x++, i++)
            {
                /* scan right, propagate distance from left */
                olddist = dist[i];
                if(olddist <= 0) continue; // Already zero distance

                c = i+offset_l;
                cdistx = distx[c];
                cdisty = disty[c];
                newdistx = cdistx+1;
                newdisty = cdisty;
                newdist = DISTAA(c, cdistx, cdisty, newdistx, newdisty);
                if(newdist < (olddist-threshold))
                {
                    distx[i]=newdistx;
                    disty[i]=newdisty;
                    dist[i]=newdist;
                    changed = 1;
                }
            }
        }
    }
    while(changed); // Sweep until no more updates are made

    /* The transformation is completed. */

}

void Image_MakeSignedDistanceField( rgbdata_t *pic )
{
	if( FBitSet( pic->flags, IMAGE_DXT_FORMAT ))
		return; // can't merge compressed formats

	if( !FBitSet( pic->flags, IMAGE_HAS_1BIT_ALPHA ))
		return; // generate SDF from onebit alpha

	short	*xdist = (short *)Mem_Alloc( pic->width * pic->height * sizeof( short ));
	short	*ydist = (short *)Mem_Alloc( pic->width * pic->height * sizeof( short ));
	double	*gx = (double *)Mem_Alloc( pic->width * pic->height * sizeof( double ));
	double	*gy = (double *)Mem_Alloc( pic->width * pic->height * sizeof( double ));
	double	*data = (double *)Mem_Alloc( pic->width * pic->height * sizeof( double ));
	double	*outside = (double *)Mem_Alloc( pic->width * pic->height * sizeof( double ));
	double	*inside = (double *)Mem_Alloc( pic->width * pic->height * sizeof( double ));
	double	img_min = 255, img_max = -255;
	int	i;

	// convert img into double (data)
	for( i = 0; i < pic->width * pic->height; ++i )
	{
		data[i] = pic->buffer[i*4+3];

		img_min = Q_min( data[i], img_min );
		img_max = Q_max( data[i], img_max );
	}

	// rescale image levels between 0 and 1
	for( i = 0; i < pic->width * pic->height; ++i )
	{
		data[i] = ( pic->buffer[i*4+3] - img_min ) / img_max;
	}

	// compute outside = edtaa3( bitmap ); % Transform background (0's)
	computegradient( data, pic->height, pic->width, gx, gy );
	edtaa3( data, gx, gy, pic->height, pic->width, xdist, ydist, outside );
	for( i = 0; i < pic->width * pic->height; ++i )
		outside[i] = Q_max( 0.0, outside[i] );

	// compute inside = edtaa3( 1 - bitmap ); % Transform foreground (1's)
	memset( gx, 0, pic->width * pic->height * sizeof( double ));
	memset( gy, 0, pic->width * pic->height * sizeof( double ));

	for( i = 0; i < pic->width * pic->height; ++i )
		data[i] = 1.0 - data[i];

	computegradient( data, pic->height, pic->width, gx, gy );
	edtaa3( data, gx, gy, pic->height, pic->width, xdist, ydist, inside );
	for( i = 0; i < pic->width * pic->height; ++i )
		inside[i] = Q_max( 0.0, inside[i] );

	// distmap = outside - inside; % Bipolar distance field
	for( i = 0; i < pic->width * pic->height; ++i )
	{
		outside[i] -= inside[i];
		outside[i] = 128 + outside[i] * 8;
		outside[i] = bound( 0, outside[i], 255 );
		pic->buffer[i*4+3] = 255 - (byte)outside[i];
	}

	ClearBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
	ClearBits( pic->flags, IMAGE_HAS_8BIT_ALPHA );
	SetBits( pic->flags, IMAGE_HAS_SDF_ALPHA );

	Mem_Free( xdist );
	Mem_Free( ydist );
	Mem_Free( gx );
	Mem_Free( gy );
	Mem_Free( data );
	Mem_Free( outside );
	Mem_Free( inside );
}