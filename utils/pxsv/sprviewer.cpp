//
//                 MDL Viewer (c) 1999 by Mete Ciragan
//
// file:           md2viewer.cpp
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
#include <stdio.h>
#include <stdlib.h>
//#include <ostream.h>
#include <mx.h>
#include <gl.h>
#include <mxBmp.h>
#include "sprviewer.h"
#include "GlWindow.h"
#include "pakviewer.h"
#include "FileAssociation.h"
#include "app_info.h"
#include "stringlib.h"
#include "SpriteModel.h"
#include "build.h"
#include "build_info.h"

SPRViewer *g_SPRViewer = 0;
char g_appTitle[] = APP_TITLE_STR;
static char recentFiles[8][256] = { "", "", "", "", "", "", "", "" };
extern bool bUseWeaponOrigin;
bool g_bStopPlaying = false;
bool g_bEndOfSequence = false;
static int g_nCurrFrame = 0;

void SPRViewer::initRecentFiles ()
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

void SPRViewer::loadRecentFiles( void )
{
	char	str[256];

	for( int i = 0; i < 8; i++ )
	{
		mx_snprintf( str, sizeof( str ), "RecentFile%i", i );
		if( !LoadString( str, recentFiles[i] ))
			break;
	}
}

void SPRViewer::saveRecentFiles( void )
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

SPRViewer :: SPRViewer() : mxWindow( 0, 0, 0, 0, 0, g_appTitle, mxWindow::Normal )
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

	menuFile->add ("Load Sprite...", IDC_FILE_LOADMODEL);
	menuFile->addSeparator ();
	menuFile->add ("Load Background Texture...", IDC_FILE_LOADBACKGROUNDTEX);
	menuFile->add ("Load Ground Texture...", IDC_FILE_LOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Unload Ground Texture", IDC_FILE_UNLOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Open PAK file...", IDC_FILE_OPENPAKFILE);
	menuFile->add ("Close PAK file", IDC_FILE_CLOSEPAKFILE);
	menuFile->addSeparator ();
	menuFile->addMenu ("Recent Sprites", menuRecentModels);
	menuFile->addMenu ("Recent PAK files", menuRecentPakFiles);
	menuFile->addSeparator ();
	menuFile->add ("Exit", IDC_FILE_EXIT);

	menuOptions->add ("Background Color...", IDC_OPTIONS_COLORBACKGROUND);
	menuOptions->add ("Ground Color...", IDC_OPTIONS_COLORGROUND);
	menuOptions->add ("Light Color...", IDC_OPTIONS_COLORLIGHT);
	menuOptions->addSeparator ();
	menuOptions->add( "Sprite AutoPlay", IDC_OPTIONS_AUTOPLAY );
	menuOptions->addSeparator ();
	menuOptions->add( "Use weapon origin", IDC_OPTIONS_WEAPONORIGIN );
	menuOptions->addSeparator ();
	menuOptions->add ("Center View", IDC_OPTIONS_CENTERVIEW);
	menuOptions->add ("Reset View", IDC_OPTIONS_RESETVIEW);
#ifdef WIN32
	menuOptions->addSeparator ();
	menuOptions->add ("Make Screenshot...", IDC_OPTIONS_MAKESCREENSHOT);
	//menuOptions->add ("Dump Sprite Info", IDC_OPTIONS_DUMP);
#endif
	menuView->add ("File Associations...", IDC_VIEW_FILEASSOCIATIONS);

#ifdef WIN32
	menuHelp->add ("Goto Homepage...", IDC_HELP_GOTOHOMEPAGE);
	menuHelp->addSeparator ();
#endif
	menuHelp->add ("About", IDC_HELP_ABOUT);

	mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );

	// create tabcontrol with subdialog windows
	tab = new mxTab (this, 0, 0, 0, 0, IDC_TAB);
