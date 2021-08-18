/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

//
// spritegen.c: generates a .spr file from a series of .lbm frame files.
// Result is stored in /raid/quake/id1/sprites/<scriptname>.spr.
//
#include <stdlib.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "scriplib.h"
#include "imagelib.h"
#include "sprite.h"
#include "filesystem.h"

#define MIN_INTERVAL	0.001f
#define MAX_INTERVAL	64.0f
#define MAX_BUFFER_SIZE	(10 * 1024 * 1024)	// 10 Mb for now
#define MAX_FRAMES		512

dsprite_t		sprite;
rgbdata_t		*frame = NULL;
byte		*lumpbuffer = NULL, *plump;
static byte	basepalette[256*3];
char		spritedir[1024];
char		spriteoutname[1024];
int		framesmaxs[2];
int		framecount;
float		g_gamma = 1.8f;
float		frameinterval;
int		origin_x;
int		origin_y;
bool		need_resample;
bool		ignore_resample;
int		resample_w;
int		resample_h;

typedef struct
{
	frametype_t	type;		// single frame or group of frames
	void		*pdata;		// either a dspriteframe_t or group info
	float		interval;		// only used for frames in groups
	int		numgroupframes;	// only used by group headers
} spritepackage_t;

spritepackage_t	frames[MAX_FRAMES];

int verify_atoi( const char *token )
{
	if( token[0] != '-' && ( token[0] < '0' || token[0] > '9' ))
	{
		TokenError( "expecting number, got \"%s\"\n", token );
	}
	return atoi( token );
}

float verify_atof( const char *token )
{
	if( token[0] != '-' && token[0] != '.' && ( token[0] < '0' || token[0] > '9' ))
	{
		TokenError( "expecting number, got \"%s\"\n", token );
	}
	return atof( token );
}

/*
============
WriteFrame
============
*/
void WriteFrame( vfile_t *file, int framenum )
{
	dspriteframe_t	*pframe;
	dspriteframe_t	frametemp;

	pframe = (dspriteframe_t *)frames[framenum].pdata;
	frametemp.origin[0] = pframe->origin[0];
	frametemp.origin[1] = pframe->origin[1];
	frametemp.width = pframe->width;
	frametemp.height = pframe->height;

	VFS_Write( file, &frametemp, sizeof( frametemp ));
	VFS_Write( file, (byte *)(pframe + 1), pframe->height * pframe->width );
}

/*
============
WriteSprite
============
*/
void WriteSprite( vfile_t *file )
{
	int	i, groupframe, curframe;
	dsprite_t	spritetemp;
	short	cnt = 256;

	sprite.boundingradius = sqrt((( framesmaxs[0] >> 1 ) * ( framesmaxs[0] >> 1 )) + (( framesmaxs[1] >> 1 ) * ( framesmaxs[1] >> 1 )));

	// write out the sprite header
	spritetemp.type = sprite.type;
	spritetemp.texFormat = sprite.texFormat;
	spritetemp.boundingradius = sprite.boundingradius;
	spritetemp.bounds[0] = framesmaxs[0];
	spritetemp.bounds[1] = framesmaxs[1];
	spritetemp.numframes = sprite.numframes;
	spritetemp.beamlength = sprite.beamlength;
	spritetemp.synctype = sprite.synctype;
	spritetemp.version = SPRITE_VERSION;
	spritetemp.ident = IDSPRITEHEADER;

	VFS_Write( file, &spritetemp, sizeof( spritetemp ));

	// Write out palette in 16bit mode
	VFS_Write( file, (void *)&cnt, sizeof( cnt ));
	VFS_Write( file, basepalette, sizeof( basepalette ));

	// write out the frames
	curframe = 0;

	for( i = 0; i < sprite.numframes; i++ )
	{
		VFS_Write( file, &frames[curframe].type, sizeof( frames[curframe].type ));

		if( frames[curframe].type == FRAME_SINGLE )
		{
			// single (non-grouped) frame
			WriteFrame( file, curframe );
			curframe++;
		}
		else
		{
			int		j, numframes;
			float		totinterval;
			dspritegroup_t	dsgroup;

			groupframe = curframe;
			curframe++;
			numframes = frames[groupframe].numgroupframes;

			// set and write the group header
			dsgroup.numframes = numframes;

			VFS_Write( file, &dsgroup, sizeof( dsgroup ));

			// write the interval array
			totinterval = 0.0f;

			for( j = 0; j < numframes; j++ )
			{
				dspriteinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = totinterval;

				VFS_Write( file, &temp, sizeof( temp ));
			}

			for( j = 0; j < numframes; j++ )
			{
				WriteFrame( file, curframe );
				curframe++;
			}
		}
	}
}

