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
#include "../../engine/studio.h"
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
						if( !TraceTexture( face, 1.0f - B2, B0, B1 ))
							continue;	// passed through alpha-pixel
					}

					if( trace->fraction > isect_t )
					{ 
						trace->contents = face->contents;
						trace->fraction = isect_t;					

						//studio gi
						if( (DotProduct( direction, face->normal ) < 0.0f)||FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ) )
						{
							trace->surface = STUDIO_SURFACE_HIT;
							for( int style = 0; style < MAXLIGHTMAPS; style++ )
							{
								trace->styles[style] = mesh->styles[style];
								VectorClear( trace->light[style] );
								if( mesh->verts[face->a].light )
									VectorMA( trace->light[style], B1, mesh->verts[face->a].light->light[style], trace->light[style] );
								if( mesh->verts[face->b].light )
									VectorMA( trace->light[style], 1.0f - B2, mesh->verts[face->b].light->light[style], trace->light[style] );
								if( mesh->verts[face->c].light )
									VectorMA( trace->light[style], B0, mesh->verts[face->c].light->light[style], trace->light[style] );

								trace->light[style][0] *= TextureToLinear( face->color[0] );
								trace->light[style][1] *= TextureToLinear( face->color[1] );
								trace->light[style][2] *= TextureToLinear( face->color[2] );

								if( FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ) )
									VectorScale( trace->light[style], 0.31831f, trace->light[style] );	//not very accurate								
							}
						}
						else
							trace->surface = -1;
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



float CWorldRayTraceBVH :: SAH( const bbox_t box )
{
	vec3_t	boxdim;
	VectorSubtract( box.max, box.min, boxdim );
	return 2.0f * ((boxdim[0] * boxdim[2]) + (boxdim[0] * boxdim[1]) + (boxdim[1] * boxdim[2]));
}

bool CWorldRayTraceBVH :: TraceTexture( const tface_t *face, float u, float v, float w )
{
	// calculate st from uvw (barycentric) coordinates
	float	s = w * mesh->verts[face->a].st[0] + u * mesh->verts[face->b].st[0] + v * mesh->verts[face->c].st[0];
	float	t = w * mesh->verts[face->a].st[1] + u * mesh->verts[face->b].st[1] + v * mesh->verts[face->c].st[1];

	s *= face->texture->width;
	t *= face->texture->height;

	int x = fix_coord( floorf(s), face->texture->width );
	int y = fix_coord( ceilf(t), face->texture->height );

	return (face->texture->data[(face->texture->width * y) + x] != 255);	
}

int CompareBBoxCentroidsX(const void* a,const void* b) 
{
	bbox_t	*box1 = *(bbox_t**)a;
	bbox_t	*box2 = *(bbox_t**)b;
  	return box1->max[0] + box1->min[0] - box2->max[0] - box2->min[0];
}

int CompareBBoxCentroidsY(const void* a,const void* b) 
{
	bbox_t	*box1 = *(bbox_t**)a;
	bbox_t	*box2 = *(bbox_t**)b;
  	return box1->max[1] + box1->min[1] - box2->max[1] - box2->min[1];
}

int CompareBBoxCentroidsZ(const void* a,const void* b) 
{
	bbox_t	*box1 = *(bbox_t**)a;
	bbox_t	*box2 = *(bbox_t**)b;
  	return box1->max[2] + box1->min[2] - box2->max[2] - box2->min[2];
}

