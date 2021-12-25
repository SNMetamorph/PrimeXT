//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           mdlviewer.cpp
// last modified:  Jun 03 1999, Mete Ciragan
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mx.h>
#include <gl.h>
#include <mxTga.h>
#include "mdlviewer.h"
#include "GlWindow.h"
#include "ControlPanel.h"
#include "StudioModel.h"
#include "pakviewer.h"
#include "FileAssociation.h"
#include "stringlib.h"

MDLViewer *g_MDLViewer = 0;
char g_appTitle[] = "Paranoia 2 Model Viewer v1.31 stable";
static char recentFiles[8][256] = { "", "", "", "", "", "", "", "" };
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;

void MDLViewer::initRecentFiles( void )
{
	for (int i = 0; i < 8; i++)
	{
		if (strlen (recentFiles[i]))
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, recentFiles[i]);
		}
		else
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, "(empty)");
			mb->setEnabled (IDC_FILE_RECENTMODELS1 + i, false);
		}
	}
}

void MDLViewer::loadRecentFiles( void )
{
	char	str[256];

	for( int i = 0; i < 8; i++ )
	{
		mx_snprintf( str, sizeof( str ), "RecentFile%i", i );
		if( !LoadString( str, recentFiles[i] ))
			break;
	}
}

void MDLViewer::saveRecentFiles( void )
{
	char	str[256];

	if( !InitRegistry( ))
		return;

	for( int i = 0; i < 8; i++ )
	{
		mx_snprintf( str, sizeof( str ), "RecentFile%i", i );
		if( !SaveString( str, recentFiles[i] ))
			break;
	}
}

MDLViewer :: MDLViewer() : mxWindow( 0, 0, 0, 0, 0, g_appTitle, mxWindow::Normal )
{
	// create menu stuff
	mb = new mxMenuBar (this);
	mxMenu *menuFile = new mxMenu ();
	mxMenu *menuOptions = new mxMenu ();
	mxMenu *menuView = new mxMenu ();
	mxMenu *menuHelp = new mxMenu ();

	mb->addMenu ("File", menuFile);
	mb->addMenu ("Options", menuOptions);
	mb->addMenu ("Tools", menuView);
	mb->addMenu ("Help", menuHelp);

	mxMenu *menuRecentModels = new mxMenu ();
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS1);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS2);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS3);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS4);

	mxMenu *menuRecentPakFiles = new mxMenu ();
	menuRecentPakFiles->add ("(empty)", IDC_FILE_RECENTPAKFILES1);
	menuRecentPakFiles->add ("(empty)", IDC_FILE_RECENTPAKFILES2);
	menuRecentPakFiles->add ("(empty)", IDC_FILE_RECENTPAKFILES3);
	menuRecentPakFiles->add ("(empty)", IDC_FILE_RECENTPAKFILES4);

	menuFile->add ("Load Model...", IDC_FILE_LOADMODEL);
	menuFile->add ("Save Model...", IDC_FILE_SAVEMODEL);
	menuFile->addSeparator ();
	menuFile->add ("Load Background Texture...", IDC_FILE_LOADBACKGROUNDTEX);
	menuFile->add ("Load Ground Texture...", IDC_FILE_LOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Unload Ground Texture", IDC_FILE_UNLOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Open PAK file...", IDC_FILE_OPENPAKFILE);
	menuFile->add ("Close PAK file", IDC_FILE_CLOSEPAKFILE);
	menuFile->addSeparator ();
	menuFile->addMenu ("Recent Models", menuRecentModels);
	menuFile->addMenu ("Recent PAK files", menuRecentPakFiles);
	menuFile->addSeparator ();
	menuFile->add ("Exit", IDC_FILE_EXIT);

	menuOptions->add ("Background Color...", IDC_OPTIONS_COLORBACKGROUND);
	menuOptions->add ("Ground Color...", IDC_OPTIONS_COLORGROUND);
	menuOptions->add ("Light Color...", IDC_OPTIONS_COLORLIGHT);
	menuOptions->addSeparator ();
	menuOptions->add( "Sequence AutoPlay", IDC_OPTIONS_AUTOPLAY );
	menuOptions->add( "Using BoneWeights", IDC_OPTIONS_BLENDWEIGHTS );
	menuOptions->addSeparator ();
	menuOptions->add( "Use weapon origin", IDC_OPTIONS_WEAPONORIGIN );
	menuOptions->add( "Weapon left-handed", IDC_OPTIONS_LEFTHAND );
	menuOptions->addSeparator ();
	menuOptions->add ("Center View", IDC_OPTIONS_CENTERVIEW);
	menuOptions->add ("Reset View", IDC_OPTIONS_RESETVIEW);
