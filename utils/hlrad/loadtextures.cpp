#include "qrad.h"
#ifdef HLRAD_TEXTURE

#ifdef WORDS_BIGENDIAN
#error "HLRAD_TEXTURE doesn't support WORDS_BIGENDIAN, because I have no big endian machine to test it"
#endif

int g_numtextures;
radtexture_t *g_textures;

typedef struct waddir_s
{
	struct waddir_s *next;
	char path[_MAX_PATH];
} waddir_t;
waddir_t *g_waddirs = NULL;

void AddWadFolder (const char *path)
{
	waddir_t *waddir;
	waddir = (waddir_t *)malloc (sizeof (waddir_t));
	hlassume (waddir != NULL, assume_NoMemory);
	{
		waddir_t **pos;
		for (pos = &g_waddirs; *pos; pos = &(*pos)->next)
			;
		waddir->next = *pos;
		*pos = waddir;
	}
	safe_snprintf (waddir->path, _MAX_PATH, "%s", path);
}

typedef struct
{
	int filepos;
	int disksize;
	int size;
	char type;
	char compression;
	char pad1, pad2;
	char name[16];
} lumpinfo_t;

typedef struct wadfile_s
{
	struct wadfile_s *next;
	char path[_MAX_PATH];
	FILE *file;
	int filesize;
	int numlumps;
	lumpinfo_t *lumpinfos;
} wadfile_t;

wadfile_t *g_wadfiles = NULL;
bool g_wadfiles_opened;

static int CDECL lump_sorter_by_name (const void *lump1, const void *lump2)
{
	lumpinfo_t *plump1 = (lumpinfo_t *)lump1;
	lumpinfo_t *plump2 = (lumpinfo_t *)lump2;
	return strcasecmp (plump1->name, plump2->name);
}

void OpenWadFile (const char *name
#ifdef ZHLT_NOWADDIR
	, bool fullpath = false
#endif
	)
{
	int i;
	wadfile_t *wad;
	wad = (wadfile_t *)malloc (sizeof (wadfile_t));
	hlassume (wad != NULL, assume_NoMemory);
	{
		wadfile_t **pos;
		for (pos = &g_wadfiles; *pos; pos = &(*pos)->next)
			;
		wad->next = *pos;
		*pos = wad;
	}
#ifdef ZHLT_NOWADDIR
   if (fullpath)
   {
	safe_snprintf (wad->path, _MAX_PATH, "%s", name);
	wad->file = fopen (wad->path, "rb");
	if (!wad->file)
	{
		Error ("Couldn't open %s", wad->path);
	}
   }
   else
   {
#endif
	waddir_t *dir;
	for (dir = g_waddirs; dir; dir = dir->next)
	{
		safe_snprintf (wad->path, _MAX_PATH, "%s\\%s", dir->path, name);
		wad->file = fopen (wad->path, "rb");
		if (wad->file)
		{
			break;
		}
	}
	if (!dir)
	{
		Fatal (assume_COULD_NOT_LOCATE_WAD, "Could not locate wad file %s", name);
		return;
	}
#ifdef ZHLT_NOWADDIR
   }
#endif
	Log ("Using Wadfile: %s\n", wad->path);
	wad->filesize = q_filelength (wad->file);
	struct
	{
		char identification[4];
		int numlumps;
		int infotableofs;
	} wadinfo;
	if (wad->filesize < (int)sizeof (wadinfo))
	{
		Error ("Invalid wad file '%s'.", wad->path);
	}
	SafeRead (wad->file, &wadinfo, sizeof (wadinfo));
	wadinfo.numlumps  = LittleLong(wadinfo.numlumps);
	wadinfo.infotableofs = LittleLong(wadinfo.infotableofs);
	if (strncmp (wadinfo.identification, "WAD2", 4) && strncmp (wadinfo.identification, "WAD3", 4))
		Error ("%s isn't a Wadfile!", wad->path);
	wad->numlumps = wadinfo.numlumps;
	if (wad->numlumps < 0 || wadinfo.infotableofs < 0 || wadinfo.infotableofs + wad->numlumps * (int)sizeof (lumpinfo_t) > wad->filesize)
	{
		Error ("Invalid wad file '%s'.", wad->path);
	}
	wad->lumpinfos = (lumpinfo_t *)malloc (wad->numlumps * sizeof (lumpinfo_t));
	hlassume (wad->lumpinfos != NULL, assume_NoMemory);
    if (fseek (wad->file, wadinfo.infotableofs, SEEK_SET))
		Error ("File read failure: %s", wad->path);
	for (i = 0; i < wad->numlumps; i++)
	{
		SafeRead (wad->file, &wad->lumpinfos[i], sizeof (lumpinfo_t));
		if (!TerminatedString(wad->lumpinfos[i].name, 16))
		{
			wad->lumpinfos[i].name[16 - 1] = 0;
			Warning("Unterminated texture name : wad[%s] texture[%d] name[%s]\n", wad->path, i, wad->lumpinfos[i].name);
		}
		wad->lumpinfos[i].filepos = LittleLong(wad->lumpinfos[i].filepos);
		wad->lumpinfos[i].disksize = LittleLong(wad->lumpinfos[i].disksize);
		wad->lumpinfos[i].size = LittleLong(wad->lumpinfos[i].size);
	}
	qsort (wad->lumpinfos, wad->numlumps, sizeof (lumpinfo_t), lump_sorter_by_name);
}

void TryOpenWadFiles ()
{
	if (!g_wadfiles_opened)
	{
		g_wadfiles_opened = true;
#ifdef ZHLT_NOWADDIR
		char filename[_MAX_PATH];
		safe_snprintf(filename, _MAX_PATH, "%s.wa_", g_Mapname);
	   if (q_exists (filename))
	   {
		OpenWadFile (filename, true);
	   }
	   else
	   {
		Warning ("Couldn't open %s", filename);
#endif
		Log ("Opening wad files from directories:\n");
		if (!g_waddirs)
		{
			Warning ("No wad directories have been set.");
		}
		else
		{
			waddir_t *dir;
			for (dir = g_waddirs; dir; dir = dir->next)
			{
				Log ("  %s\n", dir->path);
			}
		}
		const char *value = ValueForKey (&g_entities[0], "wad");
		char path[MAX_VAL];
		int i, j;
		for (i = 0, j = 0; i < (int)strlen(value) + 1; i++)
		{
			if (value[i] == ';' || value[i] == '\0')
			{
				path[j] = '\0';
				if (path[0])
				{
					char name[MAX_VAL];
					ExtractFile (path, name);
					DefaultExtension (name, ".wad");
					OpenWadFile (name);
				}
				j = 0;
			}
			else
			{
				path[j] = value[i];
				j++;
			}
		}
#ifdef ZHLT_NOWADDIR
	   }
#endif
		CheckFatal ();
	}
}

