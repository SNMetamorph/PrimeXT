//
//                 MDL Viewer (c) 1999 by Mete Ciragan
//
// file:           md2viewer.h
// last modified:  Apr 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.4
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#ifndef INCLUDED_SPRVIEWER
#define INCLUDED_SPRVIEWER



#ifndef INCLUDED_MXWINDOW
#include <mx/mxWindow.h>
#endif

#define IDC_FILE_LOADMODEL			1001
#define IDC_FILE_SAVEMODEL			1002
#define IDC_FILE_LOADBACKGROUNDTEX		1003
#define IDC_FILE_LOADGROUNDTEX		1004
#define IDC_FILE_UNLOADGROUNDTEX		1005
#define IDC_FILE_OPENPAKFILE			1006
#define IDC_FILE_OPENPAKFILE2			1007
#define IDC_FILE_CLOSEPAKFILE			1008
#define IDC_FILE_RECENTMODELS1		1009
#define IDC_FILE_RECENTMODELS2		1010
#define IDC_FILE_RECENTMODELS3		1011
#define IDC_FILE_RECENTMODELS4		1012
#define IDC_FILE_RECENTPAKFILES1		1013
#define IDC_FILE_RECENTPAKFILES2		1014
#define IDC_FILE_RECENTPAKFILES3		1015
#define IDC_FILE_RECENTPAKFILES4		1016
#define IDC_FILE_EXIT			1017

#define IDC_OPTIONS_COLORBACKGROUND		1101
#define IDC_OPTIONS_COLORGROUND		1102
#define IDC_OPTIONS_COLORLIGHT		1103
#define IDC_OPTIONS_CENTERVIEW		1104
#define IDC_OPTIONS_RESETVIEW			1105
#define IDC_OPTIONS_MAKESCREENSHOT		1106
#define IDC_OPTIONS_WEAPONORIGIN		1107
#define IDC_OPTIONS_AUTOPLAY			1109
#define IDC_OPTIONS_DUMP			1110

#define IDC_VIEW_FILEASSOCIATIONS		1201

#define IDC_HELP_GOTOHOMEPAGE			1301
#define IDC_HELP_ABOUT			1302

// control panel
#define IDC_TAB				1901
#define IDC_RENDERMODE			2001
#define IDC_ORIENTATION			2002
#define IDC_GROUND				2003
#define IDC_BACKGROUND			2005

#define IDC_ANIMATION			5001
#define IDC_SPEEDSCALE			5002
#define IDC_STOP				5003
#define IDC_PREVFRAME			5004
#define IDC_FRAME				5005
#define IDC_NEXTFRAME			5006

class mxTab;
class mxMenuBar;
class mxButton;
class mxLineEdit;
class mxLabel;
class mxChoice;
class mxCheckBox;
class mxSlider;
class GlWindow;
class PAKViewer;

class SPRViewer : public mxWindow
{
	mxMenuBar *mb;
	mxTab *tab;
	GlWindow *d_GlWindow;
	mxChoice *cRenderMode;
	mxChoice *cOrientType;
	mxCheckBox *cbGround, *cbBackground;

	mxButton *tbStop;
	mxButton *bPrevFrame, *bNextFrame;
	mxLineEdit *leFrame;

	mxLabel *lSpriteInfo1;
	mxSlider *slSpeedScale;
	PAKViewer *pakViewer;

	void loadRecentFiles ();
	void saveRecentFiles ();
	void initRecentFiles ();

	void setSpriteInfo ();
public:
	friend PAKViewer;

	// CREATORS
	SPRViewer ();
	~SPRViewer ();

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	void checkboxSet( int id, bool bState );
	void redraw ();
	void makeScreenShot (const char *filename);
	void setRenderMode (int index);
	void setOrientType (int index);
	void setShowGround (bool b);
	void setShowBackground (bool b);
	int getTableIndex();
	bool loadSprite( const char *ptr, bool centering = true );
	void centerSprite ();

	// ACCESSORS
	mxMenuBar *getMenuBar () const { return mb; }
};



extern SPRViewer *g_SPRViewer;
extern char g_appTitle[];


#endif // INCLUDED_SPRVIEWER