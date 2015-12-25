#ifndef CMDLIB_H__
#define CMDLIB_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4101)	// unreferenced local variable

//#define MODIFICATIONS_STRING "Submit detailed bug reports to (zoner@gearboxsoftware.com)\n"
//#define MODIFICATIONS_STRING "Submit detailed bug reports to (merlinis@bigpond.net.au)\n"
//#define MODIFICATIONS_STRING "Submit detailed bug reports to (amckern@yahoo.com)\n"
#define MODIFICATIONS_STRING "Submit detailed bug reports to (vluzacn@163.com)\n" //--vluzacn

#ifdef _DEBUG
#define ZHLT_VERSIONSTRING "v3.4 dbg"
#else
#define ZHLT_VERSIONSTRING "v3.4"
#endif

#define HACK_VERSIONSTRING "VL34" //--vluzacn

#if !defined (HLCSG) && !defined (HLBSP) && !defined (HLVIS) && !defined (HLRAD) && !defined (RIPENT) //--vluzacn
#error "You must define one of these in the settings of each project: HLCSG, HLBSP, HLVIS, HLRAD, RIPENT. The most likely cause is that you didn't load the project from the sln file."
#endif
#if !defined (VERSION_32BIT) && !defined (VERSION_64BIT) && !defined (VERSION_LINUX) && !defined (VERSION_OTHER) //--vluzacn
#error "You must define one of these in the settings of each project: VERSION_32BIT, VERSION_64BIT, VERSION_LINUX, VERSION_OTHER. The most likely cause is that you didn't load the project from the sln file."
#endif

#ifdef VERSION_32BIT
#define PLATFORM_VERSIONSTRING "32-bit"
#define PLATFORM_CAN_CALC_EXTENT
#endif
#ifdef VERSION_64BIT
#define PLATFORM_VERSIONSTRING "64-bit"
#define PLATFORM_CAN_CALC_EXTENT
#endif
#ifdef VERSION_LINUX
#define PLATFORM_VERSIONSTRING "linux"
#define PLATFORM_CAN_CALC_EXTENT
#endif
#ifdef VERSION_OTHER
#define PLATFORM_VERSIONSTRING "???"
#endif

//=====================================================================
// AJM: Different features of the tools can be undefined here
//      these are not officially beta tested, but seem to work okay

// ZHLT_* features are spread across more than one tool. Hence, changing
//      one of these settings probably means recompiling the whole set
#define ZHLT_INFO_COMPILE_PARAMETERS        // ALL TOOLS
#define ZHLT_NULLTEX                        // HLCSG, HLBSP
#define ZHLT_TEXLIGHT                       // HLCSG, HLRAD - triggerable texlights by LRC
#define ZHLT_GENERAL                        // ALL TOOLS - general changes
#define ZHLT_NEW_FILE_FUNCTIONS             // ALL TOOLS - file path/extension extraction functions
//#define ZHLT_DETAIL                         // HLCSG, HLBSP - detail brushes     //should never turn on
//#define ZHLT_PROGRESSFILE                   // ALL TOOLS - estimate progress reporting to -progressfile //should never turn on
//#define ZHLT_NSBOB //should never turn on
#define ZHLT_VectorMA_FIX //--vluzacn
#define ZHLT_LARGERANGE //--vluzacn
//#define ZHLT_CONSOLE //--vluzacn
#define ZHLT_PARAMFILE //--vluzacn
//#define ZHLT_LANGFILE //--vluzacn
//#define ZHLT_DELETEKEY //--vluzacn
#define ZHLT_ENTITY_LIGHTSHADOW //--vluzacn
	#ifdef ZHLT_DELETEKEY
#define ZHLT_ENTITY_INFOSUNLIGHT //--vluzacn
	#endif
#define ZHLT_PLANETYPE_FIX // Very Important !! --vluzacn
#define ZHLT_WINDING_FIX // Very Important !! --vluzacn
#define ZHLT_FILE_FUNCTIONS_FIX //--vluzacn
#define ZHLT_MAX_MAP_LEAFS //--vluzacn
#define ZHLT_WINDING_RemoveColinearPoints_VL //Important //--vluzacn
#define ZHLT_WINDING_EPSILON //--vluzacn
#define ZHLT_BOUNDINGBOX_PRECISION_FIX //--vluzacn
#define ZHLT_ENTITY_LIGHTSURFACE //--vluzacn
#define ZHLT_CHART_AllocBlock //--vluzacn
#define ZHLT_TEXNAME_CHARSET //--vluzacn
#define ZHLT_NOWADDIR //--vluzacn
#define ZHLT_XASH // build the compiler for Xash engine //--vluzacn
	#ifdef ZHLT_XASH