void TryCloseWadFiles ()
{
	if (g_wadfiles_opened)
	{
		g_wadfiles_opened = false;
		wadfile_t *wadfile, *next;
		for (wadfile = g_wadfiles; wadfile; wadfile = next)
		{
			next = wadfile->next;
			free (wadfile->lumpinfos);
			fclose (wadfile->file);
			free (wadfile);
		}
		g_wadfiles = NULL;
	}
}

void DefaultTexture (radtexture_t *tex, const char *name)
{
	int i;
	tex->width = 16;
	tex->height = 16;
	strcpy (tex->name, name);
	tex->name[16 - 1] = '\0';
	tex->canvas = (byte *)malloc (tex->width * tex->height);
	hlassume (tex->canvas != NULL, assume_NoMemory);
	for (i = 0; i < 256; i++)
	{
		VectorFill (tex->palette[i], 0x80);
	}
	for (i = 0; i < tex->width * tex->height; i++)
	{
		tex->canvas[i] = 0x00;
	}
}

void LoadTexture (radtexture_t *tex, const miptex_t *mt, int size)
{
	int i, j;
	const miptex_t *header = mt;
	const byte *data = (const byte *)mt;
	tex->width = header->width;
	tex->height = header->height;
	strcpy (tex->name, header->name);
	tex->name[16 - 1] = '\0';
	if (tex->width <= 0 || tex->height <= 0 ||
		tex->width % (2 * 1 << (MIPLEVELS - 1)) != 0 || tex->height % (2 * (1 << (MIPLEVELS - 1))) != 0)
	{
		Error ("Texture '%s': dimension (%dx%d) is not multiple of %d.", tex->name, tex->width, tex->height, 2 * (1 << (MIPLEVELS - 1)));
	}
	int mipsize;
	for (mipsize = 0, i = 0; i < MIPLEVELS; i++)
	{
		if ((int)mt->offsets[i] != (int)sizeof (miptex_t) + mipsize)
		{
			Error ("Texture '%s': unexpected miptex offset.", tex->name);
		}
		mipsize += (tex->width >> i) * (tex->height >> i);
	}
	if (size < (int)sizeof (miptex_t) + mipsize + 2 + 256 * 3)
	{
		Error ("Texture '%s': no enough data.", tex->name);
	}
	if (*(unsigned short *)&data[sizeof (miptex_t) + mipsize] != 256)
	{
		Error ("Texture '%s': palette size is not 256.", tex->name);
	}
	tex->canvas = (byte *)malloc (tex->width * tex->height);
	hlassume (tex->canvas != NULL, assume_NoMemory);
	for (i = 0; i < tex->height; i++)
	{
		for (j = 0; j < tex->width; j++)
		{
			tex->canvas[i * tex->width + j] = data[sizeof (miptex_t) + i * tex->width + j];
		}
	}
	for (i = 0; i < 256; i++)
	{
		for (j = 0; j < 3; j++)
		{
			tex->palette[i][j] = data[sizeof (miptex_t) + mipsize + 2 + i * 3 + j];
		}
	}
}

void LoadTextureFromWad (radtexture_t *tex, const miptex_t *header)
{
	tex->width = header->width;
	tex->height = header->height;
	strcpy (tex->name, header->name);
	tex->name[16 - 1] = '\0';
	wadfile_t *wad;
	for (wad = g_wadfiles; wad; wad = wad->next)
	{
		lumpinfo_t temp, *found;
		strcpy (temp.name, tex->name);
		found = (lumpinfo_t *)bsearch (&temp, wad->lumpinfos, wad->numlumps, sizeof (lumpinfo_t), lump_sorter_by_name);
		if (found)
		{
			Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': found in '%s'.\n", tex->name, wad->path);
			if (found->type != 67 || found->compression != 0)
				continue;
			if (found->disksize < (int)sizeof (miptex_t) || found->filepos < 0 || found->filepos + found->disksize > wad->filesize)
			{
				Warning ("Texture '%s': invalid texture data in '%s'.", tex->name, wad->path);
				continue;
			}
			miptex_t *mt = (miptex_t *)malloc (found->disksize);
			hlassume (mt != NULL, assume_NoMemory);
			if (fseek (wad->file, found->filepos, SEEK_SET))
				Error ("File read failure");
			SafeRead (wad->file, mt, found->disksize);
			if (!TerminatedString(mt->name, 16))
			{
				Warning("Texture '%s': invalid texture data in '%s'.", tex->name, wad->path);
				free (mt);
				continue;
			}
			Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': name '%s', width %d, height %d.\n", tex->name, mt->name, mt->width, mt->height);
			if (strcasecmp (mt->name, tex->name))
			{
				Warning("Texture '%s': texture name '%s' differs from its reference name '%s' in '%s'.", tex->name, mt->name, tex->name, wad->path);
			}
			LoadTexture (tex, mt, found->disksize);
			free (mt);
			break;
		}
	}
	if (!wad)
	{
		Warning ("Texture '%s': texture is not found in wad files.", tex->name);
		DefaultTexture (tex, tex->name);
		return;
	}
}

