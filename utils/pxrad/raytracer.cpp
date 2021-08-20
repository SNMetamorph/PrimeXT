/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// raytracer.cpp

#include "qrad.h"
#include "raytracer.h"
#include "..\..\engine\studio.h"
#include "model_trace.h"

#ifdef HLRAD_RAYTRACE
void CWorldRayTrace :: AddTriangle( tface_t *tf )
{
	if( !tf->shadow ) return;
	m_TriangleList.AddToTail( tf );
}

float CWorldRayTrace :: BoxSurfaceArea( const vec3_t boxmin, const vec3_t boxmax )
{
	vec3_t	boxdim;
	vec_t	area;

	VectorSubtract( boxmax, boxmin, boxdim );
	area = 2.0 * ((boxdim[0] * boxdim[2]) + (boxdim[0] * boxdim[1]) + (boxdim[1] * boxdim[2]));
	if( area == 0.0f ) area = FLT_EPSILON; // to prevent INF

	assert( area != 0.0 );

	return area;
}

int CWorldRayTrace :: MakeLeafNode( int first_tri, int last_tri )
{
	KDNode ret;

	ret.m_iChild = KDNODE_STATE_LEAF + (m_TriangleIndexList.Count() << 2);
	ret.SetNumberOfTrianglesInLeafNode( 1 + (last_tri - first_tri));

	for( int tnum = first_tri; tnum <= last_tri; tnum++ )
		m_TriangleIndexList.AddToTail( tnum );

	m_KDTree.AddToTail( ret );
	return m_KDTree.Count() - 1;
}

void CWorldRayTrace :: CalculateTriangleListBounds( const int *tris, int ntris, vec3_t minout, vec3_t maxout )
{
	VectorSet( minout,  1.0e23,  1.0e23,  1.0e23 );
	VectorSet( maxout, -1.0e23, -1.0e23, -1.0e23 );

	for( int i = 0; i < ntris; i++ )
	{
		const tface_t *face = m_TriangleList[tris[i]];

		for( int c = 0; c < 3; c++ )
		{
			minout[c] = Q_min( minout[c], mesh->verts[face->a].point[c] );
			maxout[c] = Q_max( maxout[c], mesh->verts[face->a].point[c] );
			minout[c] = Q_min( minout[c], mesh->verts[face->b].point[c] );
			maxout[c] = Q_max( maxout[c], mesh->verts[face->b].point[c] );
			minout[c] = Q_min( minout[c], mesh->verts[face->c].point[c] );
			maxout[c] = Q_max( maxout[c], mesh->verts[face->c].point[c] );
		}
	}
}

float CWorldRayTrace :: CalculateCostsOfSplit( int plane, int *list, int ntris, vec3_t absmin, vec3_t absmax, float &value, int &nleft, int &nright, int &nboth )
{
	float	min_coord =  1.0e23;
	float	max_coord = -1.0e23;

	// determine the costs of splitting on a given axis, and label triangles with respect to
	// that axis by storing the value in coordselect0. It will also return the number of
	// tris in the left, right, and nboth groups, in order to facilitate memory
	nleft = nboth = nright = 0;

	// now, label each triangle. Since we have not converted the triangles into
	// intersection fromat yet, we can use the CoordSelect0 field of each as a temp.
	for( int t = 0; t < ntris; t++ )
	{
		tface_t *face = m_TriangleList[list[t]];

		// determine max and min coordinate values for later optimization
		min_coord = Q_min( min_coord, mesh->verts[face->a].point[plane] );
		max_coord = Q_max( max_coord, mesh->verts[face->a].point[plane] );
		min_coord = Q_min( min_coord, mesh->verts[face->b].point[plane] );
		max_coord = Q_max( max_coord, mesh->verts[face->b].point[plane] );
		min_coord = Q_min( min_coord, mesh->verts[face->c].point[plane] );
		max_coord = Q_max( max_coord, mesh->verts[face->c].point[plane] );

		switch( face->ClassifyAgainstAxisSplit( mesh->verts, plane, value ))
		{
		case PLANECHECK_NEGATIVE:
			face->pcheck0 = PLANECHECK_NEGATIVE;
			nleft++;
			break;
		case PLANECHECK_POSITIVE:
			face->pcheck0 = PLANECHECK_POSITIVE;
			nright++;
			break;
		case PLANECHECK_STRADDLING:
			face->pcheck0 = PLANECHECK_STRADDLING;
			nboth++;
			break;
		default:
			ASSERT( 0 );
			break;
		}
	}

	// now, if the split resulted in one half being empty, "grow" the empty half
	if( nleft && (nboth == 0) && (nright == 0))
		value = max_coord;
	if( nright && (nboth == 0) && (nleft == 0))
		value = min_coord;

	vec3_t	leftMins, leftMaxs;
	vec3_t	rightMins, rightMaxs;

	// now, perform surface area/cost check to determine whether this split was worth it
	VectorCopy( absmin, leftMins );
	VectorCopy( absmax, leftMaxs );
	VectorCopy( absmin, rightMins );
	VectorCopy( absmax, rightMaxs );

	leftMaxs[plane] = value;
	rightMins[plane] = value;

	float SA_L = BoxSurfaceArea( leftMins, leftMaxs );
	float SA_R = BoxSurfaceArea( rightMins, rightMaxs );
	float ISA = 1.0 / BoxSurfaceArea( absmin, absmax );

	return COST_OF_TRAVERSAL + COST_OF_INTERSECTION * (nboth + (SA_L * ISA * nleft) + (SA_R * ISA * nright));
}