#ifdef WIN32
	SetWindowLong ((HWND) tab->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif
	mxWindow *wRender = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wRender, "Sprite Display");
	mxLabel *RenderLabel = new mxLabel (wRender, 5, 3, 120, 20, "Render Mode");
	cRenderMode = new mxChoice (wRender, 5, 17, 112, 22, IDC_RENDERMODE);
	cRenderMode->add ("Normal");
	cRenderMode->add ("Additive");
	cRenderMode->add ("Indexalpha");
	cRenderMode->add ("Alpha test");
	cRenderMode->select (0);
	mxToolTip::add (cRenderMode, "Select Render Mode");

	mxLabel *OrientLabel = new mxLabel (wRender, 5, 43, 120, 20, "Orientation");
	cOrientType = new mxChoice (wRender, 5, 60, 112, 22, IDC_ORIENTATION);
	cOrientType->add ("Parallel Upright");
	cOrientType->add ("Facing Upright");
	cOrientType->add ("Parallel");
	cOrientType->add ("Oriented");
	cOrientType->add ("Parallel Oriented");
	cOrientType->select (0);
	mxToolTip::add (cOrientType, "Select Sprite Orientation");

	cbGround = new mxCheckBox (wRender, 130, 64, 85, 20, "Show Ground", IDC_GROUND);
	cbGround->setChecked( g_viewerSettings.showGround ? true : false );
	cbBackground = new mxCheckBox (wRender, 224, 64, 130, 20, "Show Background", IDC_BACKGROUND);

	lSpriteInfo1 = new mxLabel (wRender, 130, 5, 120, 15, "" );

	// Create widgets for the Animation Tab
	tbStop = new mxButton (wRender, 130, 22, 60, 18, "Stop", IDC_STOP);
	mxToolTip::add (tbStop, "Stop Playing");
	bPrevFrame = new mxButton (wRender, 209, 22, 30, 18, "<<", IDC_PREVFRAME);
	bPrevFrame->setEnabled (false);
	mxToolTip::add (bPrevFrame, "Prev Frame");
	leFrame = new mxLineEdit (wRender, 244, 22, 50, 18, "", IDC_FRAME); 
	leFrame->setEnabled (false);
	mxToolTip::add (leFrame, "Set Frame");
	bNextFrame = new mxButton (wRender, 299, 22, 30, 18, ">>", IDC_NEXTFRAME);
	bNextFrame->setEnabled (false);
	mxToolTip::add (bNextFrame, "Next Frame");	

	mxLabel *SpdLabel = new mxLabel (wRender, 295, 44, 35, 18, "Speed");
	slSpeedScale = new mxSlider (wRender, 125, 44, 165, 18, IDC_SPEEDSCALE);
	slSpeedScale->setRange (0, 200);
	slSpeedScale->setValue (40);
	mxToolTip::add (slSpeedScale, "Speed Scale");

	// create the OpenGL window
	d_GlWindow = new GlWindow (this, 0, 0, 0, 0, "", mxWindow::Normal);
#ifdef WIN32
	SetWindowLong ((HWND) d_GlWindow->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif
	g_GlWindow = d_GlWindow;

	// finally create the pakviewer window
	pakViewer = new PAKViewer (this);

	g_FileAssociation = new FileAssociation ();

	loadRecentFiles ();
	initRecentFiles ();

	setBounds (20, 20, 690, 550);
	setVisible (true);

	if( g_viewerSettings.showGround )
		setShowGround (true);

	if( g_viewerSettings.groundTexFile[0] )
		d_GlWindow->loadTexture( g_viewerSettings.groundTexFile, TEXTURE_GROUND );
	else d_GlWindow->loadTexture( NULL, TEXTURE_GROUND );
}

SPRViewer::~SPRViewer ()
{
	//g_viewerSettings.showMaximized = isMaximized();
	//saveRecentFiles ();
	//SaveViewerSettings ();
}

void SPRViewer :: checkboxSet( int id, bool bState )
{
	mb->setChecked( id, bState );
}