/*
==============
ResetSpriteInfo
==============
*/
void ResetSpriteInfo( void )
{
	// set default sprite parms
	spriteoutname[0] = 0;

	memset( &sprite, 0, sizeof( sprite ));
	memset( frames, 0, sizeof( frames ));
	memset( basepalette, 0, sizeof( basepalette ));
	framecount = origin_x = origin_y = 0;
	frameinterval = 0.0f;
	g_gamma = 1.8f;

	framesmaxs[0] = -9999;
	framesmaxs[1] = -9999;
	sprite.type = SPR_FWD_PARALLEL;
	sprite.synctype = ST_RAND; // default
	if( frame ) Mem_Free( frame );
	frame = NULL;
}

/*
==============
WriteSPRFile
==============
*/
void WriteSPRFile( void )
{
	int	i, groups = 0, grpframes = 0;
	int	sngframes = framecount;

	if( sprite.numframes == 0 ) 
		COM_FatalError( "WriteSPRFile: no frames\n" );

	if( !Q_strlen( spriteoutname ))
		COM_FatalError( "WriteSPRFile: didn't name sprite file\n" );

	vfile_t	*f = VFS_Create();
	WriteSprite( f );

	Msg( "writing: %s\n", spriteoutname );
	COM_SaveFile( spriteoutname, VFS_GetBuffer( f ), VFS_Tell( f ), true );

	VFS_Close( f );

	// count frames
	for( i = 0; i < framecount; i++ )
	{
		if( frames[i].numgroupframes ) 
		{
			sngframes -= frames[i].numgroupframes;
			grpframes += frames[i].numgroupframes;
			groups++;
		}
	}

	// display info about current sprite
	if( groups )
	{
		Msg( "%d group%s,", groups, groups > 1 ? "s":"" );
		Msg( " contain %d frame%s\n", grpframes, grpframes > 1 ? "s":"" );
	}

	if( sngframes - groups )
		Msg( "%d ungrouped frame%s\n", sngframes - groups, (sngframes - groups) > 1 ? "s" : "" );	

	ResetSpriteInfo();

	if(( plump - lumpbuffer ) > MAX_BUFFER_SIZE )
		COM_FatalError( "WriteSPRFile: memory buffer is corrupted. Check the sprite!\n" );
}

/*
==============
Cmd_Spritename
==============
*/
void Cmd_Spritename( void )
{
	if( sprite.numframes )
		WriteSPRFile();

	GetToken( false );
	Q_snprintf( spriteoutname, sizeof( spriteoutname ), "%s/%s", spritedir, token );
	COM_DefaultExtension( spriteoutname, ".spr" );

	if( !lumpbuffer )
		lumpbuffer = (byte *)Mem_Alloc( MAX_BUFFER_SIZE );
	else memset( lumpbuffer, 0, MAX_BUFFER_SIZE );

	plump = lumpbuffer;
}

