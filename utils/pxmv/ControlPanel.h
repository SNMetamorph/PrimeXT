//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ControlPanel.h
// last modified:  Oct 20 programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.24
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#ifndef INCLUDED_CONTROLPANEL
#define INCLUDED_CONTROLPANEL



#ifndef INCLUDED_MXWINDOW
#include <mxWindow.h>
#endif

#define TAB_MODELDISPLAY				0	// render options, change rendermode, show info
#define TAB_BODYPARTS				1	// bodyparts, skins, etc
#define TAB_TEXTURES				2	// texture browser
#define TAB_SEQUENCES				3	// sequence browser
#define TAB_MISC					4	// model flags, remapping
#define TAB_CONTROLS				5	// global bone controllers, pose parameters
#define TAB_MODELEDITOR				6	// built-in model editor

#define IDC_TAB					1901
#define IDC_RENDERMODE				2001
#define IDC_TRANSPARENCY				2002
#define IDC_GROUND					2003
#define IDC_MIRROR					2004
#define IDC_BACKGROUND				2005
#define IDC_HITBOXES				2006
#define IDC_BONES					2007
#define IDC_ATTACHMENTS				2008
#define IDC_NORMALS					2009
#define IDC_WIREFRAME				2010

#define MAX_BONECTRLS				5	// 4 user-controllers + mouth
#define IDC_BODYPART				3001
#define IDC_SUBMODEL				3002
#define IDC_CONTROLLER				3003
#define IDC_CONTROLLERVALUE				3004
#define IDC_SKINS					3005

#define IDC_TEXTURES				4001
#define IDC_EXPORTTEXTURE				4002
#define IDC_IMPORTTEXTURE				4003
#define IDC_EXPORT_UVMAP				4004
#define IDC_TEXTURESCALE				4005
#define IDC_CHROME					4006
#define IDC_ADDITIVE				4007
#define IDC_TRANSPARENT				4008
#define IDC_FULLBRIGHT				4009
#define IDC_FLATSHADE				4010
#define IDC_TWO_SIDED				4011
#define IDC_SMOOTH_TBN				4012
#define IDC_SOLID_ALPHA				4013
#define IDC_EDIT_TEXTURE_NAME				4014
#define IDC_SET_TEXTURE_NAME				4015

#define IDC_SHOW_UV_MAP				4020
#define IDC_OVERLAY_UV_MAP				4021
#define IDC_ANTI_ALIAS_LINES				4022

#define IDC_SEQUENCE				5001
#define IDC_SPEEDSCALE				5002
#define IDC_STOP					5003
#define IDC_PREVFRAME				5004
#define IDC_FRAME					5005
#define IDC_NEXTFRAME				5006
#define IDC_LOOPANIM				5007
#define IDC_BLENDER0				5008
#define IDC_BLENDER1				5009
#define IDC_COPYSEQUENCENAME		5010

#define NUM_POSEPARAMETERS				4
#define IDC_POSEPARAMETER_SCALE			5100
#define IDC_POSEPARAMETER				5120
#define IDC_POSEPARAMETER_VALUE			5140
#define IDC_ENABLE_IK				5200

#define IDC_MISC					6001
#define IDC_STUDIO_ROCKET				6002	// leave a trail
#define IDC_STUDIO_GRENADE				6003	// leave a trail
#define IDC_STUDIO_GIB				6004	// leave a trail
#define IDC_STUDIO_ROTATE				6005	// rotate (bonus items)
#define IDC_STUDIO_TRACER				6006	// green split trail
#define IDC_STUDIO_ZOMGIB				6007	// small blood trail
#define IDC_STUDIO_TRACER2				6008	// orange split trail + rotate
#define IDC_STUDIO_TRACER3				6009	// purple trail
#define IDC_STUDIO_AMBIENT_LIGHT			6010	// dynamically get lighting from floor or ceil (flying monsters)
#define IDC_STUDIO_TRACE_HITBOX			6011	// always use hitbox trace instead of bbox
#define IDC_STUDIO_FORCE_SKYLIGHT			6012	// always grab lightvalues from the sky settings (even if sky is invisible)
#define IDC_TOPCOLOR				6013
#define IDC_BOTTOMCOLOR				6014

#define IDC_EDITOR					7001
#define IDC_MOVE_PX					7002
#define IDC_MOVE_NX					7003
#define IDC_MOVE_PY					7004
#define IDC_MOVE_NY					7005
#define IDC_MOVE_PZ					7006
#define IDC_MOVE_NZ					7007
#define IDC_EDIT_TYPE				7008
#define IDC_EDIT_MODE				7009
#define IDC_EDIT_STEP				7010
#define IDC_EDIT_SIZE				7011
#define IDC_CONVERT_TEXCOORDS				7012

class mxTab;
class mxChoice;
class mxCheckBox;
class mxSlider;
class mxLineEdit;
class mxLabel;
class mxButton;
class mxToggleButton;
class GlWindow;
class TextureWindow;