void LoadTextures ()
{
	if (!g_notextures)
	{
		Log ("Load Textures:\n");
	}
	g_numtextures = g_texdatasize? ((dmiptexlump_t *)g_dtexdata)->nummiptex: 0;
	g_textures = (radtexture_t *)malloc (g_numtextures * sizeof (radtexture_t));
	hlassume (g_textures != NULL, assume_NoMemory);
	int i;
	for (i = 0; i < g_numtextures; i++)
	{
		int offset = ((dmiptexlump_t *)g_dtexdata)->dataofs[i];
		int size = g_texdatasize - offset;
		radtexture_t *tex = &g_textures[i];
		if (g_notextures)
		{
			DefaultTexture (tex, "DEFAULT");
		}
		else if (offset < 0 || size < (int)sizeof (miptex_t))
		{
			Warning ("Invalid texture data in '%s'.", g_source);
			DefaultTexture (tex, "");
		}
		else
		{
			miptex_t *mt = (miptex_t *)&g_dtexdata[offset];
			if (mt->offsets[0])
			{
				Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': found in '%s'.\n", mt->name, g_source);
				Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': name '%s', width %d, height %d.\n", mt->name, mt->name, mt->width, mt->height);
				LoadTexture (tex, mt, size);
			}
			else
			{
				TryOpenWadFiles ();
				LoadTextureFromWad (tex, mt);
			}
		}
#ifdef HLRAD_REFLECTIVITY
		{
			vec3_t total;
			VectorClear (total);
			for (int j = 0; j < tex->width * tex->height; j++)
			{
				vec3_t reflectivity;
				if (tex->name[0] == '{' && tex->canvas[j] == 0xFF)
				{
					VectorFill (reflectivity, 0.0);
				}
				else
				{
					VectorScale (tex->palette[tex->canvas[j]], 1.0/255.0, reflectivity);
					for (int k = 0; k < 3; k++)
					{
						reflectivity[k] = pow (reflectivity[k], g_texreflectgamma);
					}
					VectorScale (reflectivity, g_texreflectscale, reflectivity);
				}
				VectorAdd (total, reflectivity, total);
			}
			VectorScale (total, 1.0 / (double)(tex->width * tex->height), total);
			VectorCopy (total, tex->reflectivity);
			Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': reflectivity is (%f,%f,%f).\n",
				tex->name, tex->reflectivity[0], tex->reflectivity[1], tex->reflectivity[2]);
			if (VectorMaximum (tex->reflectivity) > 1.0 + NORMAL_EPSILON)
			{
				Warning ("Texture '%s': reflectivity (%f,%f,%f) greater than 1.0.", tex->name, tex->reflectivity[0], tex->reflectivity[1], tex->reflectivity[2]);
			}
		}
#endif
	}
	if (!g_notextures)
	{
		Log ("%i textures referenced\n", g_numtextures);
		TryCloseWadFiles ();
	}
}

#ifdef ZHLT_EMBEDLIGHTMAP

// color quantization algorithm

#define CQ_DIM 3

template<class T, class T2, class T3> inline void CQ_VectorSubtract (const T a[CQ_DIM], const T2 b[CQ_DIM], T3 c[CQ_DIM])
{
	for (int x = 0; x < CQ_DIM; x++)
	{
		c[x] = a[x] - b[x];
	}
}

template<class T, class T2, class T3> inline void CQ_VectorAdd (const T a[CQ_DIM], const T2 b[CQ_DIM], T3 c[CQ_DIM])
{
	for (int x = 0; x < CQ_DIM; x++)
	{
		c[x] = a[x] + b[x];
	}
}

template<class T, class T2> inline void CQ_VectorScale (const T a[CQ_DIM], const T2 b, T c[CQ_DIM])
{
	for (int x = 0; x < CQ_DIM; x++)
	{
		c[x] = a[x] * b;
	}
}

template<class T, class T2> inline void CQ_VectorCopy (const T a[CQ_DIM], T2 b[CQ_DIM])
{
	for (int x = 0; x < CQ_DIM; x++)
	{
		b[x] = a[x];
	}
}

template<class T> inline void CQ_VectorClear (T a[CQ_DIM])
{
	for (int x = 0; x < CQ_DIM; x++)
	{
		a[x] = (T)0;
	}
}

template<class T> inline T CQ_DotProduct (const T a[CQ_DIM], const T b[CQ_DIM])
{
	T dot = (T)0;
	for (int x = 0; x < CQ_DIM; x++)
	{
		dot += a[x] * b[x];
	}
	return dot;
}

typedef struct
{
	int axis;
	int dist;
	double numpoints[2];
}
cq_splitter_t; // partition the space into { point: point[axis] < dist } and { point: point[axis] >= dist }

typedef struct cq_node_s
{
	bool isleafnode;
	cq_node_s *parentnode;
	cq_node_s *childrennode[2];

	int numpoints; // numpoints > 0
	unsigned char (*refpoints)[CQ_DIM];
	double centerofpoints[CQ_DIM];

	bool needsplit;
	cq_splitter_t bestsplitter;
	double splitpriority;
}
cq_node_t; // a cuboid region; the root node is the entire cube whose size is 255

typedef struct cq_searchnode_s
{
	bool isleafnode;
	cq_searchnode_s *childrennode[2];

	int planeaxis;
	int planedist;

	int result;
}
cq_searchnode_t;

static void CQ_SelectPartition (cq_node_t *node)
{
	CQ_VectorClear (node->centerofpoints);
	for (int i = 0; i < node->numpoints; i++)
	{
		CQ_VectorAdd (node->centerofpoints, node->refpoints[i], node->centerofpoints);
	}
	CQ_VectorScale (node->centerofpoints, 1 / (double)node->numpoints, node->centerofpoints);

	node->needsplit = false;
	for (int k = 0; k < CQ_DIM; k++)
	{
		double count;
		double counts[256];
		double sum[CQ_DIM];
		double sums[256][CQ_DIM];

		double bucketsums[256][CQ_DIM];
		int bucketsizes[256];

		const unsigned char (*nodepoints)[CQ_DIM] = node->refpoints;
		const int nodenumpoints = node->numpoints;

		memset (bucketsums, 0, 256 * sizeof (double [CQ_DIM]));
		memset (bucketsizes, 0, 256 * sizeof (int));
		for (int i = 0; i < nodenumpoints; i++)
		{
			int j = nodepoints[i][k];
			bucketsizes[j]++;
			CQ_VectorAdd (bucketsums[j], nodepoints[i], bucketsums[j]);
		}
		
		int min = 256;
		int max = -1;
		count = 0;
		CQ_VectorClear (sum);
		int j;
		for (j = 0; j < 256; j++)
		{
			counts[j] = count;
			CQ_VectorCopy (sum, sums[j]);
			count += bucketsizes[j];
			CQ_VectorAdd (sum, bucketsums[j], sum);
			if (bucketsizes[j] > 0)
			{
				if (j < min)
				{
					min = j;
				}
				if (j > max)
				{
					max = j;
				}
			}
		}
		if (max < min)
		{
			Error ("CQ_SelectPartition: internal error");
		}
		// sweep along the axis and find the plane that maximize square error reduction
		for (j = min + 1; j < max + 1; j++)
		{
			double priority = 0; // the decrease in total square deviation
			priority -= CQ_DotProduct (sum, sum) / count;
			priority += CQ_DotProduct (sums[j], sums[j]) / counts[j];
			double remain[CQ_DIM];
			CQ_VectorSubtract (sum, sums[j], remain); // sums and counts are precise (have no round-off error)
			priority += CQ_DotProduct (remain, remain) / (count - counts[j]);
			if (node->needsplit == false ||
				priority > node->splitpriority + 0.1 ||
				priority >= node->splitpriority - 0.1
				&& fabs (counts[j] - count / 2) < fabs (node->bestsplitter.numpoints[0] - count / 2))
			{
				node->needsplit = true;
				node->splitpriority = priority;
				node->bestsplitter.axis = k;
				node->bestsplitter.dist = j;
				node->bestsplitter.numpoints[0] = counts[j];
				node->bestsplitter.numpoints[1] = count - counts[j];
			}
		}
	}
}

