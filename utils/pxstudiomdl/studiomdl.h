#include <utlarray.h>
#include "mathlib.h"

#ifndef EXTERN
#define EXTERN extern
#endif

#define MAXSTUDIOTRIANGLES		65535	// max triangles per model
#define MAXSRCSTUDIOVERTS		65535	// max source vertices per submodel
#define MAXSTUDIOCMDS		64
#define MAXSTUDIOMOVEKEYS		64
#define MAXSTUDIOIKRULES		64
#define MAXSTUDIOSRCBONES		512	// bones allowed at source movement
#define MAXSTUDIOANIMATIONS		16386	// max frames per sequence
#define MAXSTUDIOANIMFRAMES		16386	// max frames per sequence
#define MAXSTUDIOSEQUENCES		1024	// total animation sequences
#define MAXSTUDIOEVENTS		64	// events per sequence
#define MAXSTUDIOMESHES		256	// max textures per model
#define MAXSTUDIOBLENDS		16	// max anim blends
#define MAXSRCSTUDIONAME		128	// source names can be longer than output
#define MAXWEIGHTLISTS   		128
#define MAXWEIGHTSPERLIST		(MAXSTUDIOBONES)

#define verify_atof( a )		verify_atof_dbg( a, __LINE__ )
#define verify_atoi( a )		verify_atoi_dbg( a, __LINE__ )

EXTERN	char	outname[1024];
EXTERN	bool	cdset;
EXTERN	char	cddir[32][256];
EXTERN	int	numdirs;
EXTERN	int	cdtextureset;
EXTERN	char	cdtexture[16][256];
EXTERN	char	rootname[MAXSTUDIONAME];	// name of the root bone
EXTERN	Vector	g_defaultscale;
EXTERN	Radian	g_defaultrotation;
EXTERN	Vector	g_defaultadjust;
EXTERN	char	defaulttexture[16][256];
EXTERN	char	sourcetexture[16][256];
EXTERN	int	numrep;
EXTERN	int	tag_reversed;
EXTERN	int	tag_normals;
EXTERN	bool	flip_triangles;
EXTERN	float	g_normal_blend;
EXTERN	float	g_alpha_threshold;
EXTERN	bool	g_dump_hboxes;
EXTERN	bool	g_dump_graph;
EXTERN	Vector	eyeposition;
EXTERN	int	gflags;
EXTERN	Vector	bbox[2];
EXTERN	Vector	cbox[2];
EXTERN	bool	g_wrotebbox;
EXTERN	bool	g_wrotecbox;
EXTERN  bool	g_keep_free_bones;
EXTERN	bool	g_collapse_bones;
EXTERN	bool	g_collapse_bones_aggressive;
EXTERN	bool	g_lockbonelengths;
EXTERN	int	maxseqgroupsize;
EXTERN	bool	g_multistagegraph;
EXTERN	int	split_textures;
EXTERN	int	clip_texcoords;
EXTERN	bool	store_uv_coords;
EXTERN	bool	allow_tileing;
EXTERN	bool	allow_boneweights;
EXTERN	bool	has_boneweights;
EXTERN	bool	g_mergebonecontrollers;
EXTERN	bool	g_realignbones;
EXTERN	bool	g_definebones;
EXTERN	bool	g_overridebones;
EXTERN	bool	g_centerstaticprop;
EXTERN	bool	g_staticprop;
EXTERN	float	g_gamma;
extern CUtlArray< char > g_KeyValueText;

typedef struct
{
	int		vertindex;
	int		normindex;	// index into normal array
	int		s, t;
	float		u, v;
} s_trianglevert_t;

typedef struct
{
	int		numbones;
	float		weight[4];
	int		bone[4]; 
} s_boneweight_t;

typedef struct 
{
	Vector		org;		// original position
	s_boneweight_t	globalWeight;
} s_vertex_t;

typedef struct 
{
	int		skinref;
	Vector		org;		// original position
	s_boneweight_t	globalWeight;
} s_normal_t;