#define ZHLT_XASH2 // build the compiler for Xash engine with change in bsp format //--vluzacn
	#endif
#define ZHLT_CHART_WADFILES //--vluzacn
#define ZHLT_DEFAULTEXTENSION_FIX //--vluzacn
#define ZHLT_FREETEXTUREAXIS //--vluzacn
#define ZHLT_WARNWORLDFACES //--vluzacn
	#ifdef ZHLT_ENTITY_LIGHTSHADOW
#define ZHLT_ENTITY_LIGHTBOUNCE //--vluzacn
	#endif
#define ZHLT_DecompressVis_FIX //--vluzacn
#define ZHLT_64BIT_FIX //--vluzacn
	#ifdef ZHLT_64BIT_FIX
#define ZHLT_EMBEDLIGHTMAP // this feature requires HLRAD_TEXTURE and RIPENT_TEXTURE //--vluzacn
	#endif
//#define ZHLT_HIDDENSOUNDTEXTURE //--vluzacn

#define COMMON_HULLU // winding optimisations by hullu

	#ifdef SYSTEM_WIN32
#define RIPENT_PAUSE //--vluzacn
	#endif
#define RIPENT_TEXTURE //--vluzacn

// tool specific settings below only mean a recompile of the tool affected
#define HLCSG_CLIPECONOMY
#define HLCSG_WADCFG
#define HLCSG_WADCFG_NEW // rewritten HLCSG_WADCFG. --vluzacn
#define HLCSG_AUTOWAD

#define HLCSG_PRECISIONCLIP
#define HLCSG_FASTFIND
#ifdef ZHLT_NULLTEX
        #define HLCSG_NULLIFY_INVISIBLE //requires null textures as prerequisite
#endif

#define HLCSG_COPYBRUSH //--vluzacn
	#ifdef HLCSG_COPYBRUSH
	#ifdef HLCSG_CLIPECONOMY
#define HLCSG_CUSTOMHULL //--vluzacn
	#endif
	#endif
	#ifdef ZHLT_DELETEKEY
#define HLCSG_SCALESIZE //--vluzacn
	#endif
#define HLCSG_SEARCHWADPATH_VL //--vluzacn
	#ifdef SYSTEM_WIN32
//#define HLCSG_GAMETEXTMESSAGE_UTF8 //--vluzacn
	#endif
#define HLCSG_LOGVERSION //--vluzacn
	#ifdef HLCSG_COPYBRUSH
#define HLCSG_COPYMODELKEYVALUE //--vluzacn
	#endif
#define HLCSG_CheckBrushContents_FIX //--vluzacn
#define HLCSG_ERROR_MISSINGTEXTURE //--vluzacn
#define HLCSG_BEVELMISSINGFIX //--vluzacn
#define HLCSG_SORTBRUSH_FIX //--vluzacn
#define HLBSP_REMOVEHULL2 //--vluzacn
#define HLCSG_CHART_FIX //--vluzacn
#define HLCSG_TextureContents_FIX //--vluzacn
#define HLCSG_KEEPLOG //--vluzacn
#define HLCSG_FUNCGROUP_FIX //--vluzacn
#define HLCSG_COUNT_NEW //--vluzacn
#define HLCSG_ALLOWHINTINENTITY //--vluzacn
#define HLCSG_PRICISION_FIX // Important!! --vluzacn
#define HLBSP_MAX_LEAF_FACES //--vluzacn
	#ifdef ZHLT_NULLTEX
#define HLBSP_SKY_SOLID //--vluzacn
#define HLCSG_HLBSP_CONTENTSNULL_FIX //--vluzacn
	#endif
#define HLCSG_HLBSP_ALLOWEMPTYENTITY // needs more testing --vluzacn
//#define HLBSP_ChooseMidPlane_FIX //--vluzacn
	#ifdef HLBSP_ChooseMidPlane_FIX
#define HLBSP_ChoosePlane_VL //--vluzacn
	#endif