#ifdef WIN32
	menuOptions->addSeparator ();
	menuOptions->add ("Make Screenshot...", IDC_OPTIONS_MAKESCREENSHOT);
	//menuOptions->add ("Dump Model Info", IDC_OPTIONS_DUMP);
#endif

	menuView->add ("File Associations...", IDC_VIEW_FILEASSOCIATIONS);

#ifdef WIN32
	menuHelp->add ("Goto Homepage...", IDC_HELP_GOTOHOMEPAGE);
	menuHelp->addSeparator ();
#endif
	menuHelp->add ("About...", IDC_HELP_ABOUT);

	mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
	mb->setChecked( IDC_OPTIONS_LEFTHAND, bUseWeaponLeftHand );
	mb->setChecked( IDC_OPTIONS_AUTOPLAY, g_viewerSettings.sequence_autoplay ? true : false );
	mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, g_viewerSettings.studio_blendweights ? true : false );

	// create the OpenGL window
	d_GlWindow = new GlWindow (this, 0, 0, 0, 0, "", mxWindow::Normal);
#ifdef WIN32
	SetWindowLong ((HWND) d_GlWindow->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif

	d_cpl = new ControlPanel (this);
	d_cpl->setGlWindow (d_GlWindow);
	d_GlWindow->setControlPanel(d_cpl);
	g_GlWindow = d_GlWindow;

	// finally create the pakviewer window
	d_PAKViewer = new PAKViewer (this);
	g_FileAssociation = new FileAssociation ();

	loadRecentFiles ();
	initRecentFiles ();

	setBounds (20, 20, 640, 540);
	setVisible (true);

	if( g_viewerSettings.showGround )
		d_cpl->setShowGround (true);

	if( g_viewerSettings.groundTexFile[0] )
		d_GlWindow->loadTexture( g_viewerSettings.groundTexFile, TEXTURE_GROUND );
	else d_GlWindow->loadTexture( NULL, TEXTURE_GROUND );
}



MDLViewer::~MDLViewer ()
{
	// grab some params in case that hasn't updates
	if( d_cpl )
		g_viewerSettings.editStep = d_cpl->getEditStep();
	g_viewerSettings.showMaximized = isMaximized();

	saveRecentFiles ();
	SaveViewerSettings ();
	g_studioModel.FreeModel ();
#ifdef WIN32
	DeleteFile ("midump.txt");
#endif
}

void MDLViewer :: checkboxSet( int id, bool bState )
{
	mb->setChecked( id, bState );
}