typedef struct
{
	int		skinref;
	Vector		vert, norm;
	s_boneweight_t	localWeight;
	s_boneweight_t	globalWeight;
} s_srcvertex_t;

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];// bone name for symbolic links
	int 		parent;		// parent bone
	int		bonecontroller;	// -1 == 0
	int		flags;		// X, Y, Z, XR, YR, ZR
	Vector		pos;		// default pos
	Vector		posscale;		// pos values scale
	Radian		rot;		// default pos
	Vector		rotscale;		// rotation values scale
	int		group;		// hitgroup
	Vector		bmin, bmax;	// bounding box
	matrix3x4		rawLocal;
	matrix3x4		rawLocalOriginal;	// original transform of preDefined bone
	matrix3x4		srcRealign;
	matrix3x4		boneToPose;
	int		proceduralindex;
	Vector4D		qAlignment;
	bool		bPreDefined;
	bool		bPreAligned;
	bool		bDontCollapse;
} s_bonetable_t;

EXTERN s_bonetable_t	g_bonetable[MAXSTUDIOSRCBONES];
EXTERN int		g_real_numbones;
EXTERN int		g_numbones;

typedef struct 
{
	char		from[MAXSRCSTUDIONAME];
	char		to[MAXSRCSTUDIONAME];
} s_renamebone_t;

EXTERN s_renamebone_t	g_renamedbone[MAXSTUDIOSRCBONES];
EXTERN int		g_numrenamedbones;

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	char		parent[MAXSRCSTUDIONAME];
	matrix3x4		rawLocal;
	bool		bPreAligned;
	matrix3x4		srcRealign;
} s_importbone_t;

EXTERN s_importbone_t	g_importbone[MAXSTUDIOSRCBONES];
EXTERN int		g_numimportbones;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];// bone name
	int		bone;
	int		group;		// hitgroup
	int		model;
	Vector		bmin, bmax;	// bounding box
} s_bbox_t;

typedef struct 
{
	char		hitboxsetname[MAXSRCSTUDIONAME];
	int		numhitboxes;
	s_bbox_t		hitbox[MAXSTUDIOSRCBONES];
} s_hitboxset_t;

EXTERN CUtlArray< s_hitboxset_t > g_hitboxsets;

typedef struct
{
	int		models;
	int		group;
	char		name[MAXSRCSTUDIONAME];// bone name
} s_hitgroup_t;

EXTERN s_hitgroup_t		g_hitgroup[MAXSTUDIOSRCBONES];
EXTERN int		g_numhitgroups;

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	int		bone;
	int		type;
	int		index;
	float		start;
	float		end;
} s_bonecontroller_t;

EXTERN s_bonecontroller_t	g_bonecontroller[MAXSTUDIOSRCBONES];
EXTERN int		g_numbonecontrollers;

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	int		flags;
} s_screenalignedbone_t;

EXTERN s_screenalignedbone_t	g_screenalignedbone[MAXSTUDIOSRCBONES];
EXTERN int		g_numscreenalignedbones;

#define IS_ABSOLUTE		0x0001
#define IS_RIGID		0x0002

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	char		bonename[MAXSRCSTUDIONAME];
	int		bone;
	int		type;
	int		flags;
	matrix3x4		local;
	int		found;	// a owning bone has been flagged
} s_attachment_t;

EXTERN s_attachment_t	g_attachment[MAXSTUDIOSRCBONES];
EXTERN int		g_numattachments;

typedef struct
{
	char		bonename[MAXSRCSTUDIONAME];
} s_bonemerge_t;

EXTERN CUtlArray< s_bonemerge_t > g_BoneMerge;
EXTERN CUtlArray< s_bonemerge_t > g_collapse;

typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	int		parent;
} s_node_t;

typedef struct 
{
	Vector		pos;
	Radian		rot;
} s_bone_t;

#define CMDSRC_GLOBAL	0
#define CMDSRC_LOCAL	1

#define CMD_WEIGHTS		1
#define CMD_SUBTRACT	2
#define CMD_AO		3
#define CMD_MATCH		4
#define CMD_FIXUP		5
#define CMD_ANGLE		6
#define CMD_IKFIXUP		7
#define CMD_IKRULE		8
#define CMD_MOTION		9
#define CMD_REFMOTION	10
#define CMD_DERIVATIVE	11
#define CMD_NOANIMATION	12
#define CMD_LINEARDELTA	13
#define CMD_SPLINEDELTA	14
#define CMD_COMPRESS	15
#define CMD_NUMFRAMES	16
#define CMD_COUNTERROTATE	17
#define CMD_SETBONE		18
#define CMD_WORLDSPACEBLEND	19
#define CMD_MATCHBLEND	20
#define CMD_LOCALHIERARCHY	21