void CWorldRayTrace :: RefineNode( int node_number, int *list, int ntris, vec3_t absmin, vec3_t absmax, int depth )
{
	// never split empty lists
	if( ntris < 3 )
	{
		// no point in continuing
		m_KDTree[node_number].m_iChild = KDNODE_STATE_LEAF + (m_TriangleIndexList.Count() << 2);
		m_KDTree[node_number].SetNumberOfTrianglesInLeafNode( ntris );

		for( int t = 0; t < ntris; t++ )
			m_TriangleIndexList.AddToTail( list[t] );
		return;
	}

	float best_splitvalue = 0;
	float best_cost = 1.0e23;
	int best_nleft = 0;
	int best_nright = 0;
	int best_nboth = 0;
	int split_plane = 0;

	// don't try all trinagles as split
	int tri_skip = 1 + (ntris / 10);

	// points when there are a lot of them
	for( int axis = 0; axis < 3; axis++ )
	{
		for( int ts = -1; ts < ntris; ts += tri_skip )
		{
			for( int tv = 0; tv < 3; tv++ )
			{
				int trial_nleft, trial_nright, trial_nboth;
				float trial_splitvalue;

				if( ts == -1 )
				{
					trial_splitvalue = 0.5f * (absmin[axis] + absmax[axis]);
				}
				else
				{
					// else, split at the triangle vertex if possible
					tface_t *face = m_TriangleList[list[ts]];
					int vert = -1;

					switch( tv )
					{
					case 0: vert = face->a; break;
					case 1: vert = face->b; break;
					case 2: vert = face->c; break;
					}

					assert( vert != -1 );
					trial_splitvalue = mesh->verts[vert].point[axis];

					if(( trial_splitvalue > absmax[axis] ) || ( trial_splitvalue < absmin[axis] ))
						continue; // don't try this vertex - not inside
					
				}

				float trial_cost = CalculateCostsOfSplit( axis, list, ntris, absmin, absmax,
						trial_splitvalue, trial_nleft, trial_nright, trial_nboth );

				if( trial_cost < best_cost )
				{
					split_plane = axis;
					best_cost = trial_cost;
					best_nleft = trial_nleft;
					best_nright = trial_nright;
					best_nboth = trial_nboth;
					best_splitvalue = trial_splitvalue;

					// save away the axis classification of each triangle
					for( int t = 0; t < ntris; t++ )
					{
						tface_t *face = m_TriangleList[list[t]];
						face->pcheck1 = face->pcheck0;
					}
				}

				if( ts == -1 ) break;
			}
		}

	}

	float cost_of_no_split = COST_OF_INTERSECTION * ntris;

	if(( cost_of_no_split <= best_cost ) || NEVER_SPLIT || ( depth > MAX_TREE_DEPTH ))
	{
		// no benefit to splitting. just make this a leaf node
		m_KDTree[node_number].m_iChild = KDNODE_STATE_LEAF + (m_TriangleIndexList.Count() << 2);
		m_KDTree[node_number].SetNumberOfTrianglesInLeafNode( ntris );

		for( int t = 0; t < ntris; t++ )
			m_TriangleIndexList.AddToTail( list[t] );
	}
	else
	{
		// its worth splitting!
		// we will achieve the splitting without sorting by using a selection algorithm.
		int *new_triangle_list = new int[ntris];

		vec3_t	leftMins, leftMaxs;
		vec3_t	rightMins, rightMaxs;
		int	n_right_output = 0;
		int	n_left_output = 0;
		int	n_both_output = 0;

		// now, perform surface area/cost check to determine whether this split was worth it
		VectorCopy( absmin, leftMins );
		VectorCopy( absmax, leftMaxs );
		VectorCopy( absmin, rightMins );
		VectorCopy( absmax, rightMaxs );

		leftMaxs[split_plane] = best_splitvalue;
		rightMins[split_plane] = best_splitvalue;
#ifdef _DEBUG
		memset( new_triangle_list, 0, sizeof( int ) * ntris );
#endif
		for( int t = 0; t < ntris; t++ )
		{
			tface_t *face = m_TriangleList[list[t]];

			switch( face->pcheck1 )
			{
			case PLANECHECK_NEGATIVE:
				new_triangle_list[n_left_output++] = list[t];
				break;
			case PLANECHECK_POSITIVE:
				n_right_output++;
				new_triangle_list[ntris - n_right_output] = list[t];
				break;
			case PLANECHECK_STRADDLING:
				new_triangle_list[best_nleft + n_both_output] = list[t];
				n_both_output++;
				break;
			default:
				ASSERT( 0 );
				break;
			}
		}

		int left_child = m_KDTree.Count();
		int right_child = left_child + 1;

		m_KDTree[node_number].m_iChild = split_plane + (left_child<<2);
		m_KDTree[node_number].m_flSplitValue = best_splitvalue;

		KDNode newnode;

		m_KDTree.AddToTail( newnode );
		m_KDTree.AddToTail( newnode );

		// now, recurse!
		if(( ntris < 20) && ((best_nleft == 0) || (best_nright == 0 )))
			depth += 100;

		RefineNode( left_child, new_triangle_list, best_nleft + best_nboth, leftMins, leftMaxs, depth + 1 );
		RefineNode( right_child, new_triangle_list + best_nleft, best_nright + best_nboth, rightMins, rightMaxs, depth + 1 );
		delete[] new_triangle_list;
	}	
}

