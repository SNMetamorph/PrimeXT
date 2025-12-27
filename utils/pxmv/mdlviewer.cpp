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
#include <time.h>
#include <mxTga.h>
#include <fmt/format.h>
#include <array>
#include <algorithm>
#include <utility>
#include <ctime>

#include "mdlviewer.h"
#include "GlWindow.h"
#include "ControlPanel.h"
#include "StudioModel.h"
#include "pakviewer.h"
#include "FileAssociation.h"
#include "file_system.h"
#include "stringlib.h"
#include "app_info.h"
#include "build.h"
#include "build_info.h"

MDLViewer *g_MDLViewer = 0;
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;

static bool IsAliasModel(const char *path)
{
	byte	buffer[256];
	FILE	*fp;
	int		remainBytes;

	if (!path) 
		return false;

	// load the model
	if ((fp = fopen(path, "rb")) == NULL)
		return false;

	fread(buffer, sizeof(buffer), 1, fp);
	remainBytes = ftell(fp);
	fclose(fp);

	if (remainBytes < 84)
		return false;

	// skip invalid signature
	if (Q_strncmp((const char *)buffer, "IDPO", 4))
		return false;

	// skip unknown version
	if (*(int *)&buffer[4] != 6)
		return false;

	return true;
}

static bool ValidateModel( const char *path )
{
	byte		buffer[256];
	studiohdr_t	*phdr;
	FILE		*fp;
	int			remainBytes;

	if (!path)
		return false;

	// load the model
	if ((fp = fopen(path, "rb")) == NULL)
		return false;

	fread(buffer, sizeof(buffer), 1, fp);
	remainBytes = ftell(fp);
	fclose(fp);

	if (remainBytes < sizeof(studiohdr_t))
		return false;

	// skip invalid signature
	if (Q_strncmp((const char *)buffer, "IDST", 4))
		return false;

	phdr = (studiohdr_t *)buffer;

	// skip unknown version
	if (phdr->version != STUDIO_VERSION)
		return false;

	// skip modelnameT.mdl
	if (phdr->numbones <= 0)
		return false;

	return true;
}

static void AddPathToList( ViewerSettings &settings, const char *path )
{
	std::string modelPath;

	if( settings.numModelPathes >= settings.modelPathList.size() )
		return; // too many strings

	modelPath = fmt::format("{}/{}", settings.oldModelPath, path);
	std::replace(modelPath.begin(), modelPath.end(), '\\', '/');

	if( !ValidateModel( modelPath.c_str() ))
		return;

	settings.modelPathList[settings.numModelPathes++] = std::move(modelPath);
}

static void SortPathList( ViewerSettings &settings )
{
	// this is a selection sort (finds the best entry for each slot)
	std::string temp;
	for( int i = 0; i < settings.numModelPathes - 1; i++ )
	{
		for( int j = i + 1; j < settings.numModelPathes; j++ )
		{
			if (stricmp(settings.modelPathList[i].c_str(), settings.modelPathList[j].c_str()) > 0)
			{
				temp = std::move(settings.modelPathList[i]);
				settings.modelPathList[i] = std::move(settings.modelPathList[j]);
				settings.modelPathList[j] = std::move(temp);
			}
		}
	}
}

void ListDirectory( ViewerSettings &settings )
{
	stringlist_t list;
	char modelPath[512];
	
	COM_ExtractFilePath( settings.modelPath.c_str(), modelPath);

	if (settings.oldModelPath.compare(modelPath) == 0)
		return;	// not changed

	settings.oldModelPath = modelPath;
	std::string path = fmt::format("{}/*.mdl", modelPath);
	settings.numModelPathes = 0;

	stringlistinit( &list );
	listdirectory( &list, path.c_str(), true);

	if (list.numstrings < 1)
		return;

	for (int i = 0; i < list.numstrings; i++) {
		AddPathToList(settings, list.strings[i]);
	}
	SortPathList(settings);
	stringlistfreecontents( &list );
}

const char *LoadNextModel( ViewerSettings &settings )
{
	int	i;
	std::string modelPath = settings.modelPath;
	std::replace(modelPath.begin(), modelPath.end(), '\\', '/');

	for (i = 0; i < settings.numModelPathes; i++)
	{
		if (stricmp(modelPath.c_str(), settings.modelPathList[i].c_str()) == 0)
		{
			i++;
			break;
		}
	}

	if (i == settings.numModelPathes)
		i = 0;

	return settings.modelPathList[i].c_str();
}