typedef struct
{
	int		motiontype;
	int		iStartFrame;	// starting frame to apply motion over
	int		iEndFrame;	// end frame to apply motion over
	int		iSrcFrame;	// frame that matches the "reference" animation
	struct s_animation	*pRefAnim;	// animation to match
	int		iRefFrame;	// reference animation's frame to match
} s_motion_t;

typedef struct
{
	int		endframe;	// frame when pos, rot is valid.
	int		flags;	// type of motion.  Only linear, linear accel, and linear decel is allowed
	float		v0;
	float		v1;
	Vector		vector;	// movement vector
	Vector		pos;	// final position
	Radian		rot;	// final rotation
} s_linearmove_t;

typedef struct
{
	Vector		pos;
	Vector4D		q;
} s_streamdata_t;

typedef struct
{
	// source animations
	int		numerror;
	s_streamdata_t	*pError;
	// compressed animations
	float		scale[6];
	int		numanim[6];
	mstudioanimvalue_t	*anim[6];
} s_animationstream_t;

typedef struct
{
	int		chain;

	int		index;
	int		type;
	int		slot;
	char		bonename[MAXSRCSTUDIONAME];
	char		attachment[MAXSRCSTUDIONAME];
	int		bone;
	Vector		pos;
	Vector4D		q;
	float		height;
	float		floor;
	float		radius;

	int		start;
	int		peak;
	int		tail;
	int		end;

	int		contact;

	bool		usesequence;
	bool		usesource;

	int		flags;

	s_animationstream_t errorData;
} s_ikrule_t;

typedef struct
{
	int		cmd;
	int		cmd_source;
	union 
	{
		struct
		{
			int		index;	
		} weightlist;

		struct
		{
			s_animation	*ref;
			int		frame;
			int		flags;
		} subtract;

		struct
		{
			s_animation	*ref;
			int		motiontype;
			int		srcframe;
			int		destframe;
			char		*pBonename;
		} ao;

		struct
		{
			s_animation	*ref;
			int		srcframe;
			int		destframe;
			int		destpre;
			int		destpost;
		} match;

		struct
		{
			s_animation	*ref;
			int		startframe;
			int		loops;
		} world;

		struct 
		{
			int		start;
			int		end;
		} fixuploop;

		struct 
		{
			float		angle;
		} angle;

		struct
		{
			s_ikrule_t	*pRule;
		} ikfixup;

		struct
		{
			s_ikrule_t	*pRule;
		} ikrule;

		struct
		{
			float		scale;
		} derivative;

		struct
		{
			int		flags;
		} linear;

		struct
		{
			int		frames;
		} compress;

		struct
		{
			int		frames;
		} numframes;

		struct
		{
			char		*pBonename;
			bool		bHasTarget;
			float			targetAngle[3];
		} counterrotate;

		struct
		{
			char		*pBonename;
			char		*pParentname;
			int		start;
			int		peak;
			int		tail;
			int		end;
		} localhierarchy;

		s_motion_t		motion;
	};
} s_animcmd_t;

// keep source data
typedef struct
{
	int		startframe;
	int		endframe;
	int		numframes;
} s_source_t;

typedef struct s_animation
{
	char		name[MAXSRCSTUDIONAME];	// animation name
	char		filename[MAXSRCSTUDIONAME];	// path to .smd file
	bool		isImplied;		// from sequence desc
	int		startframe;
	int		endframe;
	int		numframes;

	s_source_t	source;

	int		flags;

	float		fps;

	int		numbones;
	s_node_t		localBone[MAXSTUDIOSRCBONES];
	int		boneLocalToGlobal[MAXSTUDIOSRCBONES];	// local bone to world bone mapping
	int		boneGlobalToLocal[MAXSTUDIOSRCBONES];	// world bone to local bone mapping

	// default adjustments
	Vector		scale;
	Vector		adjust;
	Radian		rotation; 

	// piecewise linear motion
	s_linearmove_t	piecewisemove[MAXSTUDIOMOVEKEYS];
	int		numpiecewisekeys;
	int		movementindex;

	Vector		linearmovement;

	float		weight[MAXSTUDIOSRCBONES];
	float		posweight[MAXSTUDIOSRCBONES];

	int		numcmds;
	s_animcmd_t	cmds[MAXSTUDIOCMDS];

	int		ikruleindex;
	int		numikrules;
	s_ikrule_t	ikrule[MAXSTUDIOIKRULES];
	bool		noAutoIK;

	int		motiontype;

	int		fudgeloop;
	int		looprestart; // new starting frame for looping animations

	float		motionrollback;

	Vector		bmin;
	Vector		bmax;

	s_bone_t		*rawanim[MAXSTUDIOANIMATIONS];	// [frame][bones];
	s_bone_t		*sanim[MAXSTUDIOANIMATIONS];		// [frame][bones];
	int		numanim[MAXSTUDIOSRCBONES][6];
	mstudioanimvalue_t	*anim[MAXSTUDIOSRCBONES][6];
} s_animation_t;