/*
==============
LoadScreen
==============
*/
void LoadScreen( const char *filename, const char *displayname )
{
	if( frame != NULL )
		Mem_Free( frame );	// release previous frame

	if( !COM_FileExists( filename ))
		COM_FatalError( "%s doesn't exist\n", filename );

	MsgDev( D_INFO, "grabbing: %s\t\t%s\n", filename, displayname );
	frame = COM_LoadImage( filename );
	if( !frame ) COM_FatalError( "%s couldn't load", filename ); // ???

	// get support for doom-angled sprites
	while( TryToken( ))
	{
		if( !Q_stricmp( token, "flip_diagonal" ))
			SetBits( frame->flags, IMAGE_ROT_90 );
		else if( !Q_stricmp( token, "flip_y" ))
			SetBits( frame->flags, IMAGE_FLIP_Y );
		else if( !Q_stricmp( token, "flip_x" ))
			SetBits( frame->flags, IMAGE_FLIP_X );
	}

	int new_width = Q_min( frame->width, MIP_MAXWIDTH );
	int new_height = Q_min( frame->height, MIP_MAXHEIGHT );

	// TODO: merge all frames into single image, quantize it and then deconstruct back to single frames
	// TODO: only for truecolor images...
	frame = Image_Resample( frame, new_width, new_height );
	frame = Image_Quantize( frame );	// quantize if needs
	frame = Image_Flip( frame );		// rotate image if desired

	if( sprite.texFormat == SPR_ALPHTEST )
		Image_MakeOneBitAlpha( frame ); // check alpha

	if( sprite.numframes == 0 )
		memcpy( basepalette, frame->palette, sizeof( basepalette ));
	else if( memcmp( basepalette, frame->palette, sizeof( basepalette )))
		MsgDev( D_WARN, "\"%s\" doesn't share a pallette with the previous bitmap\n", filename );
}

/*
===============
Cmd_Type

syntax: "$type preset"
===============
*/
void Cmd_Type( void )
{
	GetToken( false );

	if( !Q_stricmp( token, "vp_parallel_upright" ))
		sprite.type = SPR_FWD_PARALLEL_UPRIGHT;
	else if( !Q_stricmp( token, "facing_upright" ))
		sprite.type = SPR_FACING_UPRIGHT;
	else if( !Q_stricmp( token, "vp_parallel" ))
		sprite.type = SPR_FWD_PARALLEL;
	else if( !Q_stricmp( token, "oriented" ))
		sprite.type = SPR_ORIENTED;
	else if( !Q_stricmp( token, "vp_parallel_oriented" ))
		sprite.type = SPR_FWD_PARALLEL_ORIENTED;
	else COM_FatalError( "bad sprite type\n" );
}

/*
===============
Cmd_Texture

syntax: "$texture preset"
syntax: "$rendermode preset"
===============
*/
void Cmd_Texture( void )
{
	GetToken( false );

	if( !Q_stricmp( token, "additive" ))
		sprite.texFormat = SPR_ADDITIVE;
	else if( !Q_stricmp( token, "normal" ))
		sprite.texFormat = SPR_NORMAL;
	else if( !Q_stricmp( token, "indexalpha" ))
		sprite.texFormat = SPR_INDEXALPHA;
	else if( !Q_stricmp( token, "alphatest" ))
		sprite.texFormat = SPR_ALPHTEST;
	else COM_FatalError( "bad sprite texture type\n" );
}

/*
===============
Cmd_Beamlength
===============
*/
void Cmd_Beamlength( void )
{
	GetToken( false );
	sprite.beamlength = verify_atof( token );
}

/*
===============
Cmd_Framerate

syntax: "$framerate value"
===============
*/
void Cmd_Framerate( void )
{
	GetToken( false );
	float framerate = verify_atof( token );
	if( framerate <= 0.0f ) COM_FatalError( "bad framerate %g\n", framerate );
	frameinterval = bound( MIN_INTERVAL, (1.0f / framerate), MAX_INTERVAL );	
}

/*
===============
Cmd_Resample

syntax: "$resample <w h>"
===============
*/
void Cmd_Resample( void )
{
	GetToken( false );
	resample_w = verify_atoi( token );
	GetToken( false );
	resample_h = verify_atoi( token );

	if( !ignore_resample )
		need_resample = true;
}

/*
===============
Cmd_NoResample

syntax: "$noresample"
===============
*/
void Cmd_NoResample( void )
{
	ignore_resample = true;
}

/*
===============
Cmd_Load

syntax "$load fire01.bmp"
===============
*/
void Cmd_Load( const char *displayname )
{
	char	path[1024];

	GetToken( false );

	Q_snprintf( path, sizeof( path ), "%s/%s", spritedir, token );
	LoadScreen( path, displayname );//COM_ExpandArg( token ));
}