#define HLCSG_HLBSP_REDUCETEXTURE //--vluzacn
#define HLBSP_DELETELEAKFILE //--vluzacn
#define HLBSP_FILL //--vluzacn
//#define HLBSP_WARNMIXEDCONTENTS //--vluzacn
#define HLBSP_NULLFACEOUTPUT_FIX //--vluzacn
#define HLCSG_STRIPWADPATH //--vluzacn
#define HLCSG_NOREDUNDANTKEY //--vluzacn
#define HLCSG_HLBSP_CUSTOMBOUNDINGBOX //--vluzacn
#define HLCSG_HLBSP_VOIDTEXINFO //--vluzacn
	#ifdef HLCSG_HLBSP_VOIDTEXINFO
	#ifdef HLCSG_SORTBRUSH_FIX
#define HLCSG_EMPTYBRUSH //--vluzacn
	#endif
	#endif
	#ifdef HLCSG_EMPTYBRUSH
#define HLCSG_WATERBACKFACE_FIX // remove this if you have fixed the engine's bug of drawing water backface. --vluzacn
#define HLCSG_NOSPLITBYHINT //--vluzacn
#define HLCSG_CUSTOMCONTENT //--vluzacn
	#endif
#define HLBSP_NewFaceFromFace_FIX //--vluzacn
	#ifdef ZHLT_DELETEKEY
#define HLCSG_OPTIMIZELIGHTENTITY //--vluzacn
	#endif
#define HLCSG_STYLEHACK //--vluzacn
#define HLCSG_CSGBrush_BRUSHNUM_FIX //--vluzacn
#define HLBSP_TJUNC_PRECISION_FIX //--vluzacn
	#ifdef HLBSP_ChoosePlane_VL
#define HLBSP_BALANCE //--vluzacn
	#endif
#define HLBSP_TryMerge_PLANENUM_FIX //--vluzacn
#define HLCSG_SORTBRUSH_KEEPORDER //--vluzacn
#define HLCSG_FACENORMALEPSILON //--vluzacn
	#ifdef ZHLT_WINDING_EPSILON
#define HLCSG_MakeHullFaces_PRECISE //--vluzacn
	#endif
#define HLCSG_VIEWSURFACE //--vluzacn
#define HLBSP_TryMerge_PRECISION_FIX //--vluzacn
	#ifdef ZHLT_WINDING_RemoveColinearPoints_VL
#define HLBSP_REMOVECOLINEARPOINTS //--vluzacn
	#endif
#define HLBSP_SubdivideFace_FIX //--vluzacn
#define HLCSG_HLBSP_DOUBLEPLANE //--vluzacn
	#ifdef HLBSP_ChoosePlane_VL
#define HLBSP_AVOIDEPSILONSPLIT //--vluzacn
	#endif
#define HLCSG_HULLFILE_AUTOPATH //--vluzacn
//#define HLBSP_SUBDIVIDE_INMID // this may contribute to 'AllocBlock: full' problem though it may generate fewer faces. --vluzacn
	#ifdef HLBSP_ChoosePlane_VL
	#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY // possible that in a model there are surfaces but no splits
#define HLCSG_HLBSP_SOLIDHINT //--vluzacn
	#endif
	#endif
#define HLCSG_SORTSIDES //--vluzacn
	#ifdef HLCSG_CUSTOMHULL
	#ifdef HLCSG_FUNCGROUP_FIX
	#ifdef HLCSG_CSGBrush_BRUSHNUM_FIX
	#ifdef HLCSG_HLBSP_VOIDTEXINFO
	#ifdef ZHLT_WINDING_EPSILON
#define ZHLT_DETAILBRUSH //--vluzacn
	#endif
	#endif
	#endif
	#endif
	#endif
#define HLBSP_VIEWPORTAL //--vluzacn
#define HLBSP_EDGESHARE_SAMESIDE //--vluzacn
#define HLBSP_MarkLeakTrail_FIX //--vluzacn
	#ifdef HLCSG_NULLIFY_INVISIBLE
#define HLCSG_NULLIFYAAATRIGGER //--vluzacn
	#endif
	#ifdef HLCSG_CUSTOMHULL
	#ifdef HLCSG_CUSTOMCONTENT
