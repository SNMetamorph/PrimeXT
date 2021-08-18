
#ifndef BSPFILE_31_H
#define BSPFILE_31_H

//=============================================================================

#define XT_BSPVERSION		31
#define IDEXTRAHEADER		(('H'<<24)+('S'<<16)+('A'<<8)+'X') // little-endian "XASH"
#define EXTRA_VERSION		4	// ver. 1 was occupied by old versions of XashXT, ver. 2 was occupied by old vesrions of P2:savior
					// ver. 3 was occupied by experimental versions of P2:savior change fmt
#define EXTRA_VERSION_OLD		2	// extra version 2 (P2:Savior regular version) to get minimal backward compatibility

#define HLFX_BSP_MAGIC_ID		0xFEED

#define DELUXEMAP_VERSION		1
#define IDDELUXEMAPHEADER		(('T'<<24)+('I'<<16)+('L'<<8)+'Q') // little-endian "QLIT"

// version 31
#define LUMP_CLIPNODES2		15	// hull0 goes into LUMP_NODES, hull1 goes into LUMP_CLIPNODES,
#define LUMP_CLIPNODES3		16	// hull2 goes into LUMP_CLIPNODES2, hull3 goes into LUMP_CLIPNODES3
#define HEADER_LUMPS_31		17

// HLFX combined with BSP30
#define HLFX30_LUMP_FACEINFO		15
#define HLFX30_LUMP_BUMP		16
#define HLFX30_LUMP_TNBASIS		17
#define HEADER_LUMPS_HLFX_30		18

// HLFX combined with BSP31
#define HLFX31_LUMP_FACEINFO		17
#define HLFX31_LUMP_BUMP		18
#define HLFX31_LUMP_TNBASIS		19
#define HEADER_LUMPS_HLFX_31		20

#define MAX_MAP_TNBASIS		0xFFFF0

typedef struct
{
	int		version;
	dlump_t		lumps[HEADER_LUMPS_31];
} dheader31_t;

typedef struct
{
	int		version;
	dlump_t		lumps[HEADER_LUMPS_HLFX_30];
	int		magicID;
} dheader30_hlfx_t;

typedef struct
{
	int		version;
	dlump_t		lumps[HEADER_LUMPS_HLFX_31];
	int		magicID;
} dheader31_hlfx_t;

typedef struct
{
	float		normal[3];
	float		tangent[3];
} dTNbasis_t;

typedef struct
{
	int		bumpofs;
	int		firstTN;
	short		numTN;
} dfacedata_t;

#endif