const char *LoadPrevModel( ViewerSettings &settings )
{
	int	i;
	std::string modelPath = settings.modelPath;
	std::replace(modelPath.begin(), modelPath.end(), '\\', '/');

	for (i = 0; i < settings.numModelPathes; i++)
	{
		if (stricmp(modelPath.c_str(), settings.modelPathList[i].c_str()) == 0)
		{
			i--;
			break;
		}
	}

	if (i < 0)
		i = settings.numModelPathes - 1;

	return settings.modelPathList[i].c_str();
}

void MDLViewer::initRecentFiles( void )
{
	for (int i = 0; i < m_settings.recentFiles.size(); i++)
	{
		if (strlen(m_settings.recentFiles[i]))
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, m_settings.recentFiles[i]);
		}
		else
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, "(empty)");
			mb->setEnabled (IDC_FILE_RECENTMODELS1 + i, false);
		}
	}
}

MDLViewer :: MDLViewer() : 
	mxWindow( 0, 0, 0, 0, 0, APP_TITLE_STR, mxWindow::Normal )
{
	// create menu stuff
	mb = new mxMenuBar (this);
	mxMenu *menuFile = new mxMenu ();
	mxMenu *menuOptions = new mxMenu ();
	mxMenu *menuView = new mxMenu ();
	mxMenu *menuHelp = new mxMenu ();

	mb->addMenu ("File", menuFile);
	mb->addMenu ("Options", menuOptions);
	//mb->addMenu ("Tools", menuView);
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

	//menuView->add ("File Associations...", IDC_VIEW_FILEASSOCIATIONS);

#ifdef WIN32
	menuHelp->add ("Goto Homepage...", IDC_HELP_GOTOHOMEPAGE);
	menuHelp->addSeparator ();
#endif
	menuHelp->add ("About...", IDC_HELP_ABOUT);

	mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
	mb->setChecked( IDC_OPTIONS_LEFTHAND, bUseWeaponLeftHand );
	mb->setChecked( IDC_OPTIONS_AUTOPLAY, m_settings.sequence_autoplay ? true : false );
	mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, m_settings.studio_blendweights ? true : false );

	m_studioModel = reinterpret_cast<StudioModel*>(malloc(sizeof(StudioModel)));
	memset(m_studioModel, 0x0, sizeof(StudioModel));
	new (m_studioModel) StudioModel(m_settings);

	// create the OpenGL window
	d_GlWindow = new GlWindow(this, m_settings, *m_studioModel, 0, 0, 0, 0, "", mxWindow::Normal);
#ifdef WIN32
	SetWindowLong ((HWND) d_GlWindow->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif

	d_cpl = new ControlPanel(this, m_settings, *m_studioModel);
	d_cpl->setGlWindow (d_GlWindow);
	d_GlWindow->setControlPanel(d_cpl);
	g_GlWindow = d_GlWindow;

	// finally create the pakviewer window
	d_PAKViewer = new PAKViewer (this, *m_studioModel);
	g_FileAssociation = new FileAssociation ();

	initRecentFiles ();

	setBounds (20, 20, 640, 540);
	setVisible (true);

	if( m_settings.showGround )
		d_cpl->setShowGround (true);

	if( m_settings.groundTexFile[0] )
		d_GlWindow->loadTexture( m_settings.groundTexFile.c_str(), TEXTURE_GROUND);

	else d_GlWindow->loadTexture( NULL, TEXTURE_GROUND );
}