void CWorldRayTraceBVH :: BuildTree( tmesh_t *src )
{
	int	i,j,k;
	int box_count, node_count;

	bbox_t		*boxes;
	bbox_t		**boxes_id[3];
	bbox_t		*boxsah_left, *boxsah_right;
	vec_t		tempSAH;
	vec_t		minSAH;
	int			minSAH_axis;
	int			minSAH_separator;
	bbox_t		minSAH_boxleft, minSAH_boxright;
	bvhnode_t	*node_cur;
	bvhnode_t	*node_last;	
	bbox_t		*box_cur;
	int			*faces_id;
	tface_t 	*face;

	mesh = src;

	box_count = 0;
	for( i = 0; i < mesh->numfaces; i++ )
		if( mesh->faces[i].shadow )
			box_count++;

	node_count = box_count * 2 - 1;

	boxes = new bbox_t[box_count];
	boxsah_left = new bbox_t[box_count];
	boxsah_right = new bbox_t[box_count];
	boxes_id[0] = new bbox_t*[box_count];
	boxes_id[1] = new bbox_t*[box_count];
	boxes_id[2] = new bbox_t*[box_count];
	faces_id = new int[box_count];

	nodes = new bvhnode_t[node_count];
	nodes[0].start_id = 0;
	nodes[0].end_id = box_count - 1;
	nodes[0].hit = BVH_EXIT_NODE;
	nodes[0].miss = BVH_EXIT_NODE;	
	VectorSet( nodes[0].bbox.min,  FLT_MAX,  FLT_MAX,  FLT_MAX );
	VectorSet( nodes[0].bbox.max, -FLT_MAX, -FLT_MAX, -FLT_MAX );	


	for( i = 0, j = 0, box_cur = &boxes[0]; i < box_count; i++, j++, box_cur++ )
	{
		while( true )
		{
			face = &mesh->faces[j];
			if( face->shadow )
				break;
			j++;
		}
		faces_id[i] = j;

		VectorCopy( mesh->verts[face->a].point, box_cur->min );
		VectorCopy( mesh->verts[face->a].point, box_cur->max );
		VectorCompareMin( mesh->verts[face->b].point, box_cur->min, box_cur->min );
		VectorCompareMax( mesh->verts[face->b].point, box_cur->max, box_cur->max );
		VectorCompareMin( mesh->verts[face->c].point, box_cur->min, box_cur->min );
		VectorCompareMax( mesh->verts[face->c].point, box_cur->max, box_cur->max );

		VectorCompareMin( nodes[0].bbox.min, box_cur->min, nodes[0].bbox.min );
		VectorCompareMax( nodes[0].bbox.max, box_cur->max, nodes[0].bbox.max );		

		boxes_id[0][i] = box_cur;
		boxes_id[1][i] = box_cur;
		boxes_id[2][i] = box_cur;
	}

	
	
	node_last = &nodes[0];
	
	for( i = 0, node_cur = &nodes[0]; i < node_count; i++, node_cur++ )
		if( node_cur->end_id > node_cur->start_id )
		{
			minSAH = FLT_MAX;
			
			for( j = 0; j < 3; j++ )
			{
				switch( j )
				{
					case 0: qsort( &boxes_id[0][node_cur->start_id], node_cur->end_id - node_cur->start_id + 1, sizeof(bbox_t*), CompareBBoxCentroidsX ); break;
					case 1: qsort( &boxes_id[1][node_cur->start_id], node_cur->end_id - node_cur->start_id + 1, sizeof(bbox_t*), CompareBBoxCentroidsY ); break;
					case 2: qsort( &boxes_id[2][node_cur->start_id], node_cur->end_id - node_cur->start_id + 1, sizeof(bbox_t*), CompareBBoxCentroidsZ ); break;
				}
				
				
				VectorCopy( boxes_id[j][node_cur->start_id]->min, boxsah_left[node_cur->start_id].min );
				VectorCopy( boxes_id[j][node_cur->start_id]->max, boxsah_left[node_cur->start_id].max );
				for( k = node_cur->start_id + 1; k <= node_cur->end_id; k++ )
				{
					VectorCompareMin( boxsah_left[k-1].min, boxes_id[j][k]->min, boxsah_left[k].min );
					VectorCompareMax( boxsah_left[k-1].max, boxes_id[j][k]->max, boxsah_left[k].max );
				}
				
				VectorCopy( boxes_id[j][node_cur->end_id]->min, boxsah_right[node_cur->end_id].min );
				VectorCopy( boxes_id[j][node_cur->end_id]->max, boxsah_right[node_cur->end_id].max );
				for( k = node_cur->end_id - 1; k >= node_cur->start_id; k-- )
				{
					VectorCompareMin( boxsah_right[k+1].min, boxes_id[j][k]->min, boxsah_right[k].min );
					VectorCompareMax( boxsah_right[k+1].max, boxes_id[j][k]->max, boxsah_right[k].max );
				}

				for( k = node_cur->start_id; k < node_cur->end_id; k++ )
				{
					tempSAH = SAH( boxsah_left[k] ) * ( k - node_cur->start_id + 1 );
					tempSAH += SAH( boxsah_right[k+1] ) * ( node_cur->end_id - k ); 

					if( tempSAH < minSAH )
					{
						minSAH = tempSAH;
						minSAH_axis = j;
						minSAH_separator = k;
						memcpy( &minSAH_boxleft, &boxsah_left[k], sizeof( bbox_t ));
						memcpy( &minSAH_boxright, &boxsah_right[k+1], sizeof( bbox_t ));
					}
				}
			}

			//splitting current node
			node_last++;
			node_cur->hit = node_last - &nodes[0];
			//child1
			node_last->start_id = node_cur->start_id;
			node_last->end_id = minSAH_separator;
			node_last->hit = node_cur->hit + 1;
			node_last->miss = node_cur->hit + 1;
			memcpy( &node_last->bbox, &minSAH_boxleft, sizeof( bbox_t ));
			//child2
			node_last++;
			node_last->start_id = minSAH_separator + 1;
			node_last->end_id = node_cur->end_id;
			node_last->hit = node_cur->miss;
			node_last->miss = node_cur->miss;
			memcpy( &node_last->bbox, &minSAH_boxright, sizeof( bbox_t ));

			//Msg(" bvh node num %d, split pos %d, split axis %d\n", i , minSAH_separator, minSAH_axis );

			//copy boxes_id from the split axis to others
			for( j = 0; j < 3; j++ )
				if( j != minSAH_axis )
					memcpy( &boxes_id[j][node_cur->start_id], &boxes_id[minSAH_axis][node_cur->start_id], (node_cur->end_id - node_cur->start_id + 1) * sizeof(bbox_t*) );			
		}


	for( i = 0, node_cur = &nodes[0]; i < node_count; i++, node_cur++ )
		if( node_cur->end_id == node_cur->start_id )	//leaf node, negative hit points to a face
			node_cur->hit = - faces_id[(boxes_id[0][node_cur->start_id] - &boxes[0])]; 
		

	delete[] boxes;
	delete[] boxsah_left;
	delete[] boxsah_right;
	delete[] boxes_id[0];
	delete[] boxes_id[1];
	delete[] boxes_id[2];
	delete[] faces_id;
}