void CWorldRayTrace :: BuildTree( tmesh_t *src )
{
	KDNode root;

	m_KDTree.AddToTail( root );
	mesh = src;

	int *root_triangle_list = new int[m_TriangleList.Count()];

	for( int t = 0; t < m_TriangleList.Count(); t++ )
		root_triangle_list[t] = t;

	CalculateTriangleListBounds( root_triangle_list, m_TriangleList.Count(), m_AbsMins, m_AbsMaxs );
	RefineNode( 0, root_triangle_list, m_TriangleList.Count(), m_AbsMins, m_AbsMaxs, 0 );
	delete[] root_triangle_list;
}

void CWorldRayTrace :: TraceRay( const vec3_t start, const vec3_t stop, trace_t *trace )
{
	vec3_t	direction;
	vec_t	dist;

	VectorSubtract( stop, start, direction );
	dist = VectorNormalize( direction );
	trace->fraction = dist;

	TraceRay( 0.0f, dist, start, direction, trace );
	trace->fraction = trace->fraction / dist;
}

_forceinline float Reciprocal( const float a )
{
	float ret = 1.0 / a;
	// newton iteration is: Y(n+1) = 2*Y(n)-a*Y(n)^2
	ret = (( ret + ret ) - ( a * ( ret * ret )));
	return ret;
}