#define HLCSG_PASSBULLETSBRUSH //--vluzacn
	#endif
	#endif
#define HLCSG_ONLYENTS_NOWADCHANGE //--vluzacn
#define HLCSG_NOFAKESPLITS //--vluzacn
	#ifdef ZHLT_LARGERANGE
#define HLBSP_MAXNODESIZE_SKYBOX //--vluzacn
	#endif
	#ifdef ZHLT_DETAILBRUSH
#define ZHLT_CLIPNODEDETAILLEVEL //--vluzacn
	#endif
#define HLBSP_HIDDENFACE //--vluzacn
	#ifdef ZHLT_WINDING_EPSILON
#define HLBSP_BRINKHACK //--vluzacn
	#endif
#define HLBSP_MERGECLIPNODE // Will this break the BSP file format? //--vluzacn
#define HLCSG_CLIPTYPEPRECISE_EPSILON_FIX //--vluzacn
	#ifdef HLBSP_BRINKHACK
#define HLBSP_BRINKNOTUSEDBYLEAF_FIX //--vluzacn
	#endif
	#ifdef HLBSP_ChoosePlane_VL
	#ifdef HLBSP_BALANCE
#define HLBSP_FAST_SELECTPARTITION //--vluzacn
	#endif
	#endif
	#ifdef ZHLT_DETAILBRUSH
#define HLBSP_DETAILBRUSH_CULL //--vluzacn
	#endif
	#ifdef ZHLT_DETAILBRUSH
#define HLBSP_SPLITFACE_FIX //--vluzacn
	#endif
	#ifdef HLCSG_ONLYENTS_NOWADCHANGE
#define HLCSG_AUTOWAD_TEXTURELIST_FIX //--vluzacn
	#endif
	#ifdef HLCSG_PRECISIONCLIP
	#ifdef HLCSG_CUSTOMHULL
	#ifdef HLCSG_AUTOWAD_TEXTURELIST_FIX
	#ifdef ZHLT_DELETEKEY
#define HLCSG_HULLBRUSH //--vluzacn
	#endif
	#endif
	#endif
	#endif
#define HLCSG_TEXMAP64_FIX //--vluzacn
	#ifdef HLBSP_FAST_SELECTPARTITION
#define HLBSP_CHOOSEMIDPLANE //--vluzacn
	#endif
	#ifdef HLBSP_BRINKHACK
#define HLBSP_BRINKHACK_BUGFIX //--vluzacn
	#endif
	#ifdef ZHLT_DETAILBRUSH
#define HLBSP_REMOVECOVEREDFACES //--vluzacn
	#endif
#define HLCSG_FILEREADFAILURE_FIX //--vluzacn
#define HLBSP_DELETETEMPFILES //--vluzacn
	#ifdef HLCSG_ONLYENTS_NOWADCHANGE
	#ifdef HLCSG_AUTOWAD
	#ifdef HLCSG_WADCFG_NEW
	#ifdef HLCSG_TEXMAP64_FIX
#define HLCSG_AUTOWAD_NEW //--vluzacn
	#endif
	#endif
	#endif
	#endif
	#ifdef HLCSG_HLBSP_VOIDTEXINFO
#define HLCSG_WARNBADTEXINFO //--vluzacn
	#endif
#define HLBSP_HASH_FIX //--vluzacn
	#ifdef ZHLT_DETAILBRUSH
#define HLCSG_COPLANARPRIORITY //--vluzacn
	#endif

#define HLVIS_MAXDIST
#define HLVIS_OVERVIEW //--vluzacn
	#ifdef HLVIS_MAXDIST
#define HLVIS_MAXDIST_NEW // GetShortestDistance used to crash randomly for no reason (compiled with VS2010), and I couldn't make it work even after fixing several obvious bugs. So replaced it with this. --vluzacn
	#endif
	#ifdef HLVIS_OVERVIEW
#define HLVIS_SKYBOXMODEL //--vluzacn
	#endif


#define HLRAD_INFO_TEXLIGHTS
#define HLRAD_WHOME // encompases all of Adam Foster's changes
#define HLRAD_HULLU // semi-opaque brush based entities and effects by hullu

#define HLRAD_TRANSNONORMALIZE //--vluzacn
#define HLRAD_OPAQUE_DIFFUSE_FIX //--vluzacn
	#ifdef HLRAD_TRANSNONORMALIZE