/*
===============
Cmd_Origin

syntax: $origin "x_pos y_pos"
===============
*/
static void Cmd_Origin( void )
{
	GetToken( false );
	origin_x = verify_atoi( token );
	GetToken( false );
	origin_y = verify_atoi( token );
}

/*
===============
Cmd_Frame

syntax "$frame xoffset yoffset width height <interval> <origin x> <origin y>"
===============
*/
void Cmd_Frame( void )
{
	int		x, y, xl, yl, xh, yh, w, h;
	byte		*screen_p, *source;
	bool		resampled = false;
	int		org_x, org_y;
	int		linedelta;
	dspriteframe_t	*pframe;
	int		pix;

	if( !frame || !frame->buffer )
		COM_FatalError( "frame not loaded\n" );

	if( framecount >= MAX_FRAMES )
		COM_FatalError( "too many frames in package\n" );
	
	GetToken( false );
	xl = verify_atoi( token );
	GetToken( false );
	yl = verify_atoi( token );
	GetToken( false );
	w = verify_atoi( token );
	GetToken( false );
	h = verify_atoi( token );

	// merge bounds
	if( xl <= 0 || xl > frame->width )
		xl = 0;
	if( yl <= 0 || yl > frame->width )
		yl = 0;
	if( w <= 0 || w > frame->width )
		w = frame->width;
	if( h <= 0 || h > frame->height )
		h = frame->height;

	if(( xl & 0x07 ) || ( yl & 0x07 ) || ( w & 0x07 ) || ( h & 0x07 ))
	{
		if( need_resample )
		{
			rgbdata_t *dst = Image_Resample( frame, resample_w, resample_h ); 
			if( frame != dst ) resampled = true;
			frame = dst;
		}
		if( !resampled ) MsgDev( D_WARN, "frame dimensions not multiples of 8\n" );
	}

	if(( w > MIP_MAXWIDTH ) || ( h > MIP_MAXHEIGHT ))
		COM_FatalError( "sprite has a dimension longer than %dx%d\n", MIP_MAXWIDTH, MIP_MAXHEIGHT );

	// get interval
	if( TokenAvailable( )) 
	{
		GetToken( false );
		frames[framecount].interval = verify_atof( token );
		if( frames[framecount].interval <= 0.0f ) MsgDev( D_WARN, "non-positive interval\n" );
		frames[framecount].interval = bound( MIN_INTERVAL, frames[framecount].interval, MAX_INTERVAL );
	}
	else if( frameinterval != 0.0f )
	{
		frames[framecount].interval = frameinterval;
	}
	else
	{
		// use default interval
		frames[framecount].interval = (float)0.1f;
	} 

	if( TokenAvailable( ))
	{
		GetToken (false);
		org_x = -verify_atoi( token );
		GetToken( false );
		org_y = verify_atoi( token );
	}
	else if(( origin_x != 0 ) && ( origin_y != 0 ))
	{
		// write shared origin
		org_x = -origin_x;
		org_y = origin_y;
	}
	else
	{
		// use center of rectangle
		org_x = -(w >> 1);
		org_y = h >> 1;
	}

	// merge all sprite info
	if( need_resample && resampled )
	{
		// check for org[n] == size[n] and org[n] == size[n]/2
		// another cases will be not changed
		if( org_x == -w )
			org_x = -frame->width;
		else if( org_x == -( w >> 1 ))
			org_x = -frame->width >> 1;
		if( org_y == h )
			org_y = frame->height;
		else if( org_y == ( h >> 1 ))
			org_y = frame->height >> 1;

		w = frame->width;
		h = frame->height;
	}

	xh = xl + w;
	yh = yl + h;

	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = FRAME_SINGLE;

	pframe->origin[0] = org_x;
	pframe->origin[1] = org_y;
	pframe->width = w;
	pframe->height = h;

	if( w > framesmaxs[0] ) framesmaxs[0] = w;
	if( h > framesmaxs[1] ) framesmaxs[1] = h;
	
	plump = (byte *)(pframe + 1);

	screen_p = frame->buffer + yl * frame->width + xl;
	linedelta = frame->width - w;
	source = plump;

	for( y = yl; y < yh; y++ )
	{
		for( x = xl; x < xh; x++ )
		{
			pix = *screen_p;
			*screen_p++ = 0; // DEBUG
//			if( pix == 255 )
//				pix = 0;
			*plump++ = pix;
		}
		screen_p += linedelta;
	}
	framecount++;
}