int
SPRViewer::handleEvent (mxEvent *event)
{
	if (event->event == mxEvent::Size)
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

		if (pakViewer->isVisible ())
		{
			w -= 170;
			pakViewer->setBounds (w, y, 170, h);
		}

		d_GlWindow->setBounds (0, y, w, h - HEIGHT);
		tab->setBounds (0, y + h - HEIGHT, w, HEIGHT);
		return 1;
	}

	if ( event->event == mxEvent::KeyDown )
	{
		redraw();
		switch( (char)event->key )
		{
		case 27:
			if( !getParent( )) // fullscreen mode ?
				mx::quit();
			break;
		case 37:
			if( g_viewerSettings.numSpritePathes > 0 )
				loadSprite( LoadPrevSprite( ));
			break;
		case 39:
			if( g_viewerSettings.numSpritePathes > 0 )
				loadSprite( LoadNextSprite( ));
			break;
#if XASH_WIN32
		// TODO handle F5 hotkey on Linux too
		case VK_F5:
		{
			bool oldUseWeaponOrigin = bUseWeaponOrigin;
			loadSprite( g_viewerSettings.spriteFile, false );
			bUseWeaponOrigin = oldUseWeaponOrigin;
			break;
		}
#endif
		case 'v':
		case 0xEC: // м in cp1251
			bUseWeaponOrigin = !mb->isChecked( IDC_OPTIONS_WEAPONORIGIN );
			mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
			break;
		}
		return 1;
	}

	switch (event->action)
	{
		case IDC_FILE_LOADMODEL:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "GoldSrc Sprite (*.spr)");
			if (ptr)
			{
				if (!loadSprite (ptr ))
				{
					char str[256];

					sprintf (str, "Error reading sprite: %s", ptr);
					mxMessageBox (this, str, "ERROR", MX_MB_OK | MX_MB_ERROR);
					break;
				}

				// now update recent files list

				int i;
				char path[256];

				if (event->action == IDC_FILE_LOADMODEL)
					strcpy (path, "[m] ");
				else
					strcpy (path, "[w] ");

				strcat (path, ptr);

				for (i = 0; i < 4; i++)
				{
					if (!mx_strcasecmp (recentFiles[i], path))
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

					strcpy (recentFiles[0], path);
				}

				initRecentFiles ();
			}
		}
		break;
		case IDC_FILE_SAVEMODEL:
		{
			char *ptr = (char *) mxGetSaveFileName (this, g_viewerSettings.spritePath, "GoldSrc Sprite (*.spr)");
			if (!ptr)
				break;

			char filename[256];
			char ext[16];

			strcpy( filename, ptr );
			strcpy( ext, mx_getextension( filename ));
			if( mx_strcasecmp( ext, ".spr" ))
				strcat( filename, ".spr" );

			if( !g_spriteModel.SaveSprite( filename ))
			{
				mxMessageBox( this, "Error saving sprite.", g_appTitle, MX_MB_OK | MX_MB_ERROR );
			}
			else
			{
				strcpy( g_viewerSettings.spriteFile, filename );
				g_viewerSettings.numModelChanges = 0;	// all the settings are handled
			}
		}
		break;
		case IDC_FILE_LOADBACKGROUNDTEX:
		case IDC_FILE_LOADGROUNDTEX:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "Windows Bitmap (*.bmp);;Truevision TGA (*.tga);;PCExchange (*.pcx)");
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
						setShowBackground (true);
					else
						setShowGround (true);

				}
				else
					mxMessageBox (this, "Error loading texture.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
			}
		}
		break;

		case IDC_FILE_UNLOADGROUNDTEX:
		{
			d_GlWindow->loadTexture( NULL, TEXTURE_GROUND );
			setShowGround (false);
		}
		break;
		case IDC_FILE_OPENPAKFILE:
		{
			const char *ptr = mxGetOpenFileName (this, "\\Quake\\id1\\", "GoldSrc Package File (*.pak)");
			if (ptr)
			{
				int i;

				pakViewer->openPAKFile (ptr);

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
			pakViewer->closePAKFile ();
			redraw ();
		}
		break;

		case IDC_FILE_RECENTMODELS1:
		case IDC_FILE_RECENTMODELS2:
		case IDC_FILE_RECENTMODELS3:
		case IDC_FILE_RECENTMODELS4:
		{
			int i = event->action - IDC_FILE_RECENTMODELS1;
			char *ptr = &recentFiles[i][4];

			if (!loadSprite( ptr ))
			{
				char str[256];

				sprintf (str, "Error reading sprite: %s", ptr);
				mxMessageBox (this, str, "ERROR", MX_MB_OK | MX_MB_ERROR);
				break;
			}

			// update recent sprite list

			char tmp[256];			
			strcpy (tmp, recentFiles[0]);
			strcpy (recentFiles[0], recentFiles[i]);
			strcpy (recentFiles[i], tmp);

			initRecentFiles ();
		}
		break;

		case IDC_FILE_RECENTPAKFILES1:
		case IDC_FILE_RECENTPAKFILES2:
		case IDC_FILE_RECENTPAKFILES3:
		case IDC_FILE_RECENTPAKFILES4:
		{
			int i = event->action - IDC_FILE_RECENTPAKFILES1 + 4;
			pakViewer->openPAKFile (recentFiles[i]);

			char tmp[256];			
			strcpy (tmp, recentFiles[4]);
			strcpy (recentFiles[4], recentFiles[i]);
			strcpy (recentFiles[i], tmp);

			initRecentFiles ();

			redraw ();
		}
		break;

		case IDC_FILE_EXIT:
			pakViewer->closePAKFile ();
			redraw ();
			mx::quit ();
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
#ifdef WIN32
		case IDC_OPTIONS_MAKESCREENSHOT:
		{
			char *ptr = (char *)mxGetSaveFileName (this, 0, "Windows Bitmap (*.bmp)");
			if (ptr)
			{
				if( !strstr( ptr, ".bmp" ))
					strcat( ptr, ".bmp" );
				makeScreenShot (ptr);
			}
		}
		break;
#endif
		case IDC_OPTIONS_WEAPONORIGIN:
			bUseWeaponOrigin = !mb->isChecked( IDC_OPTIONS_WEAPONORIGIN );
			mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
			break;

		case IDC_OPTIONS_AUTOPLAY:
			g_viewerSettings.sequence_autoplay = !mb->isChecked( IDC_OPTIONS_AUTOPLAY );
			mb->setChecked( IDC_OPTIONS_AUTOPLAY, g_viewerSettings.sequence_autoplay ? true : false );
			break;

		case IDC_OPTIONS_CENTERVIEW:
		{
			centerSprite ();
			d_GlWindow->redraw ();
		}
		break;
		case IDC_VIEW_FILEASSOCIATIONS:
			g_FileAssociation->setAssociation (0);
			g_FileAssociation->setVisible (true);
			break;

#ifdef WIN32
		case IDC_HELP_GOTOHOMEPAGE:
			ShellExecute(0, "open", BuildInfo::GetGitHubLink(), 0, 0, SW_SHOW);
			break;
#endif

		case IDC_HELP_ABOUT:
			mxMessageBox(this, va(
				APP_TITLE_STR " " APP_VERSION_STRING2 "\n"
				"Based on HLSV code by g-cont\n"
				"\n"
				"Left mouse button and drag - to rotate\n"
				"Right mouse button and drag - to zoom\n"
				"Shift + LMB and drag - to XY-pan\n"
				"\n"
				"Build date: \t%s\n"
				"Commit hash: \t%s\n"
				"Architecture: \t%s\n"
				"Platform: \t%s\n"
				"Website: \t%s", 
				BuildInfo::GetDate(), 
				BuildInfo::GetCommitHash(),
				BuildInfo::GetArchitecture(),
				BuildInfo::GetPlatform(),
				BuildInfo::GetGitHubLink()), 
				"About", MX_MB_OK | MX_MB_INFORMATION);
			break;

		// 
		// widget actions
		//
		//

		//
		// Sprite Panel
		//

		case IDC_RENDERMODE:
		{
			int index = cRenderMode->getSelectedIndex ();
			if (index >= 0)
			{
				setRenderMode (index);
			}
		}
		break;

		case IDC_ORIENTATION:
		{
			int index = cOrientType->getSelectedIndex ();
			if (index >= 0)
			{
				setOrientType (index);
			}
		}
		break;

		case IDC_GROUND:
			setShowGround (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_BACKGROUND:
			setShowBackground (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_SPEEDSCALE:
		{
			int v = ((mxSlider *)event->widget)->getValue ();
			g_viewerSettings.speedScale = (float) (v * 5) / 200.0f;
		}
		break;

		case IDC_STOP:
		{
			if( !g_bStopPlaying )
			{
				tbStop->setLabel ("Play");
				g_bStopPlaying = true;
				g_nCurrFrame = g_spriteModel.setFrame( -1 );
				leFrame->setLabel( va("%d", g_nCurrFrame) );
				bPrevFrame->setEnabled(true);
				leFrame->setEnabled(true);
				bNextFrame->setEnabled(true);
			}
			else
			{
				tbStop->setLabel ("Stop");
				g_bStopPlaying = false;
				if( g_bEndOfSequence )
					g_nCurrFrame = g_spriteModel.setFrame( 0 );
				bPrevFrame->setEnabled (false);
				leFrame->setEnabled (false);
				bNextFrame->setEnabled (false);
				g_bEndOfSequence = false;
			}
		}
		break;

		case IDC_PREVFRAME:
		{
			g_nCurrFrame = g_spriteModel.setFrame( g_nCurrFrame - 1 );
			leFrame->setLabel( va("%d", g_nCurrFrame) );
			g_bEndOfSequence = false;
		}
		break;

		case IDC_FRAME:
		{
			g_nCurrFrame = atoi (leFrame->getLabel ());
			g_nCurrFrame = g_spriteModel.setFrame( g_nCurrFrame );
			g_bEndOfSequence = false;
		}
		break;

		case IDC_NEXTFRAME:
		{
			g_nCurrFrame = g_spriteModel.setFrame( g_nCurrFrame + 1 );
			leFrame->setLabel( va("%d", g_nCurrFrame) );
			g_bEndOfSequence = false;
		}
		break;
	}

	return 1;
}


void SPRViewer :: redraw( void )
{
	mxEvent event;
	event.event = mxEvent::Size;
	event.width = w2 ();
	event.height = h2 ();
	handleEvent (&event);
}

void SPRViewer :: makeScreenShot( const char *filename )
{
#ifdef WIN32
	d_GlWindow->redraw ();
	int w = d_GlWindow->w2 ();
	int h = d_GlWindow->h2 ();

	mxImage *image = new mxImage ();
	if( image->create( w, h, 24 ))
	{
		glReadBuffer( GL_FRONT );
		glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, image->data );

		image->flip_vertical();

		if( !mxBmpWrite( filename, image ))
			mxMessageBox( this, "Error writing screenshot.", g_appTitle, MX_MB_OK|MX_MB_ERROR );

		delete image;
	}
#endif
}