#define HLRAD_NOSWAP //--vluzacn
#define HLRAD_TRANSTOTAL_HACK //--vluzacn
	#endif
	#ifdef HLRAD_HULLU
#define HLRAD_TRANSPARENCY_CPP //--vluzacn
	#endif
	#ifdef HLRAD_TRANSPARENCY_CPP
#define HLRAD_TestSegmentAgainstOpaqueList_VL //--vluzacn
	#endif
#define HLRAD_ENTSTRIPRAD //--vluzacn
#define HLRAD_CHOP_FIX //--vluzacn
#define HLRAD_CUSTOMCHOP // don't use this --vluzacn
#define HLRAD_RGBTRANSFIX //--vluzacn
	#ifdef HLRAD_TRANSNONORMALIZE
	#ifdef HLRAD_RGBTRANSFIX
#define HLRAD_TRANSWEIRDFIX //--vluzacn
	#endif
	#endif
#define HLRAD_MDL_LIGHT_HACK //--vluzacn
#define HLRAD_MINLIGHT //--vluzacn
#define HLRAD_FinalLightFace_VL // Compensate for engine's bug of no gamma correction when adding dynamic light styles together. --vluzacn
	#ifdef HLRAD_TestSegmentAgainstOpaqueList_VL
#define HLRAD_POINT_IN_EDGE_FIX	//--vluzacn
	#endif
#define HLRAD_MULTISKYLIGHT //--vluzacn
#define HLRAD_ALLOWZEROBRIGHTNESS //--vluzacn
	#ifdef HLRAD_TestSegmentAgainstOpaqueList_VL
#define HLRAD_OPAQUE_GROUP //--vluzacn //obsolete
	#endif
	#ifdef HLRAD_OPAQUE_GROUP
#define HLRAD_OPAQUE_RANGE //--vluzacn //obsolete
	#endif
#define HLRAD_MATH_VL //--vluzacn
	#ifdef HLRAD_NOSWAP
	#ifdef HLRAD_TRANSWEIRDFIX
#define HLRAD_TRANSFERDATA_COMPRESS //--vluzacn
	#endif
	#endif
#define HLRAD_TRANCPARENCYLOSS_FIX //--vluzacn
#define HLRAD_STYLE_CORING //--vluzacn
	#ifdef HLRAD_TestSegmentAgainstOpaqueList_VL
	#ifdef HLRAD_STYLE_CORING
	#ifdef HLRAD_MULTISKYLIGHT
	#ifdef HLRAD_FinalLightFace_VL
#define HLRAD_OPAQUE_STYLE //--vluzacn
	#endif
	#endif
	#endif
	#endif
	#ifdef HLRAD_NOSWAP
#define HLRAD_CheckVisBitNoVismatrix_NOSWAP //--vluzacn
	#endif
	#ifdef HLRAD_OPAQUE_STYLE
	#ifdef HLRAD_CheckVisBitNoVismatrix_NOSWAP
#define HLRAD_OPAQUE_STYLE_BOUNCE //--vluzacn
	#endif
	#endif
#define HLRAD_GetPhongNormal_VL //--vluzacn
#define HLRAD_CUSTOMSMOOTH //--vluzacn
#define HLRAD_READABLE_EXCEEDSTYLEWARNING //--vluzacn
#define HLRAD_NUDGE_SMALLSTEP //--vluzacn
#define HLRAD_HLASSUMENOMEMORY //debug //--vluzacn
#define HLRAD_TestLine_EDGE_FIX //--vluzacn
#define HLRAD_STYLEREPORT //--vluzacn
#define HLRAD_SKYFIX_FIX //--vluzacn
#define HLRAD_NUDGE_VL //--vluzacn
#define HLRAD_WEIGHT_FIX //--vluzacn
#define HLRAD_PATCHBLACK_FIX //--vluzacn
#define HLRAD_HuntForWorld_EDGE_FIX // similar to HLRAD_TestLine_EDGE_FIX. --vluzacn
#define HLRAD_WITHOUTVIS //--vluzacn
	#ifdef HLRAD_NUDGE_VL
#define HLRAD_SNAPTOWINDING //--vluzacn
	#endif
