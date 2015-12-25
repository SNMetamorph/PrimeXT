#include "messages.h"

// AJM: because these are repeated, they use up redundant memory. 
//  consequently ive made them into const strings which each occurance can point to.

// Common descriptions
const char* const internallimit = "The compiler tool hit an internal limit";
const char* const internalerror = "The compiler tool had an internal error";
const char* const maperror      = "The map has a problem which must be fixed";

// Common explanations
#define contact contactvluzacn //"contact zoner@gearboxsoftware.com immidiately"
const char* const selfexplanitory = "self explanitory";
const char* const reference       = "Check the file http://www.zhlt.info/common-mapping-problems.html for a detailed explanation of this problem";
const char* const simplify        = "The map is too complex for the game engine/compile tools to handle.  Simplify";
const char* const contactmerl     = "contact amckern@yahoo.com concerning this issue.";
const char* const contactvluzacn  = "contact vluzacn@163.com concerning this issue.";

static const MessageTable_t assumes[assume_last] = {
    {"invalid assume message", "This is a message should never be printed.", contact},

    // generic
    {"Memory allocation failure", "The program failled to allocate a block of memory.",
#ifdef ZHLT_64BIT_FIX
	#ifdef HLRAD
	 sizeof (int) <= 4? "The map is too complex for the compile tools to handle. Switch to the 64-bit version of hlrad if possible." :
     "Likely causes are (in order of likeliness) : the partition holding the swapfile is full; swapfile size is smaller than required; memory fragmentation; heap corruption"
	#else
	 contact
	#endif
#else
     "Likely causes are (in order of likeliness) : the partition holding the swapfile is full; swapfile size is smaller than required; memory fragmentation; heap corruption"
#endif
	},
    {"NULL Pointer", internalerror, contact},
    {"Bad Thread Workcount", internalerror, contact},

    // qcsg
    {"Missing '[' in texturedef (U)", maperror, reference},
    {"plane with no normal", maperror, reference},
    {"brush with coplanar faces", maperror, reference},
    {"brush outside world", maperror, reference},
    {"mixed face contents", maperror, reference},
    {"Brush type not allowed in world", maperror, reference},
    {"Brush type not allowed in entity", maperror, reference},
    {"No visibile brushes", "All brushes are CLIP or ORIGIN (at least one must be normal/visible)", selfexplanitory},
    {"Entity with ONLY an ORIGIN brush", "All entities need at least one visible brush to function properly.  CLIP, HINT, ORIGIN, do not count as visible brushes.", selfexplanitory},
    {"Could not find WAD file", "The compile tools could not locate a wad file that the map was referencing.", "Make sure the wad's listed in the level editor actually all exist"},
    {"Exceeded MAX_TRIANGLES", internallimit, contact},
    {"Exceeded MAX_SWITCHED_LIGHTS", "The maximum number of switchable light entities has been reached", selfexplanitory},
    {"Exceeded MAX_TEXFILES", internallimit, contact},

    // qbsp
    {"LEAK in the map", maperror, reference},
    {"Exceeded MAX_LEAF_FACES", "This error is almost always caused by an invalid brush, by having huge rooms, or scaling a texture down to extremely small values (between -1 and 1)",
     "Find the invalid brush.  Any imported prefabs, carved brushes, or vertex manipulated brushes should be suspect"},
    {"Exceeded MAX_WEDGES", internallimit, contact},
    {"Exceeded MAX_WVERTS", internallimit, contact},
    {"Exceeded MAX_SUPERFACEEDGES", internallimit, contact},
    {"Empty Solid Entity", "A solid entity in the map (func_wall for example) has no brushes.",  "If using Worldcraft, do a check for problems and fix any occurences of 'Empty solid'"},

    // vis
    {"Leaf portal saw into leaf", maperror, reference},
    {"Exceeded MAX_PORTALS_ON_LEAF", maperror, reference},
    {"Invalid client/server state", internalerror, contact},

    // qrad
    {"Exceeded MAX_TEXLIGHTS", "The maximum number of texture lights in use by a single map has been reached",
     "Use fewer texture lights."},
    {"Exceeded MAX_PATCHES", maperror, reference},
    {"Transfer < 0", internalerror, contact},
    {"Bad Surface Extents", maperror, reference},
    {"Malformed face normal", "The texture alignment of a visible face is unusable", "If using Worldcraft, do a check for problems and fix any occurences of 'Texture axis perpindicular to face'"},
    {"No Lights!", "lighting of map halted (I assume you do not want a pitch black map!)", "Put some lights in the map."},
    {"Bad Light Type", internalerror, contact},
    {"Exceeded MAX_SINGLEMAP", internallimit, contact},

    // common
    {"Unable to create thread", internalerror, contact},
    {"Exceeded MAX_MAP_PLANES", "The maximum number of plane definitions has been reached",
     "The map has grown too complex"},
    {"Exceeded MAX_MAP_TEXTURES", "The maximum number of textures for a map has been reached", selfexplanitory},

    {"Exceeded MAX_MAP_MIPTEX", "Texture memory usage on the map has exceeded the limit",
     "Merge similar textures, remove unused textures from the map"},
    {"Exceeded MAX_MAP_TEXINFO", internallimit, contact},
    {"Exceeded MAX_MAP_SIDES", internallimit, contact},
    {"Exceeded MAX_MAP_BRUSHES", "The maximum number of brushes for a map has been reached", selfexplanitory},
    {"Exceeded MAX_MAP_ENTITIES", "The maximum number of entities for the compile tools has been reached", selfexplanitory},
    {"Exceeded MAX_ENGINE_ENTITIES", "The maximum number of entities for the half-life engine has been reached", selfexplanitory},

    {"Exceeded MAX_MAP_MODELS", "The maximum number of brush based entities has been reached",
     "Remove unnecessary brush entities, consolidate similar entities into a single entity"},
    {"Exceeded MAX_MAP_VERTS", "The maximum number of vertices for a map has been reached", simplify}, // internallimit, contact //--vluzacn
    {"Exceeded MAX_MAP_EDGES", internallimit, contact},

    {"Exceeded MAX_MAP_CLIPNODES", maperror, reference},
    {"Exceeded MAX_MAP_MARKSURFACES", internallimit, contact},
    {"Exceeded MAX_MAP_FACES", "The maximum number of faces for a map has been reached", "This error is typically caused by having a large face with a small texture scale on it, or overly complex maps."},
    {"Exceeded MAX_MAP_SURFEDGES", internallimit, contact},
    {"Exceeded MAX_MAP_NODES", "The maximum number of nodes for a map has been reached", simplify},
    {"CompressVis Overflow", internalerror, contact},
    {"DecompressVis Overflow", internalerror, contact},
#ifdef ZHLT_MAX_MAP_LEAFS
	{"Exceeded MAX_MAP_LEAFS", "The maximum number of leaves for a map has been reached", simplify},
#endif
    {"Execution Cancelled", "Tool execution was cancelled either by the user or due to a fatal compile setting", selfexplanitory},
    {"Internal Error", internalerror, contact},
	//KGP added
	{"Exceeded MAX_MAP_LIGHTING","You have run out of light data memory" ,"Use the -lightdata <#> command line option to increase your maximum light memory.  The default is 32768 (KB)."}, // 6144 (KB) //--vluzacn
    {"Exceeded MAX_INTERNAL_MAP_PLANES", "The maximum number of plane definitions has been reached", "The map has grown too complex"},
#ifdef HLRAD_TEXTURE
	{"Could not locate WAD file", "The compile tools could not locate a wad file that the map was referencing.",
#ifdef ZHLT_NOWADDIR
	"Make sure the file '<mapname>.wa_' exists. This is a file generated by hlcsg and you should not delete it. If you have to run hlrad without this file, use '-waddir' to specify folders where hlrad can find all the wad files."
#else
	"Configure game directory pathes for hlrad in file '<tool path>\\settings.txt', and make sure this wad file either has been included into bsp by hlcsg or exists in one of the game directories."
#endif
	},
#endif
#ifdef ZHLT_64BIT_FIX
	{"Couldn't open extent file", "<mapname>.ext doesn't exist. This file is required by the " PLATFORM_VERSIONSTRING " version of hlrad.", "Make sure hlbsp has run correctly. Alternatively, run 'ripent.exe -writeextentfile <mapname>' to create the extent file."},
#endif
}; 

const MessageTable_t* GetAssume(assume_msgs id)
{
    if (!(id > assume_first && id < assume_last))//(!(id > assume_first) && (id < assume_last)) --vluzacn
    {
        id = assume_first;
    }
    return &assumes[id];
}