int
MDLViewer::handleEvent (mxEvent *event)
{
	switch (event->event)
	{
	case mxEvent::Action:
	{
		switch (event->action)
		{
		case IDC_FILE_LOADMODEL:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "*.mdl");
			if (ptr)
			{
				int i;
				d_cpl->loadModel (ptr);

				for (i = 0; i < 4; i++)
				{
					if (!mx_strcasecmp (recentFiles[i], ptr))
						break;
				}

				// swap existing recent file
				if (i < 4)
				{
					char tmp[256];
					strcpy (tmp, recentFiles[0]);
					strcpy (recentFiles[0], recentFiles[i]);
					strcpy (recentFiles[i], tmp);
				}

				// insert recent file
				else
				{
					for (i = 3; i > 0; i--)
						strcpy (recentFiles[i], recentFiles[i - 1]);

					strcpy (recentFiles[0], ptr);
				}

				initRecentFiles ();
			}
		}
		break;

		case IDC_FILE_SAVEMODEL:
		{
			char *ptr = (char *) mxGetSaveFileName (this, g_viewerSettings.modelPath, "*.mdl", g_viewerSettings.modelPath);
			if (!ptr)
				break;

			char filename[256];
			char ext[16];

			strcpy( filename, ptr );
			strcpy( ext, mx_getextension( filename ));
			if( mx_strcasecmp( ext, ".mdl" ))
				strcat( filename, ".mdl" );

			if( !g_studioModel.SaveModel( filename ))
			{
				mxMessageBox( this, "Error saving model.", g_appTitle, MX_MB_OK | MX_MB_ERROR );
			}
			else
			{
				strcpy( g_viewerSettings.modelFile, filename );
				g_viewerSettings.numModelChanges = 0;	// all the settings are handled
			}
		}
		break;

		case IDC_FILE_LOADBACKGROUNDTEX:
		case IDC_FILE_LOADGROUNDTEX:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "*.bmp;*.tga;*.pcx");
			if (ptr)
			{
				int name = TEXTURE_UNUSED;

				if( event->action == IDC_FILE_LOADBACKGROUNDTEX )
					name = TEXTURE_BACKGROUND;
				else if( event->action == IDC_FILE_LOADGROUNDTEX )					
					name = TEXTURE_GROUND;

				if (d_GlWindow->loadTexture( ptr, name ))
				{
					if (event->action == IDC_FILE_LOADBACKGROUNDTEX)
						d_cpl->setShowBackground (true);
					else
						d_cpl->setShowGround (true);

				}
				else
					mxMessageBox (this, "Error loading texture.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
			}
		}
		break;

		case IDC_FILE_UNLOADGROUNDTEX:
		{
			d_GlWindow->loadTexture( NULL, TEXTURE_GROUND );
			d_cpl->setShowGround (false);
		}
		break;

		case IDC_FILE_OPENPAKFILE:
		{
			const char *ptr = mxGetOpenFileName (this, "\\sierra\\half-life\\valve", "*.pak");
			if (ptr)
			{
				int i;

				d_PAKViewer->openPAKFile (ptr);

				for (i = 4; i < 8; i++)
				{
					if (!mx_strcasecmp (recentFiles[i], ptr))
						break;
				}

				// swap existing recent file
				if (i < 8)
				{
					char tmp[256];
					strcpy (tmp, recentFiles[4]);
					strcpy (recentFiles[4], recentFiles[i]);
					strcpy (recentFiles[i], tmp);
				}

				// insert recent file
				else
				{
					for (i = 7; i > 4; i--)
						strcpy (recentFiles[i], recentFiles[i - 1]);

					strcpy (recentFiles[4], ptr);
				}

				initRecentFiles ();

				redraw ();
			}
		}
		break;

		case IDC_FILE_CLOSEPAKFILE:
		{
			d_PAKViewer->closePAKFile ();
			redraw ();
		}
		break;

		case IDC_FILE_RECENTMODELS1:
		case IDC_FILE_RECENTMODELS2:
		case IDC_FILE_RECENTMODELS3:
		case IDC_FILE_RECENTMODELS4:
		{
			int i = event->action - IDC_FILE_RECENTMODELS1;
			d_cpl->loadModel (recentFiles[i]);

			char tmp[256];			
			strcpy (tmp, recentFiles[0]);
			strcpy (recentFiles[0], recentFiles[i]);
			strcpy (recentFiles[i], tmp);

			initRecentFiles ();

			redraw ();
		}
		break;

		case IDC_FILE_RECENTPAKFILES1:
		case IDC_FILE_RECENTPAKFILES2:
		case IDC_FILE_RECENTPAKFILES3:
		case IDC_FILE_RECENTPAKFILES4:
		{
			int i = event->action - IDC_FILE_RECENTPAKFILES1 + 4;
			d_PAKViewer->openPAKFile (recentFiles[i]);

			char tmp[256];			
			strcpy (tmp, recentFiles[4]);
			strcpy (recentFiles[4], recentFiles[i]);
			strcpy (recentFiles[i], tmp);

			initRecentFiles ();

			redraw ();
		}
		break;

		case IDC_FILE_EXIT:
		{
			d_PAKViewer->closePAKFile ();
			redraw ();
			mx::quit ();
		}
		break;

		case IDC_OPTIONS_COLORBACKGROUND:
		case IDC_OPTIONS_COLORGROUND:
		case IDC_OPTIONS_COLORLIGHT:
		{
			float *cols[3] = { g_viewerSettings.bgColor, g_viewerSettings.gColor, g_viewerSettings.lColor };
			float *col = cols[event->action - IDC_OPTIONS_COLORBACKGROUND];
			int r = (int) (col[0] * 255.0f);
			int g = (int) (col[1] * 255.0f);
			int b = (int) (col[2] * 255.0f);
			if (mxChooseColor (this, &r, &g, &b))
			{
				col[0] = (float) r / 255.0f;
				col[1] = (float) g / 255.0f;
				col[2] = (float) b / 255.0f;
			}
		}
		break;

		case IDC_OPTIONS_CENTERVIEW:
			d_cpl->centerView (false);
			break;

		case IDC_OPTIONS_RESETVIEW:
			d_cpl->centerView (true);
			break;

		case IDC_OPTIONS_MAKESCREENSHOT:
		{
			char *ptr = (char *)mxGetSaveFileName( this, "", "*.bmp" );
			if( ptr )
			{
				if( !strstr( ptr, ".bmp" ))
					strcat( ptr, ".bmp" );
				d_GlWindow->dumpViewport( ptr );
			}
		}
		break;

		case IDC_OPTIONS_WEAPONORIGIN:
			bUseWeaponOrigin = !mb->isChecked( IDC_OPTIONS_WEAPONORIGIN );
			mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
			break;

		case IDC_OPTIONS_LEFTHAND:
			bUseWeaponLeftHand = !mb->isChecked( IDC_OPTIONS_LEFTHAND );
			mb->setChecked( IDC_OPTIONS_LEFTHAND, bUseWeaponLeftHand );
			break;

		case IDC_OPTIONS_AUTOPLAY:
			g_viewerSettings.sequence_autoplay = !mb->isChecked( IDC_OPTIONS_AUTOPLAY );
			mb->setChecked( IDC_OPTIONS_AUTOPLAY, g_viewerSettings.sequence_autoplay ? true : false );
			break;

		case IDC_OPTIONS_BLENDWEIGHTS:
			g_viewerSettings.studio_blendweights = !mb->isChecked( IDC_OPTIONS_BLENDWEIGHTS );
			mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, g_viewerSettings.studio_blendweights ? true : false );
			break;

		case IDC_OPTIONS_DUMP:
			d_cpl->dumpModelInfo ();
			break;

		case IDC_VIEW_FILEASSOCIATIONS:
			g_FileAssociation->setAssociation (0);
			g_FileAssociation->setVisible (true);
			break;