#define HLRAD_HuntForWorld_FIX //--vluzacn
	#ifdef HLRAD_HuntForWorld_FIX
	#ifdef HLRAD_HuntForWorld_EDGE_FIX
	#ifdef HLRAD_GetPhongNormal_VL
	#ifdef HLRAD_SNAPTOWINDING
#define HLRAD_CalcPoints_NEW // --vluzacn
	#endif
	#endif
	#endif
	#endif
#define HLRAD_DPLANEOFFSET_MISCFIX //--vluzacn
#define HLRAD_NEGATIVEDIVIDEND_MISCFIX //--vluzacn
#define HLRAD_LERP_FIX //--vluzacn
	#ifdef ZHLT_TEXLIGHT
	#ifdef HLRAD_LERP_FIX
#define HLRAD_LERP_VL //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_LERP_VL
#define HLRAD_LERP_TRY5POINTS //--vluzacn
	#endif
#define HLRAD_DEBUG_DRAWPOINTS //--vluzacn
#define HLRAD_SubdividePatch_NOTMIDDLE //--vluzacn
	#ifdef HLRAD_CalcPoints_NEW
#define HLRAD_PHONG_FROMORIGINAL //--vluzacn
	#endif
	#ifdef HLRAD_GetPhongNormal_VL
#define HLRAD_SMOOTH_FACELIST //--vluzacn
	#endif
#define HLRAD_SortPatches_FIX // Important!! --vluzacn
	#ifdef HLRAD_MULTISKYLIGHT
#define HLRAD_GatherPatchLight //--vluzacn
	#endif
	#ifdef HLRAD_GatherPatchLight
#define HLRAD_SOFTSKY //--vluzacn
	#endif
#define HLRAD_OPAQUE_NODE //--vluzacn
	#ifdef HLRAD_CheckVisBitNoVismatrix_NOSWAP
#define HLRAD_TRANSLUCENT //--vluzacn
	#endif
	#ifdef HLRAD_OPAQUE_NODE
	#ifdef HLRAD_CalcPoints_NEW
#define HLRAD_OPAQUE_BLOCK //--vluzacn
	#endif
	#endif
#define HLRAD_EDGESHARE_NOSPECIAL //--vluzacn
	#ifdef HLRAD_SMOOTH_FACELIST
#define HLRAD_SMOOTH_TEXNORMAL //--vluzacn
	#endif
#define HLRAD_TEXTURE //--vluzacn
	#ifdef HLRAD_TEXTURE
#define HLRAD_REFLECTIVITY //--vluzacn
	#endif
#define HLRAD_VIS_FIX //--vluzacn
#define HLRAD_ENTITYBOUNCE_FIX //--vluzacn
	#ifdef HLRAD_TEXTURE
	#ifdef HLRAD_OPAQUE_NODE
#define HLRAD_OPAQUE_ALPHATEST //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_GatherPatchLight
	#ifdef ZHLT_TEXLIGHT
#define HLRAD_TEXLIGHTTHRESHOLD_FIX //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_REFLECTIVITY
	#ifdef HLRAD_TEXLIGHTTHRESHOLD_FIX
#define HLRAD_CUSTOMTEXLIGHT //--vluzacn
	#endif
	#endif
#define HLRAD_ARG_MISC //--vluzacn
#define HLRAD_PairEdges_FACESIDE_FIX //--vluzacn
	#ifdef HLRAD_ENTITYBOUNCE_FIX
#define HLRAD_VISMATRIX_NOMARKSURFACES //--vluzacn
	#endif
#define HLRAD_WATERBLOCKLIGHT //--vluzacn
	#ifdef HLRAD_MDL_LIGHT_HACK
#define HLRAD_MDL_LIGHT_HACK_NEW //--vluzacn
	#endif
	#ifdef HLRAD_LERP_VL
	#ifdef HLRAD_SMOOTH_FACELIST
#define HLRAD_LERP_FACELIST //--vluzacn
	#endif
	#endif
#define HLRAD_WATERBACKFACE_FIX // remove this if you have fixed the engine's bug of drawing water backface. --vluzacn
	#ifdef HLRAD_SMOOTH_TEXNORMAL
	#ifdef HLRAD_LERP_VL
	#ifdef HLRAD_CalcPoints_NEW
#define HLRAD_LERP_TEXNORMAL //--vluzacn
	#endif
	#endif
	#endif