/*
===============
Cmd_Group

syntax: 
$group or $angled
{
	$load fire01.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
	$load fire02.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>"
	$load fire03.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
}	
===============
*/
void Cmd_Group( bool angled )
{
	int	groupframe;
	int	depth = 0;

	groupframe = framecount++;

	frames[groupframe].type = angled ? FRAME_ANGLED : FRAME_GROUP;
	resample_w = resample_h = 0; // invalidate resample for group 
	frames[groupframe].numgroupframes = 0;
	need_resample = false;

	while( 1 )
	{
		GetToken( true );

		if( endofscript )
		{
			if( depth != 0 )
				COM_FatalError( "missing }\n" );
			break;
                    }

		if( !Q_stricmp( token, "{" ))
		{
			depth++;
		}
		else if( !Q_stricmp( token, "}" ))
		{
			depth--;
			break; // end of group
		}
		else if( !Q_stricmp( token, "$framerate" ))
		{
			Cmd_Framerate();
		}
		else if( !Q_stricmp( token, "$resample" ))
		{
			Cmd_Resample();
		}
		else if( !Q_stricmp( token, "$frame" ))
		{
			Cmd_Frame();
			frames[groupframe].numgroupframes++;
		}
		else if( !Q_stricmp( token, "$load" ))
		{
			Cmd_Load( angled ? "[^3angled frame^7]" : "[^2group frame^7]" );
		}
		else
		{
			while( TryToken( )); // skip unknown commands
		}
	}

	if( depth != 0 )
		COM_FatalError( "missing }\n" );

	if( frames[groupframe].numgroupframes == 0 ) 
	{
		// don't create blank groups, rewind frames
		MsgDev( D_WARN, "Cmd_Group: remove blank group\n" );
		sprite.numframes--;
		framecount--;
	}
	else if( angled && frames[groupframe].numgroupframes != 8 ) 
	{
		// don't create blank groups, rewind frames
		MsgDev(D_WARN, "Cmd_Group: remove angled group with invalid framecount\n" );
		sprite.numframes--;
		framecount--;
	}

	// back to single frames, invalidate resample
	resample_w = resample_h = 0;
	need_resample = false;
}

/*
===============
Cmd_GroupStart	

syntax: 
$groupstart
	$load fire01.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
	$load fire02.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>"
	$load fire03.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
$groupend
===============
*/
void Cmd_GroupStart( void )
{
	int	groupframe;

	groupframe = framecount++;
	resample_w = resample_h = 0; // invalidate resample for group 
	frames[groupframe].type = FRAME_GROUP;
	frames[groupframe].numgroupframes = 0;
	need_resample = false;

	while( 1 )
	{
		GetToken( true );

		if( endofscript )
			COM_FatalError( "end of file during group\n" );

		if( !Q_stricmp( token, "$frame" ))
		{
			Cmd_Frame();
			frames[groupframe].numgroupframes++;
		}
		else if( !Q_stricmp( token, "$load" ))
		{
			Cmd_Load( "[^2group frame^7]" );
		}
		else if( !Q_stricmp( token, "$framerate" ))
		{
			Cmd_Framerate();
		}
		else if( !Q_stricmp( token, "$resample" ))
		{
			Cmd_Resample();
		}
		else if( !Q_stricmp( token, "$groupend" ))
		{
			break;
		}
		else
		{
			while( TryToken( )); // skip unknown commands
		}

	}

	if( frames[groupframe].numgroupframes == 0 ) 
	{
		// don't create blank groups, rewind frames
		MsgDev( D_WARN, "remove blank group\n" );
		sprite.numframes--;
		framecount--;
	}

	// back to single frames, invalidate resample
	resample_w = resample_h = 0;
	need_resample = false;
}