#ifdef WIN32
		case IDC_HELP_GOTOHOMEPAGE:
			ShellExecute (0, "open", "http://cs-mapping.com.ua/forum/forumdisplay.php?f=189", 0, 0, SW_SHOW);
			break;
#endif

		case IDC_HELP_ABOUT:
			mxMessageBox (this,
				"Paranoia 2 Model Viewer v1.31 (c) 2019 by Unkle Mike\n"
				"Based on original HLMV code by Mete Ciragan\n\n"
				"Left-drag to rotate.\n"
				"Right-drag to zoom.\n"
				"Shift-left-drag to x-y-pan.\n"
				"Ctrl-drag to move lights.\n\n"
				"Build:\t" __DATE__ ".\n"
				"Email:\tg-cont@rambler.ru\n"
				"Web:\thttp://www.hlfx.ru/forum", "About Paranoia 2 Model Viewer",
				MX_MB_OK | MX_MB_INFORMATION );
			break;
		
		} //switch (event->action)

	} // mxEvent::Action
	break;

	case mxEvent::Size:
	{
		int w = event->width;
		int h = event->height;
		int y = mb->getHeight ();
#ifdef WIN32
#define HEIGHT 120
#else
#define HEIGHT 140
		h -= 40;
#endif

		if (d_PAKViewer->isVisible ())
		{
			w -= 170;
			d_PAKViewer->setBounds (w, y, 170, h);
		}

		d_GlWindow->setBounds (0, y, w, h - HEIGHT);
		d_cpl->setBounds (0, y + h - HEIGHT, w, HEIGHT);
	}
	break;
	case mxEvent::KeyDown:
	{
		switch( (char)event->key )
		{
		case 32:
		{
			static float lasttime = 0.0f;

			if( lasttime > g_studioModel.getCurrentTime( ))
				break;
			lasttime = g_studioModel.getCurrentTime() + 0.1f;

			int iSeq = g_studioModel.GetSequence();

			if( iSeq == g_studioModel.SetSequence( iSeq + 1 ))
				g_studioModel.SetSequence( 0 );
		}
		break;

		case 27:
			if( !getParent( )) // fullscreen mode ?
				mx::quit();
			break;
		case 37:
			if( g_viewerSettings.numModelPathes > 0 )
				d_cpl->loadModel( LoadPrevModel( ));
			break;
		case 39:
			if( g_viewerSettings.numModelPathes > 0 )
				d_cpl->loadModel( LoadNextModel( ));
			break;
		case VK_F5:
		{
			bool oldUseWeaponOrigin = bUseWeaponOrigin;
			d_cpl->loadModel( g_viewerSettings.modelFile, false );
			bUseWeaponOrigin = oldUseWeaponOrigin;
			break;
		}
		case 'g':
		case 'ï':
			g_viewerSettings.showGround = !g_viewerSettings.showGround;
			if( !g_viewerSettings.showGround )
				g_viewerSettings.mirror = false;
			break;
		case 'h':
		case 'ð':
			g_viewerSettings.showHitBoxes = !g_viewerSettings.showHitBoxes;
			break;
		case 'o':
		case 'ù':
			g_viewerSettings.showBones = !g_viewerSettings.showBones;
			break;
		case '5':
			g_viewerSettings.transparency -= 0.05f;
			if( g_viewerSettings.transparency < 0.0f )
				g_viewerSettings.transparency = 0.0f;
			break;
		case '6':
			g_viewerSettings.transparency += 0.05f;
			if( g_viewerSettings.transparency > 1.0f )
				g_viewerSettings.transparency = 1.0f;
			break;
		case 'b':
		case 'è':
			g_viewerSettings.showBackground = !g_viewerSettings.showBackground;
			break;
		case 's':
		case 'û':
			g_viewerSettings.useStencil = !g_viewerSettings.useStencil;
			break;
		case 'm':
		case 'ü':
			g_viewerSettings.mirror = !g_viewerSettings.mirror;
			if( g_viewerSettings.mirror )
				g_viewerSettings.showGround = true;
			break;
		case 'v':
		case 'ì':
			bUseWeaponOrigin = !mb->isChecked( IDC_OPTIONS_WEAPONORIGIN );
			mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
			break;
		case 'l':
		case 'ä':
			bUseWeaponLeftHand = !mb->isChecked( IDC_OPTIONS_LEFTHAND );
			mb->setChecked( IDC_OPTIONS_LEFTHAND, bUseWeaponLeftHand );
			break;

		case 'w':
		case 'ö':
			g_viewerSettings.studio_blendweights = !mb->isChecked( IDC_OPTIONS_BLENDWEIGHTS );
			mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, g_viewerSettings.studio_blendweights ? true : false );
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			g_viewerSettings.renderMode = event->key - '1';
			break;
		case '-':
			g_viewerSettings.speedScale -= 0.1f;
			if( g_viewerSettings.speedScale < 0.0f )
				g_viewerSettings.speedScale = 0.0f;
			break;
		case '+':
			g_viewerSettings.speedScale += 0.1f;
			if( g_viewerSettings.speedScale > 5.0f )
				g_viewerSettings.speedScale = 5.0f;
			break;
		}
	}
	break;
	} // event->event

	return 1;
}