void CWorldRayTraceBVH :: TraceRay( const vec3_t start, const vec3_t stop, trace_t *trace, bool stop_on_first_intersection )
{
	vec3_t	ray_dir, inv_ray_dir, ray_origin;
	vec_t	dist;

	VectorCopy( start, ray_origin );
	VectorSubtract( stop, start, ray_dir );
	dist = VectorNormalize( ray_dir );
	trace->fraction = dist;

	ray_dir[0] = (ray_dir[0] == 0.0f) ? FLT_EPSILON : ray_dir[0];
	ray_dir[1] = (ray_dir[1] == 0.0f) ? FLT_EPSILON : ray_dir[1];
	ray_dir[2] = (ray_dir[2] == 0.0f) ? FLT_EPSILON : ray_dir[2];
	VectorRecip( ray_dir, inv_ray_dir );


	bvhnode_t	*node_cur;
	vec3_t tbot, ttop, tmin, tmax;
	vec2_t t;
	vec_t t0, t1;
	vec2_t hit;

	vec3_t p0, e0, e1, pv;
	vec_t inv_det;
	vec3_t tv, qv;
	vec4_t uvt;
	bool tri_hit;

	//int counter = 0;
	//int tris_counter = 0;

	int node_cur_id = 0;
	while( node_cur_id != BVH_EXIT_NODE )
	{
		//counter++;
		node_cur = &nodes[node_cur_id];
		
		VectorSubtract( node_cur->bbox.min, ray_origin, tbot );
		VectorMultiply( tbot, inv_ray_dir, tbot );

		VectorSubtract( node_cur->bbox.max, ray_origin, ttop );
		VectorMultiply( ttop, inv_ray_dir, ttop );

		VectorCompareMin( ttop, tbot, tmin );
		VectorCompareMax( ttop, tbot, tmax );

		t[0] = Q_max( tmin[0], tmin[1] );
		t[1] = Q_max( tmin[0], tmin[2] );
		t0 = Q_max( t[0], t[1] );

		t[0] = Q_min( tmax[0], tmax[1] );
		t[1] = Q_min( tmax[0], tmax[2] );
		t1 = Q_min( t[0], t[1] );

		if( ( t1 >= Q_max( t0, 0.0f ) )&&( t0 <= trace->fraction ) )	//box hit
		{
			if( node_cur->hit <= 0 )	//leaf node, check face intersection
			{
				//tris_counter++;

				node_cur_id = node_cur->miss;

				const tface_t *face = &mesh->faces[-node_cur->hit];
				VectorCopy( mesh->verts[face->a].point, p0 );
				VectorSubtract( mesh->verts[face->b].point, p0, e0 );
				VectorSubtract( mesh->verts[face->c].point, p0, e1 );

				CrossProduct( ray_dir, e1, pv );
				inv_det = 1.0f / DotProduct( e0, pv );

				VectorSubtract( ray_origin, p0, tv );
				CrossProduct( tv, e0, qv );

				uvt[0] = DotProduct( tv, pv ) * inv_det;
				uvt[1] = DotProduct( ray_dir, qv ) * inv_det;
				uvt[2] = 1.0f - uvt[0] - uvt[1];				
				uvt[3] = DotProduct( e1, qv ) * inv_det;

				tri_hit = uvt[0] >= 0.0f;
				tri_hit = tri_hit && ( uvt[1] >= 0.0f );
				tri_hit = tri_hit && ( uvt[2] >= 0.0f );
				tri_hit = tri_hit && ( uvt[3] >= 0.0f );

				if( tri_hit )
				{
					if( face->texture->data )
						if( !TraceTexture( face, uvt[0], uvt[1], uvt[2]) )
							continue;

					trace->fraction = uvt[3];
					trace->contents = face->contents;

					if( stop_on_first_intersection )
					{	
						trace->surface = -1;
						trace->fraction /= dist;
						return;
					}

					//studio gi
					if( (DotProduct( ray_dir, face->normal ) < 0.0f )||FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ) )
					{
						trace->surface = STUDIO_SURFACE_HIT;
						for( int style = 0; style < MAXLIGHTMAPS; style++ )
						{
							trace->styles[style] = mesh->styles[style];
							VectorClear( trace->light[style] );
							if( mesh->verts[face->a].light )
								VectorMA( trace->light[style], uvt[2], mesh->verts[face->a].light->light[style], trace->light[style] );
							if( mesh->verts[face->b].light )
								VectorMA( trace->light[style], uvt[0], mesh->verts[face->b].light->light[style], trace->light[style] );
							if( mesh->verts[face->c].light )
								VectorMA( trace->light[style], uvt[1], mesh->verts[face->c].light->light[style], trace->light[style] );

							trace->light[style][0] *= TextureToLinear( face->color[0] );
							trace->light[style][1] *= TextureToLinear( face->color[1] );
							trace->light[style][2] *= TextureToLinear( face->color[2] );

							if( FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ) )
								VectorScale( trace->light[style], 0.31831f, trace->light[style] );	//not very accurate
						}
					}
					else
						trace->surface = -1;
				}
			}
			else
				node_cur_id = node_cur->hit;
		}
		else
			node_cur_id = node_cur->miss;
	}

	trace->fraction /= dist;	
	//Msg( " fraction %f, counter %d, tris_counter %d\n", trace->fraction, counter, tris_counter );
}

#endif