/*
===============
ParseScript	
===============
*/
void ParseScript( void )
{
	while( 1 )
	{
		GetToken( true );

		if( endofscript )
			break;
	
		if( !Q_stricmp( token, "$load" ))
		{
			Cmd_Load ( "[^1frame^7]" );
		}
		else if( !Q_stricmp( token, "$spritename" ))
		{
			Cmd_Spritename ();
		}
		else if( !Q_stricmp( token, "$type" ))
		{
			Cmd_Type ();
		}
		else if( !Q_stricmp( token, "$texture" ))
		{
			Cmd_Texture ();
		}
		else if( !Q_stricmp( token, "$beamlength" ))
		{
			Cmd_Beamlength ();
		}
		else if( !Q_stricmp( token, "$origin" ))
		{
			Cmd_Origin();
		}
		else if( !Q_stricmp( token, "$sync" ))
		{
			sprite.synctype = ST_SYNC;
		}
		else if( !Q_stricmp( token, "$frame" ))
		{
			Cmd_Frame();
			sprite.numframes++;
		}		
		else if( !Q_stricmp( token, "$group" )) 
		{
			Cmd_Group( false );
			sprite.numframes++;
		}
		else if( !Q_stricmp( token, "$angled" )) 
		{
			Cmd_Group( true );
			sprite.numframes++;
		}
		else if( !Q_stricmp( token, "$groupstart" ))
		{
			Cmd_GroupStart();
			sprite.numframes++;
		}
		else if( !Q_stricmp( token, "$noresample" ))
		{
			Cmd_NoResample();
		}
		else if( !Q_stricmp( token, "$resample" ))
		{
			Cmd_Resample();
		}
		else
		{
			while( TryToken( )); // skip unknown commands
		}
	}
}

/*
==============
main
	
==============
*/
int main( int argc, char **argv )
{
	int	i;
	char	path[1024];
	bool	log_append = false;
	bool	log_exist = false;

	atexit( Sys_CloseLog );
	COM_InitCmdlib( argv, argc );

	if( argc == 1 )
	{
		Msg( "	    P2:Savior Sprite Model Compiler\n" );
		Msg( "		 XashXT Group 2018(^1c^7)\n\n\n" );

		Msg( "usage: spritegen <options> file.qc\n"
		"\nlist options:\n"
		"^2-a^7 - append logfile\n"
		"^2-dev^7 - shows developer messages\n"
		"\n\t\tPress any key to exit" );

		system( "pause>nul" );
		return 1;
	}
		
	for( i = 1; i < argc - 1; i++ )
	{
		if( argv[i][0] == '-' )
		{
			if( !Q_stricmp( argv[i], "-dev" ))
			{
				SetDeveloperLevel( verify_atoi( argv[i+1] ));
				i++;
				continue;
			}
		}
		switch( argv[i][1] )
		{
		case 'a':
			log_append = true;
			break;
		}
	}

	if( !argv[i] ) return 1;

	log_exist = COM_FileExists( "spritegen.log" );

	if( log_append )
		Sys_InitLogAppend( "spritegen.log" );
	else Sys_InitLog( "spritegen.log" );

	if( !log_exist || !log_append )
	{
		Msg( "	    P2:Savior Sprite Model Compiler\n" );
		Msg( "		 XashXT Group 2018(^1c^7)\n\n" );
	}

	// load the script
	Q_strncpy( path, argv[i], sizeof( path ));
	COM_ExtractFilePath( path, spritedir );
	COM_DefaultExtension( path, ".qc" );
	Msg( "parse: %s\n", path );
	LoadScriptFile( path );

	// parse it	
	ParseScript ();
	WriteSPRFile ();
	Mem_Free( lumpbuffer );

	SetDeveloperLevel( D_REPORT );
	Mem_Check(); // report leaks
	if( log_append ) Msg( "\n" );

	return 0;
}