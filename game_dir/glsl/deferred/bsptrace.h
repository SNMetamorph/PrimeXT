/*
bsptrace.h - GPU implementation of BSP-acellerated world trace
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#define FRAC_EPSILON	(1.0 / 32.0)
#define CONTENTS_EMPTY	-1.0
#define CONTENTS_SOLID	-2.0
#define CONTENTS_SKY	-6.0
#define MAX_STACK_DEPTH	10	// FIXME: this is not enough in some cases...

uniform sampler2DRect	u_BspPlanesMap;
uniform sampler2DRect	u_BspNodesMap;
uniform sampler2DRect	u_BspModelsMap;

uniform int		u_NumVisibleModels;

// data storage representation
// node vec4( children[0], children[1], planenum, unused );
// plane vec4( normal.x, normal.y, normal.z, dist );

vec4 fetchPlane( float number )
{
	// transform value into texcoords
	int x = int( mod( number, 256.0 ));
	int y = int( number / 256.0 );

	return texture2DRect( u_BspPlanesMap, ivec2( x, y ));
}

vec4 fetchNode( float number )
{
	// transform value into texcoords
	int x = int( mod( number, 256.0 ));
	int y = int( number / 256.0 );

	return texture2DRect( u_BspNodesMap, ivec2( x, y ));
}

float traceRay( const in vec3 p0, const in vec3 p1, float nodenum )
{
	vec3	stack_p1[MAX_STACK_DEPTH];
	vec2	stack_node[MAX_STACK_DEPTH];
	vec3	other_p0, other_p1;
	int	stack_pos = 0;
	vec4	node, plane;
	float	front, back;
	int	x, y;
	bool	side;
	float	frac;

	other_p0 = p0;
	other_p1 = p1;

	while( true )
	{
		while( nodenum < 0.0 )
		{
			if( nodenum == CONTENTS_SOLID )
				return 0.0; // occluded

			if( stack_pos == 0 || nodenum == CONTENTS_SKY )
				return 1.0; // passed

			// pop up the stack for a back side
			stack_pos--;
			nodenum = stack_node[stack_pos].x;
			side = bool( stack_node[stack_pos].y );

			// set the hit point for this plane
			other_p0 = other_p1;

			// go down the back side
			other_p1 = stack_p1[stack_pos];

			x = int( mod( nodenum, 256.0 ));
			y = int( nodenum / 256.0 );
			node = texture2DRect( u_BspNodesMap, ivec2( x, y ));
			nodenum = side ? node.x : node.y; // select children
		}

		x = int( mod( nodenum, 256.0 ));
		y = int( nodenum / 256.0 );
		node = texture2DRect( u_BspNodesMap, ivec2( x, y ));
		x = int( mod( node.z, 256.0 ));
		y = int( node.z / 256.0 );
		plane = texture2DRect( u_BspPlanesMap, ivec2( x, y ));
		front = dot( other_p0, plane.xyz ) - plane.w;
		back = dot( other_p1, plane.xyz ) - plane.w;

		if( front >= -FRAC_EPSILON && back >= -FRAC_EPSILON )
		{
			nodenum = node.x;	// children 0
			continue;
		}

		if( front < FRAC_EPSILON && back < FRAC_EPSILON )
		{
			nodenum = node.y;	// children 1
			continue;
		}

		side = bool( front < 0.0 );
		frac = saturate( front / (front - back));

		stack_node[stack_pos] = vec2( nodenum, float( side ));
		stack_p1[stack_pos] = other_p1;
		stack_pos++; // move stack

		other_p1 = mix( other_p0, other_p1, frac );
		nodenum = side ? node.y : node.x; // select children

		if( stack_pos >= MAX_STACK_DEPTH )
			break; // stack overflow
	}
	return 1.0;
}

/*
==================
MoveBounds
==================
*/
void MoveBounds( const vec3 start, const vec3 end, out vec3 boxmins, out vec3 boxmaxs )
{
	if( end.x > start.x )
	{
		boxmins.x = start.x - 1.0;
		boxmaxs.x = end.x + 1.0;
	}
	else
	{
		boxmins.x = end.x - 1.0;
		boxmaxs.x = start.x + 1.0;
	}

	if( end.y > start.y )
	{
		boxmins.y = start.y - 1.0;
		boxmaxs.y = end.y + 1.0;
	}
	else
	{
		boxmins.y = end.y - 1.0;
		boxmaxs.y = start.y + 1.0;
	}

	if( end.z > start.z )
	{
		boxmins.z = start.z - 1.0;
		boxmaxs.z = end.z + 1.0;
	}
	else
	{
		boxmins.z = end.z - 1.0;
		boxmaxs.z = start.z + 1.0;
	}
}

/*
=================
BoundsIntersect
=================
*/
bool BoundsIntersect( const vec3 mins1, const vec3 maxs1, const vec3 mins2, const vec3 maxs2 )
{
	if( mins1.x > maxs2.x || mins1.y > maxs2.y || mins1.z > maxs2.z )
		return false;
	if( maxs1.x < mins2.x || maxs1.y < mins2.y || maxs1.z < mins2.z )
		return false;
	return true;
}

float traceRay2( const in vec3 p0, const in vec3 p1 )
{
	float frac = traceRay( p0, p1, 0.0 );

	if( bool( u_NumVisibleModels <= 0 ))
		return frac;

	if( bool( frac != 0.0 ))
	{
		vec3	trace_mins;
		vec3	trace_maxs;
		vec3 end = mix( p0, p1, frac );
		MoveBounds( p0, end, trace_mins, trace_maxs );

		for( int i = 0; i < u_NumVisibleModels; i++ )
		{
			vec4 absmin = texture2DRect( u_BspModelsMap, ivec2( 0, i ));
			vec4 absmax = texture2DRect( u_BspModelsMap, ivec2( 1, i ));
			vec3 start_l = p0, end_l = end;

			if( !BoundsIntersect( trace_mins, trace_maxs, absmin.xyz, absmax.xyz ))
				continue;	// no intersection

			// transform to model space
			if( bool( absmin.w != 0.0 ))
			{
				mat4	world;

				world[0] = texture2DRect( u_BspModelsMap, ivec2( 2, i ));
				world[1] = texture2DRect( u_BspModelsMap, ivec2( 3, i ));
				world[2] = texture2DRect( u_BspModelsMap, ivec2( 4, i ));
				world[3] = texture2DRect( u_BspModelsMap, ivec2( 5, i ));

				start_l = (vec4( p0, 1.0 ) * world).xyz;
				end_l = (vec4( end, 1.0 ) * world).xyz;
			}

			float frac2 = traceRay( start_l, end_l, absmax.w );
			if( frac2 < frac ) frac = frac2;
			if( frac2 <= 0.0 ) return frac;
		}
	}

	return frac;
}