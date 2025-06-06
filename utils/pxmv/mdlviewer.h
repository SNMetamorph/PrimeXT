//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           mdlviewer.h
// last modified:  Apr 28 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#ifndef INCLUDED_MDLVIEWER
#define INCLUDED_MDLVIEWER

#include "ViewerSettings.h"

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
#define IDC_OPTIONS_LEFTHAND			1108
#define IDC_OPTIONS_AUTOPLAY			1109
#define IDC_OPTIONS_BLENDWEIGHTS		1110
#define IDC_OPTIONS_DUMP			1111

#define IDC_VIEW_FILEASSOCIATIONS		1201

#define IDC_HELP_GOTOHOMEPAGE			1301
#define IDC_HELP_ABOUT			1302


class mxMenuBar;
class GlWindow;
class ControlPanel;
class PAKViewer;
class ViewerSettings;
class StudioModel;

class MDLViewer : public mxWindow
{
	mxMenuBar *mb;
	GlWindow *d_GlWindow;
	ControlPanel *d_cpl;
	PAKViewer *d_PAKViewer;
	ViewerSettings m_settings;
	StudioModel *m_studioModel;

	void initRecentFiles ();

public:
	// CREATORS
	MDLViewer ();
	~MDLViewer ();

	// MANIPULATORS
	virtual int handleEvent(mxEvent *event);
	void checkboxSet( int id, bool bState );
	void redraw();
	bool shouldMaximizeWindow();

	// ACCESSORS
	mxMenuBar *getMenuBar () const { return mb; }
	GlWindow *getGlWindow () const { return d_GlWindow; }
	PAKViewer *getPAKViewer () const { return d_PAKViewer; }
};

extern MDLViewer *g_MDLViewer;

#endif // INCLUDED_MDLVIEWER