EXTERN s_animation_t	*g_panimation[MAXSTUDIOSEQUENCES*MAXSTUDIOBLENDS];	// each sequence can have 16 blends
EXTERN int		g_numani, g_real_numani;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	int		chain;
	float		flPosWeight;
	float		flLocalQWeight;
} s_iklock_t;

EXTERN int		g_numikautoplaylocks;
EXTERN s_iklock_t		g_ikautoplaylock[16];

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	s_animcmd_t	cmds[MAXSTUDIOCMDS];
	int		numcmds;
} s_cmdlist_t;

EXTERN s_cmdlist_t		g_cmdlist[MAXSTUDIOANIMATIONS];
EXTERN int		g_numcmdlists;

typedef struct 
{
	int		event;
	int		frame;
	char		options[MAXEVENTSTRING];
} s_event_t;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	int		sequence;
	int		flags;
	int		pose;
	float		start;
	float		peak;
	float		tail;
	float		end;
} s_autolayer_t;

typedef struct 
{
	int		motiontype;
	Vector		linearmovement;

	char		name[MAXSRCSTUDIONAME];
	int		flags;
	float		fps;
	int		numframes;

	int		activity;
	int		actweight;

	int		frameoffset; // used to adjust frame numbers

	int		numevents;
	s_event_t		event[MAXSTUDIOEVENTS];

	int		numblends;
	int		groupsize[2];
	s_animation_t	*panim[MAXSTUDIOBLENDS];
	int		paramindex[2];
	float		paramstart[2];
	float		paramend[2];
	int		paramattachment[2];
	int		paramcontrol[2];
	float		param0[MAXSTUDIOBLENDS];
	float		param1[MAXSTUDIOBLENDS];
	float		weight[MAXSTUDIOSRCBONES];

	s_animation_t	*paramanim;
	s_animation_t	*paramcompanim;
	s_animation_t	*paramcenter;

	int		seqgroup;
	int		animindex;
	int		animdescindex;

	float		fadeintime;
	float		fadeouttime;

	Vector		bmin;
	Vector		bmax;
	int		entrynode;
	int		exitnode;
	int		nodeflags;

	int		numikrules;

	int		cycleposeindex;

	int		numautolayers;
	s_autolayer_t	autolayer[64];

	s_iklock_t	iklock[64];
	int		numiklocks;

	CUtlArray< char >	KeyValue;
} s_sequence_t;

EXTERN s_sequence_t		g_sequence[MAXSTUDIOSEQUENCES];
EXTERN int		g_numseq, g_real_numseq;

typedef struct
{
	char		label[MAXSRCSTUDIONAME];
	char		name[MAXSRCSTUDIONAME];
} s_sequencegroup_t;

EXTERN s_sequencegroup_t	g_sequencegroup[MAXSTUDIOSEQUENCES];
EXTERN int		g_numseqgroups;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	float		min;
	float		max;
	int		flags;
	float		loop;
} s_poseparameter_t;

EXTERN s_poseparameter_t	g_pose[MAXSTUDIOPOSEPARAM];
EXTERN int		g_numposeparameters;

EXTERN int		g_xnode[100][100];
EXTERN char		*g_xnodename[100];
EXTERN int		g_numxnodes;
EXTERN int		g_xnodeskip[10000][2];
EXTERN int		g_numxnodeskips;