MDLViewer::~MDLViewer ()
{
	// grab some params in case that hasn't updates
	if( d_cpl )
		m_settings.editStep = d_cpl->getEditStep();

	m_settings.showMaximized = isMaximized();

	m_settings.Save();
	m_studioModel->FreeModel();
	m_studioModel->~StudioModel();
	free(m_studioModel);
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
			const char *ptr = mxGetOpenFileName (this, 0, "GoldSrc Models (*.mdl)");
			if (ptr)
			{
				int i;
				d_cpl->loadModel (ptr);

				for (i = 0; i < 4; i++)
				{
					if (!mx_strcasecmp (m_settings.recentFiles[i], ptr))
						break;
				}

				// swap existing recent file
				if (i < 4)
				{
					char tmp[256];
					strcpy (tmp, m_settings.recentFiles[0]);
					strcpy (m_settings.recentFiles[0], m_settings.recentFiles[i]);
					strcpy (m_settings.recentFiles[i], tmp);
				}

				// insert recent file
				else
				{
					for (i = 3; i > 0; i--)
						strcpy (m_settings.recentFiles[i], m_settings.recentFiles[i - 1]);

					strcpy (m_settings.recentFiles[0], ptr);
				}

				initRecentFiles ();
			}
		}
		break;

		case IDC_FILE_SAVEMODEL:
		{
			char *ptr = (char *) mxGetSaveFileName (this, m_settings.modelPath.c_str(), "GoldSrc Model (*.mdl)");
			if (!ptr)
				break;

			char filename[256];
			char ext[16];

			strcpy( filename, ptr );
			strcpy( ext, mx_getextension( filename ));
			if( mx_strcasecmp( ext, ".mdl" ))
				strcat( filename, ".mdl" );

			if( !m_studioModel->SaveModel( filename ))
			{
				mxMessageBox( this, "Error saving model.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR );
			}
			else
			{
				m_settings.modelFile = filename;
				m_settings.numModelChanges = 0;	// all the settings are handled
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
						d_cpl->setShowBackground (true);
					else
						d_cpl->setShowGround (true);

				}
				else
					mxMessageBox (this, "Error loading texture.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
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
			const char *ptr = mxGetOpenFileName (this, "\\sierra\\half-life\\valve", "GoldSrc Package File (*.pak)");
			if (ptr)
			{
				int i;

				d_PAKViewer->openPAKFile (ptr);

				for (i = 4; i < 8; i++)
				{
					if (!mx_strcasecmp (m_settings.recentFiles[i], ptr))
						break;
				}

				// swap existing recent file
				if (i < 8)
				{
					char tmp[256];
					strcpy (tmp, m_settings.recentFiles[4]);
					strcpy (m_settings.recentFiles[4], m_settings.recentFiles[i]);
					strcpy (m_settings.recentFiles[i], tmp);
				}

				// insert recent file
				else
				{
					for (i = 7; i > 4; i--)
						strcpy (m_settings.recentFiles[i], m_settings.recentFiles[i - 1]);

					strcpy (m_settings.recentFiles[4], ptr);
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
			d_cpl->loadModel (m_settings.recentFiles[i]);

			char tmp[256];			
			strcpy (tmp, m_settings.recentFiles[0]);
			strcpy (m_settings.recentFiles[0], m_settings.recentFiles[i]);
			strcpy (m_settings.recentFiles[i], tmp);

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
			d_PAKViewer->openPAKFile (m_settings.recentFiles[i]);

			char tmp[256];			
			strcpy (tmp, m_settings.recentFiles[4]);
			strcpy (m_settings.recentFiles[4], m_settings.recentFiles[i]);
			strcpy (m_settings.recentFiles[i], tmp);

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
			float *cols[3] = { m_settings.bgColor, m_settings.gColor, m_settings.lColor };
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
			std::array<char, 128> nameBuffer;
			std::time_t currentTime = std::time(nullptr);
			std::strftime(nameBuffer.data(), nameBuffer.size(), "pxmv_%F_%H-%M-%S.png", std::gmtime(&currentTime));
			const char *fileName = (char *)mxGetSaveFileName( this, nameBuffer.data(), "Any supported format (*.bmp; *.tga; *.dds; *.png)");

			if (fileName)
			{
				d_GlWindow->dumpViewport(fileName);
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
			m_settings.sequence_autoplay = !mb->isChecked( IDC_OPTIONS_AUTOPLAY );
			mb->setChecked( IDC_OPTIONS_AUTOPLAY, m_settings.sequence_autoplay ? true : false );
			break;

		case IDC_OPTIONS_BLENDWEIGHTS:
			m_settings.studio_blendweights = !mb->isChecked( IDC_OPTIONS_BLENDWEIGHTS );
			mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, m_settings.studio_blendweights ? true : false );
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
			ShellExecute(0, "open", BuildInfo::GetGitHubLink(), 0, 0, SW_SHOW);
			break;
#endif

		case IDC_HELP_ABOUT:
			mxMessageBox(this, va(
				APP_TITLE_STR " " APP_VERSION_STRING2 "\n"
				"Based on \"Paranoia 2 Model Viewer\" made by g-cont\n"
				"\n"
				"Left mouse button and drag - to rotate model\n"
				"Right mouse button and drag - to zoom camera\n"
				"Shift + LMB and drag - to XY-pan\n"
				"Ctrl + LMB and drag - to move light source\n"
				"Left/Right Arrow - to switch between MDL files in current directory\n"
				"\n"
				"Build date:   \t%s\n"
				"Commit hash:  \t%s\n"
				"Architecture: \t%s\n"
				"Platform:     \t%s\n"
				"Website:      \t%s", 
				BuildInfo::GetDate(), 
				BuildInfo::GetCommitHash(),
				BuildInfo::GetArchitecture(),
				BuildInfo::GetPlatform(),
				BuildInfo::GetGitHubLink()), 
				"About", MX_MB_OK | MX_MB_INFORMATION);
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

			if( lasttime > m_studioModel->getCurrentTime( ))
				break;
			lasttime = m_studioModel->getCurrentTime() + 0.1f;

			int iSeq = m_studioModel->GetSequence();

			if( iSeq == m_studioModel->SetSequence( iSeq + 1 ))
				m_studioModel->SetSequence( 0 );
		}
		break;

		case 27:
			if( !getParent( )) // fullscreen mode ?
				mx::quit();
			break;
		case 37:
			if( m_settings.numModelPathes > 0 )
				d_cpl->loadModel( LoadPrevModel( m_settings ));
			break;
		case 39:
			if( m_settings.numModelPathes > 0 )
				d_cpl->loadModel( LoadNextModel( m_settings ));
			break;
#if XASH_WIN32
		// TODO handle F5 hotkey on Linux too
		case VK_F5:
		{
			bool oldUseWeaponOrigin = bUseWeaponOrigin;
			d_cpl->loadModel( m_settings.modelFile.c_str(), false);
			bUseWeaponOrigin = oldUseWeaponOrigin;
			break;
		}
#endif
		case 'g':
		case 0xEF:
			m_settings.showGround = !m_settings.showGround;
			if( !m_settings.showGround )
				m_settings.mirror = false;
			break;
		case 'h':
		case 0xF0:
			m_settings.showHitBoxes = !m_settings.showHitBoxes;
			break;
		case 'o':
		case 0xF9:
			m_settings.showBones = !m_settings.showBones;
			break;
		case '5':
			m_settings.transparency -= 0.05f;
			if( m_settings.transparency < 0.0f )
				m_settings.transparency = 0.0f;
			break;
		case '6':
			m_settings.transparency += 0.05f;
			if( m_settings.transparency > 1.0f )
				m_settings.transparency = 1.0f;
			break;
		case 'b':
		case 0xE8:
			m_settings.showBackground = !m_settings.showBackground;
			break;
		case 's':
		case 0xFB:
			m_settings.useStencil = !m_settings.useStencil;
			break;
		case 'm':
		case 0xFC:
			m_settings.mirror = !m_settings.mirror;
			if( m_settings.mirror )
				m_settings.showGround = true;
			break;
		case 'v':
		case 0xEC:
			bUseWeaponOrigin = !mb->isChecked( IDC_OPTIONS_WEAPONORIGIN );
			mb->setChecked( IDC_OPTIONS_WEAPONORIGIN, bUseWeaponOrigin );
			break;
		case 'l':
		case 0xE4:
			bUseWeaponLeftHand = !mb->isChecked( IDC_OPTIONS_LEFTHAND );
			mb->setChecked( IDC_OPTIONS_LEFTHAND, bUseWeaponLeftHand );
			break;

		case 'w':
		case 0xF6:
			m_settings.studio_blendweights = !mb->isChecked( IDC_OPTIONS_BLENDWEIGHTS );
			mb->setChecked( IDC_OPTIONS_BLENDWEIGHTS, m_settings.studio_blendweights ? true : false );
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			m_settings.renderMode = static_cast<ViewerSettings::RenderMode>(event->key - '1');
			break;
		case '-':
			m_settings.speedScale -= 0.1f;
			if( m_settings.speedScale < 0.0f )
				m_settings.speedScale = 0.0f;
			break;
		case '+':
			m_settings.speedScale += 0.1f;
			if( m_settings.speedScale > 5.0f )
				m_settings.speedScale = 5.0f;
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

bool MDLViewer::shouldMaximizeWindow()
{
	return m_settings.showMaximized;
}

int main( int argc, char *argv[] )
{
	//
	// make sure, we start in the right directory
	//
	mx_setcwd (mx::getApplicationPath ());
	atexit( Sys_CloseLog );

	// init random generator
	srand( (unsigned)time( NULL ) );

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

	if( IsAliasModel( cmdline ))
	{
		// TODO print message q1 alias models deprecated
		return 0;
	}

	//mx::setDisplayMode (0, 0, 0);
	mx::init (argc, argv);
	g_MDLViewer = new MDLViewer ();
	g_MDLViewer->setMenuBar (g_MDLViewer->getMenuBar ());
	g_MDLViewer->setBounds (20, 20, 640, 540);
	g_MDLViewer->setVisible (true);

	if( g_MDLViewer->shouldMaximizeWindow() )
		g_MDLViewer->maximize();

	if (Q_stristr (cmdline, ".mdl"))
	{
		g_ControlPanel->loadModel (cmdline);
	}

	int ret = mx::run ();

	mx::cleanup ();

	return ret;
}

#if XASH_WIN32 == 1
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPreInst, LPSTR lpCmdLine, int nCmdShow)
{
	return main(__argc, __argv);
}
#endif