bool CWorldRayTrace :: TraceTexture( const tface_t *face, float u, float v, float w )
{
	// calculate st from uvw (barycentric) coordinates
	float	s = w * mesh->verts[face->a].st[0] + u * mesh->verts[face->b].st[0] + v * mesh->verts[face->c].st[0];
	float	t = w * mesh->verts[face->a].st[1] + u * mesh->verts[face->b].st[1] + v * mesh->verts[face->c].st[1];

	// convert ST to real pixels position
	int	x = fix_coord( s * face->texture->width, face->texture->width - 1 );
	int	y = fix_coord( t * face->texture->height, face->texture->height - 1 );

	// test pixel
	if( face->texture->data[(face->texture->width * y) + x] != 255 )
	{
		// at this point we hit the opaque pixel
		return true;
	}

	return false;
}

void CWorldRayTrace :: TraceRay( vec_t p1f, vec_t p2f, const vec3_t start, const vec3_t direction, trace_t *trace )
{
	vec3_t OneOverRayDir;
	int c;

	VectorCopy( direction, OneOverRayDir );

	// add epsilon to avoid division by zero
	for( c = 0; c < 3; c++ )
	{
		if( OneOverRayDir[c] == 0.0f )
			OneOverRayDir[c] = FLT_EPSILON;
		OneOverRayDir[c] = Reciprocal( OneOverRayDir[c] );
	}

	// now, clip rays against bounding box
	for( c = 0; c < 3; c++ )
	{
		float isect_min_t =	( m_AbsMins[c] - start[c] ) * OneOverRayDir[c];
		float isect_max_t =	( m_AbsMaxs[c] - start[c] ) * OneOverRayDir[c];
		p1f = Q_min( p1f, Q_min( isect_min_t, isect_max_t ));
		p2f = Q_max( p2f, Q_max( isect_min_t, isect_max_t ));
	}

	if( p1f > p2f ) return;

	// based on ray direction, whether to
	// visit left or right node first
	int front_idx[3], back_idx[3];
	int signbits = SignbitsForPlane( direction );

	if( FBitSet( signbits, 1 ))
	{
		back_idx[0] = 0;
		front_idx[0] = 1;
	}
	else
	{
		back_idx[0] = 1;
		front_idx[0] = 0;
	}

	if( FBitSet( signbits, 2 ))
	{
		back_idx[1] = 0;
		front_idx[1] = 1;
	}
	else
	{
		back_idx[1] = 1;
		front_idx[1] = 0;
	}

	if( FBitSet( signbits, 4 ))
	{
		back_idx[2] = 0;
		front_idx[2] = 1;
	}
	else
	{
		back_idx[2] = 1;
		front_idx[2] = 0;
	}

	NodeToVisit NodeQueue[MAX_NODE_STACK_LEN];
	NodeToVisit *stack_ptr = &NodeQueue[MAX_NODE_STACK_LEN];
	int mailboxids[MAILBOX_HASH_SIZE]; // used to avoid redundant triangle tests
	KDNode const *CurNode = &(m_KDTree[0]);
	memset( mailboxids, 0xff, sizeof( mailboxids ));	// !!speed!! keep around?

	while( 1 )
	{
		// traverse until next leaf
		while( CurNode->NodeType() != KDNODE_STATE_LEAF )
		{	   
			KDNode const *FrontChild = &(m_KDTree[CurNode->LeftChild()]);
			int split_plane_number = CurNode->NodeType();

			// dist = (split - org) / dir			
			float dist_to_sep_plane = ( CurNode->m_flSplitValue - start[split_plane_number] ) * OneOverRayDir[split_plane_number];
			bool active = ( p1f <= p2f ); // mask of which rays are active
			bool hits_front = active && ( dist_to_sep_plane >= p1f );

			// now, decide how to traverse children. can either do front, back, or do front and push back.
			if( !hits_front )
			{
				// missed the front. only traverse back
				CurNode = FrontChild + back_idx[split_plane_number];
				p1f = Q_max( p1f, dist_to_sep_plane );

			}
			else
			{
				bool hits_back = active && ( dist_to_sep_plane <= p2f );

				if( !hits_back )
				{
					// missed the back - only need to traverse front node
					CurNode = FrontChild + front_idx[split_plane_number];
					p2f = Q_min( p2f, dist_to_sep_plane );
				}
				else
				{
					// at least some rays hit both nodes.
					// must push far, traverse near
					assert( stack_ptr > NodeQueue );
					stack_ptr--;
					stack_ptr->node = FrontChild + back_idx[split_plane_number];
					stack_ptr->p1f = Q_max( p1f, dist_to_sep_plane );
					stack_ptr->p2f = p2f;
					CurNode = FrontChild + front_idx[split_plane_number];
					p2f = Q_min( p2f, dist_to_sep_plane );
				}
			}
		}

		// hit a leaf! must do intersection check
		int ntris = CurNode->NumberOfTrianglesInLeaf();

		if( ntris )
		{
			int const *tlist = &(m_TriangleIndexList[CurNode->TriangleIndexStart()]);

			do
			{
				int tnum = *(tlist++);

				// check mailbox
				int mbox_slot = tnum & (MAILBOX_HASH_SIZE - 1);
				const tface_t *face = m_TriangleList[tnum];

				if( mailboxids[mbox_slot] != tnum )
				{
					mailboxids[mbox_slot] = tnum;

					// compute plane intersection
					float DDotN = DotProduct( direction, face->normal );

					// mask off zero or near zero (ray parallel to surface)
					bool did_hit = (( DDotN > FLT_EPSILON ) || ( DDotN < -FLT_EPSILON ));

					if( !FBitSet( mesh->flags, FMESH_VERTEX_LIGHTING|FMESH_MODEL_LIGHTMAPS ))
					{
						if( !FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ))
						{
							// NOTE: probably we should normalize the n but for speed reasons i don't do it
							if( DDotN < -FLT_EPSILON ) continue;
						}
					}

					if( !did_hit ) continue; // to prevent division by zero

					float numerator = face->NdotP1 - DotProduct( start, face->normal );
					float isect_t = numerator / DDotN; // fraction

					// now, we have the distance to the plane. lets update our mask
					did_hit = did_hit && ( isect_t > m_flBackFrac );
					did_hit = did_hit && ( isect_t < trace->fraction );
					if( !did_hit ) continue;

					// now, check 3 edges
					float hitc1 = start[face->pcoord0] + ( isect_t * direction[face->pcoord0] );
					float hitc2 = start[face->pcoord1] + ( isect_t * direction[face->pcoord1] );
					
					// do barycentric coordinate check
					float B0 = face->edge1[0] * hitc1 + face->edge1[1] * hitc2 + face->edge1[2];
					did_hit = did_hit && ( B0 >= 0.0f );

					float B1 = face->edge2[0] * hitc1 + face->edge2[1] * hitc2 + face->edge2[2];
					did_hit = did_hit && ( B1 >= 0.0f );

					float B2 = B0 + B1;
					did_hit = did_hit && ( B2 <= 1.0f );

					if( !did_hit ) continue;

					// if the triangle is transparent
					if( face->texture->data )
					{
						// assuming a triangle indexed as v0, v1, v2
						// the projected edge equations are set up such that the vert opposite the first
						// equation is v2, and the vert opposite the second equation is v0
						// Therefore we pass them back in 1, 2, 0 order
						// Also B2 is currently B1 + B0 and needs to be 1 - (B1+B0) in order to be a real
						// barycentric coordinate.  Compute that now and pass it to the callback
						if( !TraceTexture( face, 1.0 - B2, B0, B1 ))
							continue;	// passed through alpha-pixel
					}

					if( trace->fraction > isect_t )
					{ 
						trace->contents = face->contents;
						trace->fraction = isect_t;
					}
				}
			} while( --ntris );

			// now, check if all rays have terminated
			if( p2f > trace->fraction ) return;
		}
		
 		if( stack_ptr == &NodeQueue[MAX_NODE_STACK_LEN] )
			return;

		// pop stack!
		CurNode = stack_ptr->node;
		p1f = stack_ptr->p1f;
		p2f = stack_ptr->p2f;
		stack_ptr++;
	}
}
#endif