int SPRViewer::getTableIndex()
{
	return tab->getSelectedIndex ();
}

void SPRViewer :: setRenderMode( int mode )
{
	if (mode >= 0)
	{
		cRenderMode->select (mode);
		d_GlWindow->setRenderMode (mode);
	}
}

void SPRViewer :: setOrientType( int mode )
{
	if (mode >= 0)
	{
		cOrientType->select (mode);
		d_GlWindow->setOrientType (mode);
	}
}

void SPRViewer :: setShowGround( bool b )
{
	g_viewerSettings.showGround = b;
	cbGround->setChecked (b);
}

void SPRViewer :: setShowBackground( bool b )
{
	g_viewerSettings.showBackground = b;
	cbBackground->setChecked (b);
}

void SPRViewer::centerSprite ()
{
	g_spriteModel.centerView( false );
}

bool SPRViewer::loadSprite( const char *ptr, bool centering )
{
	if( !d_GlWindow->loadSprite( ptr, centering ))
		return false;

	char path[256];
	strcpy (path, mx_getpath (ptr));
	mx_setcwd (path);

	setSpriteInfo();
	centerSprite();					
	d_GlWindow->redraw ();

	return true;
}

void SPRViewer::setSpriteInfo (void)
{
	static char str[1024];

	msprite_t *phdr = g_spriteModel.getSpriteHeader();

	if (phdr)
	{
		sprintf( str, "Frames: %d\n", phdr->numframes );
		setRenderMode( phdr->texFormat );
		setOrientType( phdr->type );
	}
	else
	{
		strcpy (str, "No Sprite.");
	}

	lSpriteInfo1->setLabel (str);
	d_GlWindow->setFrameInfo( 0, MAX_SPRITE_FRAMES );
}

int main( int argc, char *argv[] )
{
	//
	// make sure, we start in the right directory
	//
	mx_setcwd (mx::getApplicationPath ());

#if XASH_WIN32 == 1
	// HOTFIX hide console window
	ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

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

	InitViewerSettings();
	// TODO use json config file instead registry
	//LoadViewerSettings();

	mx::init (argc, argv);

	g_SPRViewer = new SPRViewer ();
	g_SPRViewer->setMenuBar (g_SPRViewer->getMenuBar ());
	g_SPRViewer->setBounds (20, 20, 640, 540);
	g_SPRViewer->setVisible (true);

	// TODO implement for other platforms
	//if( g_viewerSettings.showMaximized )
	//	g_SPRViewer->Maximize();

	if (Q_stristr (cmdline, ".spr"))
	{
		g_SPRViewer->loadSprite (cmdline);
	}

	int ret = mx::run ();

	mx::cleanup ();

	return ret;
}