// FIXME: what about texture overrides inline with loading models
typedef struct 
{
	char		name[MAXSRCSTUDIONAME];
	int		flags;
	int		srcwidth;
	int		srcheight;
	struct rgbdata_s	*psrc;
	float		max_s;
	float		min_s;
	float		max_t;
	float		min_t;
	int		skintop;
	int		skinleft;
	int		skinwidth;
	int		skinheight;
	float		fskintop;
	float		fskinleft;
	float		fskinwidth;
	float		fskinheight;
	void		*pdata;
	int		size;
	int		parent;
} s_texture_t;

EXTERN s_texture_t		g_texture[MAXSTUDIOSKINS];
EXTERN int		g_numtextures;
EXTERN int		g_numskinref;
EXTERN int		g_numskinfamilies;
EXTERN int		g_skinref[256][MAXSTUDIOSKINS]; // [skin][skinref], returns texture index
EXTERN int		g_numtexturegroups;
EXTERN int		g_numtexturelayers[32];
EXTERN int		g_numtexturereps[32];
EXTERN int		g_texturegroup[32][32][32];

typedef struct 
{
	int		alloctris;
	int		numtris;
	s_trianglevert_t	(*triangle)[3];

	int		skinref;
	int		numnorms;
} s_mesh_t;

typedef struct s_model_s 
{
	char		name[MAXSRCSTUDIONAME];

	int		numbones;
	s_node_t		localBone[MAXSTUDIOSRCBONES];
	matrix3x4		boneToPose[MAXSTUDIOSRCBONES];
	s_bone_t		skeleton[MAXSTUDIOSRCBONES];

	Vector		scale;
	bool		flip_triangles;

	// bone remapping
	int		boneflags[MAXSTUDIOSRCBONES]; // is local bone (or child) referenced with a vertex
	int		boneref[MAXSTUDIOSRCBONES]; // is local bone (or child) referenced with a vertex
	int		boneLocalToGlobal[MAXSTUDIOSRCBONES]; // local bone to world bone mapping
	int		boneGlobalToLocal[MAXSTUDIOSRCBONES]; // world bone to local bone mapping

	Vector		boundingbox[MAXSTUDIOSRCBONES][2];

	CUtlArray<s_vertex_t> vert;
	CUtlArray<s_normal_t> norm;
	CUtlArray<s_srcvertex_t> srcvert;

	int		nummesh;
	s_mesh_t		*pmesh[MAXSTUDIOMESHES];

	float		boundingradius;
} s_model_t;

EXTERN s_model_t		*g_model[MAXSTUDIOMODELS];
EXTERN int		g_nummodels;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	int		nummodels;
	int		base;
	s_model_t		*pmodel[MAXSTUDIOMODELS];
} s_bodypart_t;

EXTERN s_bodypart_t		g_bodypart[MAXSTUDIOBODYPARTS];
EXTERN int		g_numbodyparts;

typedef struct
{
	int		flags;
	char		bonename[MAXSRCSTUDIONAME];
	int		bone;
	char		controlname[MAXSRCSTUDIONAME];
	int		control;
	int		axis;
	Vector		pos[6];
	Vector4D		quat[6];
} s_axisinterpbone_t;

EXTERN s_axisinterpbone_t	g_axisinterpbones[MAXSTUDIOBONES];
EXTERN int		g_axisinterpbonemap[MAXSTUDIOBONES]; // map used axisinterpbone's to source axisinterpbone's
EXTERN int		g_numaxisinterpbones;

typedef struct 
{
	int		flags;
	char		bonename[MAXSRCSTUDIONAME];
	int		bone;
	char		parentname[MAXSRCSTUDIONAME];
	char		controlparentname[MAXSRCSTUDIONAME];
	char		controlname[MAXSRCSTUDIONAME];
	int		control;
	int		numtriggers;
	Vector		size;
	Vector		basepos;
	float		percentage;
	float		tolerance[32];
	Vector4D		trigger[32];
	Vector		pos[32];
	Vector4D		quat[32];
} s_quatinterpbone_t;

EXTERN s_quatinterpbone_t	g_quatinterpbones[MAXSTUDIOBONES];
EXTERN int		g_quatinterpbonemap[MAXSTUDIOBONES]; // map used quatinterpbone's to source axisinterpbone's
EXTERN int		g_numquatinterpbones;