#define HLRAD_REDUCELIGHTMAP //--vluzacn
	#ifdef HLRAD_STYLE_CORING
	#ifdef ZHLT_TEXLIGHT
	#ifdef HLRAD_REDUCELIGHTMAP
#define HLRAD_AUTOCORING //--vluzacn
	#endif
	#endif
	#endif
#define HLRAD_OPAQUEINSKY_FIX //--vluzacn
	#ifdef HLRAD_SOFTSKY
#define HLRAD_SUNSPREAD //--vluzacn
	#endif
	#ifdef HLRAD_MULTISKYLIGHT
	#ifdef HLRAD_WHOME
#define HLRAD_SUNDIFFUSE //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_GatherPatchLight
#define HLRAD_FASTMODE //--vluzacn
	#endif
#define HLRAD_OVERWRITEVERTEX_FIX //--vluzacn
	#ifdef HLRAD_CUSTOMTEXLIGHT
#define HLRAD_TEXLIGHT_SPOTS_FIX //--vluzacn
	#endif
	#ifdef HLRAD_OPAQUE_STYLE_BOUNCE
	#ifdef HLRAD_REFLECTIVITY
#define HLRAD_BOUNCE_STYLE //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_CalcPoints_NEW
#define HLRAD_BLUR //--vluzacn
	#endif
	#ifdef HLRAD_NOSWAP
	#ifdef HLRAD_TRANSWEIRDFIX
	#ifdef HLRAD_SOFTSKY
#define HLRAD_ACCURATEBOUNCE //--vluzacn
	#endif
	#endif
	#endif
	#ifdef HLRAD_TEXLIGHT_SPOTS_FIX
	#ifdef HLRAD_ACCURATEBOUNCE
#define HLRAD_ACCURATEBOUNCE_TEXLIGHT // note: this reduces the compile time in '-extra' mode //--vluzacn
	#endif
	#endif
	#ifdef ZHLT_TEXLIGHT
	#ifdef HLRAD_CalcPoints_NEW
	#ifdef HLRAD_AUTOCORING
#define HLRAD_ACCURATEBOUNCE_SAMPLELIGHT //--vluzacn
	#endif
	#endif
	#endif
	#ifdef HLRAD_ACCURATEBOUNCE_TEXLIGHT
#define HLRAD_ACCURATEBOUNCE_ALTERNATEORIGIN //--vluzacn
	#endif
	#ifdef HLRAD_PATCHBLACK_FIX
	#ifdef HLRAD_NOSWAP
#define HLRAD_ACCURATEBOUNCE_REDUCEAREA //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_CUSTOMTEXLIGHT
#define HLRAD_CUSTOMTEXLIGHT_COLOR //--vluzacn
	#endif
	#ifdef ZHLT_WINDING_EPSILON
#define HLRAD_SUBDIVIDEPATCH_NEW //--vluzacn
	#endif
	#ifdef HLRAD_NOSWAP
#define HLRAD_DIVERSE_LIGHTING //--vluzacn
	#endif
	#ifdef HLRAD_CalcPoints_NEW
	#ifdef HLRAD_BLUR
	#ifdef HLRAD_GetPhongNormal_VL
	#ifdef ZHLT_TEXLIGHT
	#ifdef HLRAD_SNAPTOWINDING
#define HLRAD_GROWSAMPLE //--vluzacn
	#endif
	#endif
	#endif
	#endif
	#endif
	#ifdef HLRAD_BLUR
#define HLRAD_AVOIDNORMALFLIP //--vluzacn
	#endif
	#ifdef HLRAD_BLUR
	#ifdef HLRAD_GROWSAMPLE
#define HLRAD_BLUR_MINIMALSQUARE //--vluzacn
	#endif
	#endif
	#ifdef HLRAD_BLUR_MINIMALSQUARE
#define HLRAD_AVOIDWALLBLEED //--vluzacn
	#endif
	#ifdef HLRAD_FinalLightFace_VL
#define HLRAD_PRESERVELIGHTMAPCOLOR //--vluzacn
	#endif
#define HLRAD_MORE_PATCHES //--vluzacn
	#ifdef HLRAD_VISMATRIX_NOMARKSURFACES