class ControlPanel : public mxWindow
{
	mxTab *tab;
	mxChoice *cRenderMode;
	mxLabel *lOpacityValue;
	mxSlider *slTransparency;
	mxCheckBox *cbAttachments;
	mxCheckBox *cbGround, *cbMirror, *cbBackground;
	mxLabel *lDrawnPolys;
	mxChoice *cSequence;
	mxSlider *slSpeedScale;
	mxButton *tbStop;
	mxButton *bPrevFrame, *bNextFrame;
	mxButton *bCopySequenceName;
	mxLabel *lSequenceInfo;
	mxSlider *slBlender0;
	mxSlider *slBlender1;
	mxCheckBox *cbLoopAnim;
	mxLineEdit *leFrame;
	mxChoice *cBodypart, *cController, *cSubmodel;
	mxLabel *BodyPartLabel;
	mxSlider *slController;
	mxChoice *cSkin;
	mxLabel *lDrawnPolys2;
	mxLabel *lModelInfo1, *lModelInfo2;
	mxChoice *cTextures;
	mxCheckBox *cbChrome;
	mxCheckBox *cbAdditive;
	mxCheckBox *cbTransparent;
	mxCheckBox *cbFullbright;
	mxCheckBox *cbFlatshade;
	mxCheckBox *cbTwoSided;
	mxCheckBox *cbSolidAlpha;
	mxCheckBox *cbSmooth;
	mxCheckBox *cbShowUVMap;
	mxCheckBox *cbOverlayUVMap;
	mxCheckBox *cbAntiAliasLines;
	mxLineEdit *leTextureName;
	mxLabel *lTexSize;
	mxLabel *lTexScale;
	mxSlider *slTexScale;
	mxLineEdit *leWidth, *leHeight;
	mxLineEdit *leMeshScale, *leBoneScale;
	mxCheckBox *cbFlagRocket;
	mxCheckBox *cbFlagGrenade;
	mxCheckBox *cbFlagGib;
	mxCheckBox *cbFlagRotate;
	mxCheckBox *cbFlagTracer;
	mxCheckBox *cbFlagZomgib;
	mxCheckBox *cbFlagTracer2;
	mxCheckBox *cbFlagTracer3;
	mxCheckBox *cbFlagAmbientLight;
	mxCheckBox *cbFlagTraceHitbox;
	mxCheckBox *cbFlagForceSkylight;
	mxSlider *slTopColor;
	mxSlider *slBottomColor;
	mxChoice *cPoseParameter[NUM_POSEPARAMETERS];
	mxSlider *slPoseParameter[NUM_POSEPARAMETERS];
	mxLineEdit *lePoseParameter[NUM_POSEPARAMETERS];
	mxCheckBox *cbEnableIK;
	mxChoice *cEditMode;
	mxChoice *cEditType;
	mxLineEdit *leEditStep;
	mxLineEdit *leEditString;
	mxCheckBox *cbEditSize;

	mxButton *tbMovePosX;
	mxButton *tbMoveNegX;
	mxButton *tbMovePosY;
	mxButton *tbMoveNegY;
	mxButton *tbMovePosZ;
	mxButton *tbMoveNegZ;
	mxButton *tbConvertTexCoords;

	GlWindow *d_GlWindow;
	TextureWindow *d_textureWindow;

	// sequence remapping
	int	*iSelectionToSequence; // selection to sequence
	int	*iSequenceToSelection; // sequence to selection
	float	controllervalues[MAX_BONECTRLS];	
	int	controllerindex[MAX_BONECTRLS];
public:
	// CREATORS
	ControlPanel (mxWindow *parent);
	virtual ~ControlPanel ();

	// MANIPULATORS
	int handleEvent (mxEvent *event);

	void dumpModelInfo ();
	int loadModel (const char *filename, bool centerView = true);

	void setRenderMode (int mode);
	void setShowGround (bool b);
	void setMirror (bool b);
	void setShowBackground (bool b);

	void initSequences ();
	void setSequence (int index);

	void initBodyparts ();
	void setBodypart (int index);
	int setSubmodel (int index);

	void initBoneControllers ();
	void setBoneController (int index);
	void setBoneControllerValue (int index, float value);
	void setBlend(int index, float value );

	void initEditModes ();
	void addEditType ( const char *name, int type, int id = -1 );
	void toggleMoveSize ( bool size );

	void initSkins ();

	void setModelInfo ();

	void setSequenceInfo ();

	void setModelFlags ();

	void initTextures ();

	void updatePoseParameters( void );

	void centerView ( bool reset );

	void setGlWindow (GlWindow *window) { d_GlWindow = window; }
	void resetPlayingSequence( void );
	void setPlaySequence( void );

	void updateDrawnPolys (void);

	float getEditStep( void );

	// no ACCESSORS

	int getTableIndex();
};



extern ControlPanel *g_ControlPanel;



#endif // INCLUDED_CONTROLPANEL