typedef struct
{
	char		bonename[MAXSRCSTUDIONAME];
	int		bone;
	char		parentname[MAXSRCSTUDIONAME];
	int		parent;
	char		aimname[MAXSRCSTUDIONAME];
	int		aimAttach;
	int		aimBone;
	Vector		aimvector;
	Vector		upvector;
	Vector		basepos;
} s_aimatbone_t;

EXTERN s_aimatbone_t	g_aimatbones[MAXSTUDIOBONES];
EXTERN int		g_aimatbonemap[MAXSTUDIOBONES]; // map used aimatpbone's to source aimatpbone's (may be optimized out)
EXTERN int		g_numaimatbones;

typedef struct
{
	// weights, indexed by numbones per weightlist
	char		name[MAXSTUDIONAME];
	int		numbones;
	char		*bonename[MAXWEIGHTSPERLIST];
	float		boneweight[MAXWEIGHTSPERLIST];
	float		boneposweight[MAXWEIGHTSPERLIST];

	// weights, indexed by global bone index
	float		weight[MAXSTUDIOBONES];
	float		posweight[MAXSTUDIOBONES];
} s_weightlist_t;

EXTERN s_weightlist_t	g_weightlist[MAXWEIGHTLISTS];
EXTERN int		g_numweightlist;

typedef struct
{
	int		bone;
	Vector		kneeDir;
} s_iklink_t;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	char		bonename[MAXSRCSTUDIONAME];
	int		axis;
	float		value;
	int		numlinks;
	s_iklink_t	link[10]; // hip, knee, ankle, toes...
	float		height;
	float		radius;
	float		floor;
	Vector		center;
} s_ikchain_t;

EXTERN s_ikchain_t		g_ikchain[16];
EXTERN  int		g_numikchains;

typedef struct
{
	int		flags;
	char		bonename[MAXSRCSTUDIONAME];
	int		bone;
	mstudiojigglebone_t	data;	// the actual jiggle properties
} s_jigglebone_t;

EXTERN s_jigglebone_t	g_jigglebones[MAXSTUDIOBONES];
EXTERN int		g_jigglebonemap[MAXSTUDIOBONES]; // map used jigglebone's to source jigglebonebone's
EXTERN int		g_numjigglebones;

typedef struct
{
	char		parentname[MAXSRCSTUDIONAME];
	char		childname[MAXSRCSTUDIONAME];
	char		subparentname[MAXSRCSTUDIONAME];
} s_forcedhierarchy_t;

EXTERN s_forcedhierarchy_t	g_forcedhierarchy[MAXSTUDIOBONES];
EXTERN int		g_numforcedhierarchy;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	Radian		rot;
} s_forcedrealign_t;

EXTERN s_forcedrealign_t	g_forcedrealign[MAXSTUDIOBONES];
EXTERN int		g_numforcedrealign;

typedef struct
{
	char		name[MAXSRCSTUDIONAME];
	int		numseq;
	char		*sequencename[64];
} s_limitrotation_t;

EXTERN s_limitrotation_t	g_limitrotation[MAXSTUDIOBONES];
EXTERN int		g_numlimitrotation;

//
// studiomdl.cpp
//
int findGlobalBone( const char *name );
int findGlobalBoneXSI( const char *name );
int findLocalBone( const s_animation_t *panim, const char *name );
bool IsGlobalBoneXSI( const char *name, const char *bonename );
int LookupVertex( s_model_t *pmodel, s_srcvertex_t *srcv );
int LookupNormal( s_model_t *pmodel, s_srcvertex_t *srcv );
int LookupAttachment( const char *name );
void clip_rotations( float &rot );
void clip_rotations( Radian &rot );
// methods associated with the key value text block
int KeyValueTextSize( CUtlArray< char > *pKeyValue );
const char *KeyValueText( CUtlArray< char > *pKeyValue );

//
// simplify.cpp
//
void SimplifyModel( void );

//
// skin.cpp
//
void SetSkinValues( void );

extern int BuildTris (s_trianglevert_t (*x)[3], s_mesh_t *y, byte **ppdata );
int LoadBMP (const char* szFile, byte** ppbBits, byte** ppbPalette, int *width, int *height );

//
// write.cpp
//
extern void WriteFile( void );