#define HLRAD_SPARSEVISMATRIX_FAST //--vluzacn
	#endif
	#ifdef HLRAD_LERP_VL
	#ifdef HLRAD_SMOOTH_FACELIST
	#ifdef HLRAD_GROWSAMPLE
	#ifdef HLRAD_DEBUG_DRAWPOINTS
#define HLRAD_LOCALTRIANGULATION //--vluzacn
	#endif
	#endif
	#endif
	#endif
	#ifdef HLRAD_LOCALTRIANGULATION
#define HLRAD_BILINEARINTERPOLATION //--vluzacn
	#endif
#define HLRAD_TEXLIGHTGAP //--vluzacn
	#ifdef HLRAD_LOCALTRIANGULATION
#define HLRAD_FARPATCH_FIX //--vluzacn
	#endif
#define HLRAD_TRANSPARENCY_FAST //--vluzacn

#if defined (ZHLT_XASH) || defined (ZHLT_XASH2)
#if !defined (ZHLT_TEXLIGHT) || !defined (HLRAD_LERP_VL) || !defined (HLRAD_AUTOCORING) || !defined (HLRAD_MULTISKYLIGHT) || !defined (HLRAD_FinalLightFace_VL) || !defined (HLRAD_AVOIDNORMALFLIP)
#error "ZHLT_XASH has not been implemented for current configuration"
#endif
#endif
//=====================================================================

#if _MSC_VER <1400
#define strcpy_s strcpy //--vluzacn
#define sprintf_s sprintf //--vluzacn
#endif
#if _MSC_VER >= 1400
#pragma warning(disable: 4996)
#endif

#ifdef __MINGW32__
#include <io.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 0 //--vluzacn
// AJM: gnu compiler fix
#ifdef __GNUC__
#define _alloca __builtin_alloca
#define alloca __builtin_alloca
#endif
#endif

#include "win32fix.h"
#include "mathtypes.h"

#ifdef SYSTEM_WIN32
#pragma warning(disable: 4127)                      // conditional expression is constant
#pragma warning(disable: 4115)                      // named type definition in parentheses
#pragma warning(disable: 4244)                      // conversion from 'type' to type', possible loss of data
// AJM
#pragma warning(disable: 4786)                      // identifier was truncated to '255' characters in the browser information
#pragma warning(disable: 4305)                      // truncation from 'const double' to 'float'
#pragma warning(disable: 4800)                     // forcing value to bool 'true' or 'false' (performance warning)
#endif


#ifdef STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <limits.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef ZHLT_NETVIS
#include "c2cpp.h"
#endif

#ifdef SYSTEM_WIN32
#define SYSTEM_SLASH_CHAR  '\\'
#define SYSTEM_SLASH_STR   "\\"
#endif
#ifdef SYSTEM_POSIX
#define SYSTEM_SLASH_CHAR  '/'
#define SYSTEM_SLASH_STR   "/"
#endif

// the dec offsetof macro doesn't work very well...
#define myoffsetof(type,identifier) ((size_t)&((type*)0)->identifier)
#define sizeofElement(type,identifier) (sizeof((type*)0)->identifier)

#ifdef SYSTEM_POSIX
extern char*    strupr(char* string);
extern char*    strlwr(char* string);
#endif
extern const char* stristr(const char* const string, const char* const substring);
extern bool CDECL FORMAT_PRINTF(3,4) safe_snprintf(char* const dest, const size_t count, const char* const args, ...);
extern bool     safe_strncpy(char* const dest, const char* const src, const size_t count);
extern bool     safe_strncat(char* const dest, const char* const src, const size_t count);
extern bool     TerminatedString(const char* buffer, const int size);

extern char*    FlipSlashes(char* string);

extern double   I_FloatTime();

extern int      CheckParm(char* check);

extern void     DefaultExtension(char* path, const char* extension);
extern void     DefaultPath(char* path, char* basepath);
extern void     StripFilename(char* path);
extern void     StripExtension(char* path);

extern void     ExtractFile(const char* const path, char* dest);
extern void     ExtractFilePath(const char* const path, char* dest);
extern void     ExtractFileBase(const char* const path, char* dest);
extern void     ExtractFileExtension(const char* const path, char* dest);

extern short    BigShort(short l);
extern short    LittleShort(short l);
extern int      BigLong(int l);
extern int      LittleLong(int l);
extern float    BigFloat(float l);
extern float    LittleFloat(float l);

#endif //CMDLIB_H__