static cq_searchnode_t *CQ_AllocSearchTree (int maxcolors)
{
	cq_searchnode_t *searchtree;
	searchtree = (cq_searchnode_t *)malloc ((2 * maxcolors - 1) * sizeof (cq_searchnode_t));
	hlassume (searchtree != NULL, assume_NoMemory);
	return searchtree;
}

static void CQ_FreeSearchTree (cq_searchnode_t *searchtree)
{
	free (searchtree);
}

static void CQ_CreatePalette (int numpoints, const unsigned char (*points)[CQ_DIM],
					   int maxcolors, unsigned char (*colors_out)[CQ_DIM], int &numcolors_out,
					   cq_searchnode_t *searchtree_out) //[2 * maxcolors - 1]
{
	if (numpoints <= 0 || maxcolors <= 0)
	{
		numcolors_out = 0;
		return;
	}

	unsigned char (*pointarray)[CQ_DIM];
	pointarray = (unsigned char (*)[CQ_DIM])malloc (numpoints * sizeof (unsigned char [CQ_DIM]));
	hlassume (pointarray != NULL, assume_NoMemory);
	memcpy (pointarray, points, numpoints * sizeof (unsigned char [CQ_DIM]));

	cq_node_t *n;
	cq_searchnode_t *s;
	int numnodes = 0;
	int maxnodes = 2 * maxcolors - 1;
	cq_node_t *nodes = (cq_node_t *)malloc (maxnodes * sizeof (cq_node_t));
	hlassume (nodes != NULL, assume_NoMemory);

	n = &nodes[0];
	numnodes++;

	n->isleafnode = true;
	n->parentnode = NULL;
	n->numpoints = numpoints;
	n->refpoints = pointarray;
	CQ_SelectPartition (n);
	int i;

	for (i = 1; i < maxcolors; i++)
	{
		bool needsplit;
		double bestpriority;
		cq_node_t *bestnode;

		needsplit = false;
		for (int j = 0; j < numnodes; j++)
		{
			n = &nodes[j];
			if (!n->isleafnode || !n->needsplit)
			{
				continue;
			}
			if (needsplit == false || n->splitpriority > bestpriority + 0.1)
			{
				needsplit = true;
				bestpriority = n->splitpriority;
				bestnode = n;
			}
		}
		if (!needsplit)
		{
			break;
		}

		bestnode->isleafnode = false;
		for (int k = 0; k < 2; k++)
		{
			n = &nodes[numnodes];
			numnodes++;
			if (numnodes > maxnodes)
			{
				Error ("CQ_CreatePalette: internal error");
			}

			bestnode->childrennode[k] = n;
			n->isleafnode = true;
			n->parentnode = bestnode;
			n->numpoints = 0;
			n->refpoints = NULL;
		}

		// partition the points using the best splitter
		{
			const int splitaxis = bestnode->bestsplitter.axis;
			const int splitdist = bestnode->bestsplitter.dist;
			const int numpoints = bestnode->numpoints;
			unsigned char (*points)[CQ_DIM] = bestnode->refpoints;

			unsigned char (*left)[CQ_DIM];
			unsigned char (*right)[CQ_DIM];
			left = &bestnode->refpoints[0];
			right = &bestnode->refpoints[bestnode->numpoints - 1];
			while (1)
			{
				while ((*left)[splitaxis] < splitdist)
				{
					left++;
				}
				while ((*right)[splitaxis] >= splitdist)
				{
					right--;
				}
				if (left >= right)
				{
					break;
				}
				unsigned char tmp[CQ_DIM];
				CQ_VectorCopy (*left, tmp);
				CQ_VectorCopy (*right, *left);
				CQ_VectorCopy (tmp, *right);
			}
			if (right != left - 1)
			{
				Error ("CQ_CreatePalette: internal error");
			}

			bestnode->childrennode[0]->numpoints = left - bestnode->refpoints;
			bestnode->childrennode[0]->refpoints = bestnode->refpoints;
			bestnode->childrennode[1]->numpoints = &bestnode->refpoints[bestnode->numpoints] - left;
			bestnode->childrennode[1]->refpoints = left;
			if (bestnode->childrennode[0]->numpoints <= 0 ||
				bestnode->childrennode[0]->numpoints != bestnode->bestsplitter.numpoints[0])
			{
				Error ("CQ_CreatePalette: internal error");
			}
			if (bestnode->childrennode[1]->numpoints <= 0 ||
				bestnode->childrennode[1]->numpoints != bestnode->bestsplitter.numpoints[1])
			{
				Error ("CQ_CreatePalette: internal error");
			}
		}

		CQ_SelectPartition (bestnode->childrennode[0]);
		CQ_SelectPartition (bestnode->childrennode[1]);
	}

	for (i = 0; i < numnodes; i++)
	{
		n = &nodes[i];
		s = &searchtree_out[i];
		s->isleafnode = n->isleafnode;
		if (!n->isleafnode)
		{
			s->planeaxis = n->bestsplitter.axis;
			s->planedist = n->bestsplitter.dist;
			s->childrennode[0] = &searchtree_out[n->childrennode[0] - nodes];
			s->childrennode[1] = &searchtree_out[n->childrennode[1] - nodes];
		}
	}

	numcolors_out = 0;
	n = &nodes[0];
	while (1)
	{
		while (!n->isleafnode)
		{
			n = n->childrennode[0];
		}
		s = &searchtree_out[n - nodes];
		s->result = numcolors_out;
		for (int k = 0; k < CQ_DIM; k++)
		{
			int val = (int)floor (n->centerofpoints[k] + 0.5 + 0.00001);
			val = qmax (0, qmin (val, 255));
			colors_out[numcolors_out][k] = val;
		}
		numcolors_out++;
		while (n->parentnode)
		{
			if (n == n->parentnode->childrennode[0])
			{
				break;
			}
			n = n->parentnode;
		}
		if (!n->parentnode)
		{
			break;
		}
		n = n->parentnode->childrennode[1];
	}

	if (2 * numcolors_out - 1 != numnodes)
	{
		Error ("CQ_CreatePalette: internal error");
	}

	free (pointarray);
	free (nodes);
}