void
MDLViewer::redraw ()
{
	mxEvent event;
	event.event = mxEvent::Size;
	event.width = w2 ();
	event.height = h2 ();
	handleEvent (&event);
}



int main( int argc, char *argv[] )
{
	//
	// make sure, we start in the right directory
	//
	mx_setcwd (mx::getApplicationPath ());
	atexit( Sys_CloseLog );

	char cmdline[1024] = "";
	if (argc > 1)
	{
		strcpy (cmdline, argv[1]);
		for (int i = 2; i < argc; i++)
		{
			strcat (cmdline, " ");
			strcat (cmdline, argv[i]);
		}
	}

	if( IsAliasModel( cmdline ) && COM_FileExists( "q1mv.exe" ))
	{
		strcpy (cmdline, "q1mv.exe" );
		for (int i = 1; i < argc; i++)
		{
			strcat (cmdline, " ");
			strcat (cmdline, argv[i]);
		}
		WinExec (cmdline, SW_SHOW);
		return 0;
	}

	LoadViewerSettings();

	//mx::setDisplayMode (0, 0, 0);
	mx::init (argc, argv);
	g_MDLViewer = new MDLViewer ();
	g_MDLViewer->setMenuBar (g_MDLViewer->getMenuBar ());
	g_MDLViewer->setBounds (20, 20, 640, 540);
	g_MDLViewer->setVisible (true);

	if( g_viewerSettings.showMaximized )
		g_MDLViewer->Maximize();

	if (Q_stristr (cmdline, ".mdl"))
	{
		g_ControlPanel->loadModel (cmdline);
	}

	int ret = mx::run ();

	mx::cleanup ();

	return ret;
}