static void CQ_MapPoint_r (int *bestdist, int *best,
					cq_searchnode_t *node, const unsigned char (*colors)[CQ_DIM],
					const unsigned char point[CQ_DIM], int searchradius)
{
	while (!node->isleafnode)
	{
		int dist = point[node->planeaxis] - node->planedist;
		if (dist <= -searchradius)
		{
			node = node->childrennode[0];
		}
		else if (dist >= searchradius - 1)
		{
			node = node->childrennode[1];
		}
		else
		{
			CQ_MapPoint_r (bestdist, best, node->childrennode[0], colors, point, searchradius);
			CQ_MapPoint_r (bestdist, best, node->childrennode[1], colors, point, searchradius);
			return;
		}
	}
	int dist = 0;
	for (int k = 0; k < CQ_DIM; k++)
	{
		dist += (colors[node->result][k] - point[k]) * (colors[node->result][k] - point[k]);
	}
	if (dist <= *bestdist)
	{
		if (dist < *bestdist || node->result < *best)
		{
			*bestdist = dist;
			*best = node->result;
		}
	}
}

static int CQ_MapPoint (const unsigned char point[CQ_DIM], const unsigned char (*colors)[CQ_DIM], int numcolors, cq_searchnode_t *searchtree)
{
	if (numcolors <= 0)
	{
		Error ("CQ_MapPoint: internal error");
	}

	cq_searchnode_t *node;
	int bestdist;
	int best;
	int searchradius;

	for (node = searchtree; !node->isleafnode; )
	{
		node = node->childrennode[point[node->planeaxis] >= node->planedist];
	}
	best = node->result;
	bestdist = 0;
	for (int k = 0; k < CQ_DIM; k++)
	{
		bestdist += (colors[best][k] - point[k]) * (colors[best][k] - point[k]);
	}

	searchradius = (int)ceil(sqrt ((double)bestdist) + 0.1);
	CQ_MapPoint_r (&bestdist, &best, searchtree, colors, point, searchradius);
	return best;
}

// =====================================================================================
//  EmbedLightmapInTextures
//      check for "zhlt_embedlightmap" and update g_dfaces, g_texinfo, g_dtexdata and g_dlightdata
// =====================================================================================

#define RADTEXTURES_MAX 2048 // should be smaller than 62 * 62 and smaller than MAX_MAP_TEXTURES
static int g_newtextures_num = 0;
static byte *g_newtextures_data[RADTEXTURES_MAX];
static int g_newtextures_size[RADTEXTURES_MAX];

int NewTextures_GetCurrentMiptexIndex ()
{
	dmiptexlump_t *texdata = (dmiptexlump_t *)g_dtexdata;
	return texdata->nummiptex + g_newtextures_num;
}

void NewTextures_PushTexture (int size, void *data)
{
	if (g_newtextures_num >= RADTEXTURES_MAX)
	{
		Error ("the number of textures created by hlrad has exceeded its internal limit(%d).", (int)RADTEXTURES_MAX);
	}
	g_newtextures_data[g_newtextures_num] = (byte *)malloc (size);
	hlassume (g_newtextures_data[g_newtextures_num] != NULL, assume_NoMemory);
	memcpy (g_newtextures_data[g_newtextures_num], data, size);
	g_newtextures_size[g_newtextures_num] = size;
	g_newtextures_num++;
}

void NewTextures_Write ()
{
	if (!g_newtextures_num)
	{
		return;
	}

	int i;
	dmiptexlump_t *texdata = (dmiptexlump_t *)g_dtexdata;

	byte *dataaddr = (byte *)&texdata->dataofs[texdata->nummiptex];
	int datasize = (g_dtexdata + g_texdatasize) - dataaddr;
	byte *newdataaddr = (byte *)&texdata->dataofs[texdata->nummiptex + g_newtextures_num];
	hlassume (g_texdatasize + (newdataaddr - dataaddr) <= g_max_map_miptex, assume_MAX_MAP_MIPTEX);
	memmove (newdataaddr, dataaddr, datasize);
	g_texdatasize += newdataaddr - dataaddr;
	for (i = 0; i < texdata->nummiptex; i++)
	{
		if (texdata->dataofs[i] < 0) // bad texture
		{
			continue;
		}
		texdata->dataofs[i] += newdataaddr - dataaddr;
	}

	hlassume (texdata->nummiptex + g_newtextures_num < MAX_MAP_TEXTURES, assume_MAX_MAP_TEXTURES);
	for (i = 0; i < g_newtextures_num; i++)
	{
		hlassume (g_texdatasize + g_newtextures_size[i] <= g_max_map_miptex, assume_MAX_MAP_MIPTEX);
		memcpy (g_dtexdata + g_texdatasize, g_newtextures_data[i], g_newtextures_size[i]);
		texdata->dataofs[texdata->nummiptex + i] = g_texdatasize;
		g_texdatasize += g_newtextures_size[i];
	}
	texdata->nummiptex += g_newtextures_num;

	for (i = 0; i < g_newtextures_num; i++)
	{
		free (g_newtextures_data[i]);
	}
	g_newtextures_num = 0;
}

static unsigned int Hash (int size, void *data)
{
	unsigned int hash = 0;
	for (int i = 0; i < size; i++)
	{
		hash = 31 * hash + ((unsigned char *)data)[i];
	}
	return hash;
}

static void GetLightInt (dface_t *face, const int texsize[2], int ix, int iy, vec3_t &light)
{
	ix = qmax (0, qmin (ix, texsize[0]));
	iy = qmax (0, qmin (iy, texsize[1]));
	VectorClear (light);
	if (face->lightofs < 0)
	{
		return;
	}
	for (int k = 0; k < MAXLIGHTMAPS && face->styles[k] != 255; k++)
	{
		byte *samples = &g_dlightdata[face->lightofs + k * (texsize[0] + 1) * (texsize[1] + 1) * 3];
		if (face->styles[k] == 0)
		{
			VectorAdd (light, &samples[(iy * (texsize[0] + 1) + ix) * 3], light);
		}
	}
}

static void GetLight (dface_t *face, const int texsize[2], double x, double y, vec3_t &light)
{
	int ix, iy;
	double dx, dy;
	ix = (int)floor (x);
	iy = (int)floor (y);
	dx = x - ix;
	dx = qmax (0, qmin (dx, 1));
	dy = y - iy;
	dy = qmax (0, qmin (dy, 1));
	
	// do bilinear interpolation
	vec3_t light00, light10, light01, light11;
	GetLightInt (face, texsize, ix, iy, light00);
	GetLightInt (face, texsize, ix + 1, iy, light10);
	GetLightInt (face, texsize, ix, iy + 1, light01);
	GetLightInt (face, texsize, ix + 1, iy + 1, light11);
	vec3_t light0, light1;
	VectorScale (light00, 1 - dy, light0);
	VectorMA (light0, dy, light01, light0);
	VectorScale (light10, 1 - dy, light1);
	VectorMA (light1, dy, light11, light1);
	VectorScale (light0, 1 - dx, light);
	VectorMA (light, dx, light1, light);
}

static bool GetValidTextureName (int miptex, char name[16])
{
	int numtextures = g_texdatasize? ((dmiptexlump_t *)g_dtexdata)->nummiptex: 0;
	int offset;
	int size;
	miptex_t *mt;
	
	if (miptex < 0 || miptex >= numtextures)
	{
		return false;
	}
	offset = ((dmiptexlump_t *)g_dtexdata)->dataofs[miptex];
	size = g_texdatasize - offset;
	if (offset < 0 || g_dtexdata + offset < (byte *)&((dmiptexlump_t *)g_dtexdata)->dataofs[numtextures] ||
		size < (int)sizeof (miptex_t))
	{
		return false;
	}

	mt = (miptex_t *)&g_dtexdata[offset];
	safe_strncpy (name, mt->name, 16);

	if (strcmp (name, mt->name))
	{
		return false;
	}
	
	if (strlen (name) >= 5 && !strncasecmp (&name[1], "_rad", 4))
	{
		return false;
	}

	return true;
}

void EmbedLightmapInTextures ()
{
	if (!g_lightdatasize)
	{
		// hlrad hasn't run
		return;
	}
	if (!g_texdatasize)
	{
		// texdata hasn't been initialized
		return;
	}
	if (g_notextures)
	{
		// hlrad didn't load the wad files
		return;
	}

	int i, j, k;
	int miplevel;
	int count = 0;
	int count_bytes = 0;
	bool logged = false;

	for (i = 0; i < g_numfaces; i++)
	{
		dface_t *f = &g_dfaces[i];
		
		if (f->lightofs == -1) // some faces don't have lightmap
		{
			continue;
		}
		if (f->texinfo < 0 || f->texinfo >= g_numtexinfo)
		{
			continue;
		}
		
		entity_t *ent = g_face_entity[i];
		int originaltexinfonum = f->texinfo;
		texinfo_t *originaltexinfo = &g_texinfo[originaltexinfonum];
		char texname[16];
		if (!GetValidTextureName (originaltexinfo->miptex, texname))
		{
			continue;
		}
		radtexture_t *tex = &g_textures[originaltexinfo->miptex];

		if (ent == &g_entities[0]) // world
		{
			continue;
		}
		if (!strncmp (texname, "sky", 3)
			|| originaltexinfo->flags & TEX_SPECIAL) // skip special surfaces
		{
			continue;
		}
		if (!IntForKey (ent, "zhlt_embedlightmap"))
		{
			continue;
		}

		if (!logged)
		{
			Log ("\n");
			Log ("Embed Lightmap : ");
			Developer (DEVELOPER_LEVEL_MESSAGE, "\n");
			logged = true;
		}

		bool poweroftwo = DEFAULT_EMBEDLIGHTMAP_POWEROFTWO;
		vec_t denominator = DEFAULT_EMBEDLIGHTMAP_DENOMINATOR;
		vec_t gamma = DEFAULT_EMBEDLIGHTMAP_GAMMA;
		int resolution = DEFAULT_EMBEDLIGHTMAP_RESOLUTION;
		if (IntForKey (ent, "zhlt_embedlightmapresolution"))
		{
			resolution = IntForKey (ent, "zhlt_embedlightmapresolution");
			if (resolution <= 0 || resolution > TEXTURE_STEP || ((resolution - 1) & resolution) != 0)
			{
				Error ("resolution cannot be %d; valid values are 1, 2, 4 ... %d.", resolution, (int)TEXTURE_STEP);
			}
		}

		// calculate texture size and allocate memory for all miplevels

		int texturesize[2];
		float (*texture)[5]; // red, green, blue and alpha channel; the last one is number of samples
		byte (*texturemips[MIPLEVELS])[4]; // red, green, blue and alpha channel
		int s, t;
		int texmins[2];
		int texmaxs[2];
		int texsize[2]; // texturesize = (texsize + 1) * TEXTURE_STEP
		int side[2];

		GetFaceExtents (i, texmins, texmaxs);
		texsize[0] = texmaxs[0] - texmins[0];
		texsize[1] = texmaxs[1] - texmins[1];
		if (texsize[0] < 0 || texsize[1] < 0 || texsize[0] > MAX_SURFACE_EXTENT || texsize[1] > MAX_SURFACE_EXTENT)
		{
			Warning ("skipped a face with bad surface extents @ (%4.3f %4.3f %4.3f)", g_face_centroids[i][0], g_face_centroids[i][1], g_face_centroids[i][2]);
			continue;
		}

		for (k = 0; k < 2; k++)
		{
			texturesize[k] = (texsize[k] + 1) * TEXTURE_STEP;
			if (texturesize[k] < texsize[k] * TEXTURE_STEP + resolution * 4)
			{
				texturesize[k] = texsize[k] * TEXTURE_STEP + resolution * 4; // prevent edge bleeding
			}
			texturesize[k] = (texturesize[k] + resolution - 1) / resolution;
			texturesize[k] += 15 - (texturesize[k] + 15) % 16; // must be multiples of 16
			if (poweroftwo)
			{
				for (j = 0; j <= 30; j++)
				{
					if ((1 << j) >= texturesize[k])
					{
						texturesize[k] = (1 << j);
						break;
					}
				}
			}
			side[k] = (texturesize[k] * resolution - texsize[k] * TEXTURE_STEP) / 2;
		}
		texture = (float (*)[5])malloc (texturesize[0] * texturesize[1] * sizeof (float [5]));
		hlassume (texture != NULL, assume_NoMemory);
		for (miplevel = 0; miplevel < MIPLEVELS; miplevel++)
		{
			texturemips[miplevel] = (byte (*)[4])malloc ((texturesize[0] >> miplevel) * (texturesize[1] >> miplevel) * sizeof (byte [4]));
			hlassume (texturemips[miplevel] != NULL, assume_NoMemory);
		}

		// calculate the texture

		for (t = 0; t < texturesize[1]; t++)
		{
			for (s = 0; s < texturesize[0]; s++)
			{
				float (*dest)[5] = &texture[t * texturesize[0] + s];
				VectorFill (*dest, 0);
				(*dest)[3] = 0;
				(*dest)[4] = 0;
			}
		}
		for (t = -side[1]; t < texsize[1] * TEXTURE_STEP + side[1]; t++)
		{
			for (s = -side[0]; s < texsize[0] * TEXTURE_STEP + side[0]; s++)
			{
				double s_vec, t_vec;
				double src_s, src_t;
				int src_is, src_it;
				byte src_index;
				byte src_color[3];
				double dest_s, dest_t;
				int dest_is, dest_it;
				float (*dest)[5];
				double light_s, light_t;
				vec3_t light;

				s_vec = s + texmins[0] * TEXTURE_STEP + 0.5;
				t_vec = t + texmins[1] * TEXTURE_STEP + 0.5;

				if (resolution == 1)
				{
					dest_s = s_vec;
					dest_t = t_vec;
				}
				else // the final blurred texture is shifted by a half pixel so that lightmap samples align with the center of pixels
				{
					dest_s = s_vec / resolution + 0.5;
					dest_t = t_vec / resolution + 0.5;
				}
				dest_s = dest_s - texturesize[0] * floor (dest_s / texturesize[0]);
				dest_t = dest_t - texturesize[1] * floor (dest_t / texturesize[1]);
				dest_is = (int)floor (dest_s); // dest_is = dest_s % texturesize[0]
				dest_it = (int)floor (dest_t); // dest_it = dest_t % texturesize[1]
				dest_is = qmax (0, qmin (dest_is, texturesize[0] - 1));
				dest_it = qmax (0, qmin (dest_it, texturesize[1] - 1));
				dest = &texture[dest_it * texturesize[0] + dest_is];

				src_s = s_vec;
				src_t = t_vec;
				src_s = src_s - tex->width * floor (src_s / tex->width);
				src_t = src_t - tex->height * floor (src_t / tex->height);
				src_is = (int)floor (src_s); // src_is = src_s % tex->width
				src_it = (int)floor (src_t); // src_it = src_t % tex->height
				src_is = qmax (0, qmin (src_is, tex->width - 1));
				src_it = qmax (0, qmin (src_it, tex->height - 1));
				src_index = tex->canvas[src_it * tex->width + src_is];
				VectorCopy (tex->palette[src_index], src_color);

				// get light from the center of the destination pixel
				light_s = (s_vec + resolution * (dest_is + 0.5 - dest_s)) / TEXTURE_STEP - texmins[0];
				light_t = (t_vec + resolution * (dest_it + 0.5 - dest_t)) / TEXTURE_STEP - texmins[1];
				GetLight (f, texsize, light_s, light_t, light);

				(*dest)[4] += 1;
				if (!(texname[0] == '{' && src_index == 255))
				{
					for (k = 0; k < 3; k++)
					{
						float v = src_color[k] * pow (light[k] / denominator, gamma);
						(*dest)[k] += 255 * qmax (0, qmin (v, 255));
					}
					(*dest)[3] += 255;
				}
			}
		}
		for (t = 0; t < texturesize[1]; t++)
		{
			for (s = 0; s < texturesize[0]; s++)
			{
				float (*src)[5] = &texture[t * texturesize[0] + s];
				byte (*dest)[4] = &texturemips[0][t * texturesize[0] + s];

				if ((*src)[4] == 0) // no samples (outside face range?)
				{
					VectorFill (*dest, 0);
					(*dest)[3] = 255;
				}
				else
				{
					if ((*src)[3] / (*src)[4] <= 0.4 * 255) // transparent
					{
						VectorFill (*dest, 0);
						(*dest)[3] = 0;
					}
					else // normal
					{
						for (j = 0; j < 3; j++)
						{
							int val = (int)floor ((*src)[j] / (*src)[3] + 0.5);
							(*dest)[j] = qmax (0, qmin (val, 255));
						}
						(*dest)[3] = 255;
					}
				}
			}
		}

		for (miplevel = 1; miplevel < MIPLEVELS; miplevel++)
		{
			for (t = 0; t < (texturesize[1] >> miplevel); t++)
			{
				for (s = 0; s < (texturesize[0] >> miplevel); s++)
				{
					byte (*src[4])[4];
					byte (*dest)[4];
					double average[4];

					dest = &texturemips[miplevel][t * (texturesize[0] >> miplevel) + s];
					src[0] = &texturemips[miplevel - 1][(2 * t) * (texturesize[0] >> (miplevel - 1)) + (2 * s)];
					src[1] = &texturemips[miplevel - 1][(2 * t) * (texturesize[0] >> (miplevel - 1)) + (2 * s + 1)];
					src[2] = &texturemips[miplevel - 1][(2 * t + 1) * (texturesize[0] >> (miplevel - 1)) + (2 * s)];
					src[3] = &texturemips[miplevel - 1][(2 * t + 1) * (texturesize[0] >> (miplevel - 1)) + (2 * s + 1)];

					VectorClear (average);
					average[3] = 0;
					for (k = 0; k < 4; k++)
					{
						for (j = 0; j < 3; j++)
						{
							average[j] += (*src[k])[3] * (*src[k])[j];
						}
						average[3] += (*src[k])[3];
					}

					if (average[3] / 4 <= 0.4 * 255)
					{
						VectorClear (*dest);
						(*dest)[3] = 0;
					}
					else
					{
						for (j = 0; j < 3; j++)
						{
							int val = (int)floor (average[j] / average[3] + 0.5);
							(*dest)[j] = qmax (0, qmin (val, 255));
						}
						(*dest)[3] = 255;
					}
				}
			}
		}

		// create its palette

		byte palette[256][3];
		cq_searchnode_t *palettetree = CQ_AllocSearchTree (256);
		int paletteoffset;
		int palettenumcolors;

		{
			int palettemaxcolors;
			int numsamplepoints;
			unsigned char (*samplepoints)[3];

			if (texname[0] == '{')
			{
				paletteoffset = 0;
				palettemaxcolors = 255;
				VectorCopy (tex->palette[255], palette[255]); // the transparency color
			}
			/*else if (texname[0] == '!')
			{
				paletteoffset = 16; // because the 4th entry and the 5th entry are reserved for fog color and fog density
				for (j = 0; j < 16; j++)
				{
					VectorCopy (tex->palette[j], palette[j]);
				}
				palettemaxcolors = 256 - 16;
			}*/
			else
			{
				paletteoffset = 0;
				palettemaxcolors = 256;
			}

			samplepoints = (unsigned char (*)[3])malloc (texturesize[0] * texturesize[1] * sizeof (unsigned char [3]));
			hlassume (samplepoints != NULL, assume_NoMemory);
			numsamplepoints = 0;
			for (t = 0; t < texturesize[1]; t++)
			{
				for (s = 0; s < texturesize[0]; s++)
				{
					byte (*src)[4] = &texturemips[0][t * texturesize[0] + s];
					if ((*src)[3] > 0)
					{
						VectorCopy (*src, samplepoints[numsamplepoints]);
						numsamplepoints++;
					}
				}
			}

			CQ_CreatePalette (numsamplepoints, samplepoints, palettemaxcolors, &palette[paletteoffset], palettenumcolors, palettetree);
			for (j = palettenumcolors; j < palettemaxcolors; j++)
			{
				VectorClear (palette[paletteoffset + j]);
			}

			free (samplepoints);
		}

		// emit a texinfo

		hlassume (g_numtexinfo < MAX_MAP_TEXINFO, assume_MAX_MAP_TEXINFO);
		f->texinfo = g_numtexinfo;
		texinfo_t *info = &g_texinfo[g_numtexinfo];
		g_numtexinfo++;

		*info = g_texinfo[originaltexinfonum];
		if (resolution != 1)
		{
			// apply a scale and a shift over the original vectors
			for (k = 0; k < 2; k++)
			{
				VectorScale (info->vecs[k], 1.0 / resolution, info->vecs[k]);
				info->vecs[k][3] = info->vecs[k][3] / resolution + 0.5;
			}
		}
		info->miptex = NewTextures_GetCurrentMiptexIndex ();

		// emit a texture

		int miptexsize;
		
		miptexsize = (int)sizeof (miptex_t);
		for (miplevel = 0; miplevel < MIPLEVELS; miplevel++)
		{
			miptexsize += (texturesize[0] >> miplevel) * (texturesize[1] >> miplevel);
		}
		miptexsize += 2 + 256 * 3 + 2;
		miptex_t *miptex = (miptex_t *)malloc (miptexsize);
		hlassume (miptex != NULL, assume_NoMemory);

		memset (miptex, 0, sizeof (miptex_t));
		miptex->width = texturesize[0];
		miptex->height = texturesize[1];
		byte *p = (byte *)miptex + sizeof (miptex_t);
		for (miplevel = 0; miplevel < MIPLEVELS; miplevel++)
		{
			miptex->offsets[miplevel] = p - (byte *)miptex;
			for (int t = 0; t < (texturesize[1] >> miplevel); t++)
			{
				for (int s = 0; s < (texturesize[0] >> miplevel); s++)
				{
					byte (*src)[4] = &texturemips[miplevel][t * (texturesize[0] >> miplevel) + s];
					if ((*src)[3] > 0)
					{
						if (palettenumcolors)
						{
							unsigned char point[3];
							VectorCopy (*src, point);
							*p = paletteoffset + CQ_MapPoint (point, &palette[paletteoffset], palettenumcolors, palettetree);
						}
						else // this should never happen
						{
							*p = paletteoffset + 0;
						}
					}
					else
					{
						*p = 255;
					}
					p++;
				}
			}
		}
		*(short *)p = 256;
		p += 2;
		memcpy (p, palette, 256 * 3);
		p += 256 * 3;
		*(short *)p = 0;
		p += 2;
		if (p != (byte *)miptex + miptexsize)
		{
			Error ("EmbedLightmapInTextures: internal error");
		}

		if (texname[0] == '{')
		{
			strcpy (miptex->name, "{_rad");
		}
		/*else if (texname[0] == '!')
		{
			strcpy (miptex->name, "!_rad");
		}*/
		else
		{
			strcpy (miptex->name, "__rad");
		}
		if (originaltexinfonum < 0 || originaltexinfonum > 99999)
		{
			Error ("EmbedLightmapInTextures: internal error: texinfo out of range");
		}
		miptex->name[5] = '0' + (originaltexinfonum / 10000) % 10; // store the original texinfo
		miptex->name[6] = '0' + (originaltexinfonum / 1000) % 10;
		miptex->name[7] = '0' + (originaltexinfonum / 100) % 10;
		miptex->name[8] = '0' + (originaltexinfonum / 10) % 10;
		miptex->name[9] = '0' + (originaltexinfonum) % 10;
		char table[62];
		for (int k = 0; k < 62; k++)
		{
			table[k] = k >= 36? 'a' + (k - 36): k >= 10? 'A' + (k - 10): '0' + k; // same order as the ASCII table
		}
		miptex->name[10] = '\0';
		miptex->name[11] = '\0';
		miptex->name[12] = '\0';
		miptex->name[13] = '\0';
		miptex->name[14] = '\0';
		miptex->name[15] = '\0';
		unsigned int hash = Hash (miptexsize, miptex);
		miptex->name[10] = table[(hash / 62 / 62) % 52 + 10];
		miptex->name[11] = table[(hash / 62) % 62];
		miptex->name[12] = table[(hash) % 62];
		miptex->name[13] = table[(count / 62) % 62];
		miptex->name[14] = table[(count) % 62];
		miptex->name[15] = '\0';
		NewTextures_PushTexture (miptexsize, miptex);
		count++;
		count_bytes += miptexsize;
		Developer (DEVELOPER_LEVEL_MESSAGE, "Created texture '%s' for face (texture %s) at (%4.3f %4.3f %4.3f)\n", miptex->name, texname, g_face_centroids[i][0], g_face_centroids[i][1], g_face_centroids[i][2]);

		free (miptex);

		CQ_FreeSearchTree (palettetree);
		
		free (texture);
		for (miplevel = 0; miplevel < MIPLEVELS; miplevel++)
		{
			free (texturemips[miplevel]);
		}
	}
	NewTextures_Write (); // update texdata now

	if (logged)
	{
		Log ("added %d texinfos and textures (%d bytes)\n", count, count_bytes);
	}
}

#endif
#endif

