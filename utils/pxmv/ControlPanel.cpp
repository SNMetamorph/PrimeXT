//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ControlPanel.cpp
// last modified:  Oct 20 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
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
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "StudioModel.h"
#include "GlWindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mx.h>
#include <mxBmp.h>
#include "mdlviewer.h"
#include "stringlib.h"
#include "cmdlib.h"
#include "app_info.h"

ControlPanel *g_ControlPanel = 0;
bool g_bStopPlaying = false;
bool g_bEndOfSequence = false;
static int g_nCurrFrame = 0;
extern void ListDirectory( ViewerSettings &settings );

ControlPanel :: ControlPanel( mxWindow *parent, ViewerSettings &settings, StudioModel &model ) : 
	mxWindow( parent, 0, 0, 0, 0, "Control Panel", mxWindow::Normal ),
	m_studioModel( model ),
	m_settings( settings )
{
	// create tabcontrol with subdialog windows
	tab = new mxTab (this, 0, 0, 0, 0, IDC_TAB);
#ifdef WIN32
	SetWindowLong ((HWND) tab->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif
	mxWindow *wRender = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wRender, "Model Display");
	mxLabel *RenderLabel = new mxLabel (wRender, 5, 3, 120, 20, "Render Mode");
	cRenderMode = new mxChoice (wRender, 5, 17, 112, 22, IDC_RENDERMODE);
	cRenderMode->add ("Wireframe");
	cRenderMode->add ("Flat Shaded");
	cRenderMode->add ("Smooth Shaded");
	cRenderMode->add ("Texture Shaded");
	cRenderMode->add ("BoneWeights");
	cRenderMode->add ("Normals");	
	cRenderMode->select (3);
	mxToolTip::add (cRenderMode, "Select Render Mode");
	lOpacityValue = new mxLabel (wRender, 5, 45, 100, 18, "Opacity: 100%");
	slTransparency = new mxSlider (wRender, 0, 62, 120, 18, IDC_TRANSPARENCY);
	slTransparency->setValue (100);
	mxToolTip::add (slTransparency, "Model Transparency");
	mxCheckBox *cbHitBoxes = new mxCheckBox (wRender, 140, 5, 120, 20, "Show Hit Boxes", IDC_HITBOXES);
	mxCheckBox *cbBones = new mxCheckBox (wRender, 140, 25, 120, 20, "Show Bones", IDC_BONES);
	cbAttachments = new mxCheckBox (wRender, 140, 45, 120, 20, "Show Attachments", IDC_ATTACHMENTS);
	mxCheckBox *cbNormals = new mxCheckBox (wRender, 140, 65, 120, 20, "Show Normals", IDC_NORMALS);

	cbGround = new mxCheckBox (wRender, 260, 5, 130, 20, "Show Ground", IDC_GROUND);
	cbGround->setChecked( m_settings.showGround ? true : false );

	cbMirror = new mxCheckBox (wRender, 260, 25, 130, 20, "Mirror Model On Ground", IDC_MIRROR);
	cbBackground = new mxCheckBox (wRender, 260, 45, 130, 20, "Show Background", IDC_BACKGROUND);
	mxCheckBox *cbWireframe = new mxCheckBox (wRender, 260, 65, 130, 20, "Wireframe Overlay", IDC_WIREFRAME);

	leMeshScale = new mxLineEdit (wRender, 430, 5, 50, 18, "1.0");
	mxToolTip::add (leMeshScale, "Mesh Scale");
	leBoneScale = new mxLineEdit (wRender, 430, 25, 50, 18, "1.0");
	mxToolTip::add (leBoneScale, "Bone Scale");
	mxButton *bMeshScale = new mxButton (wRender, 485, 5, 80, 18, "Scale Meshes", 10001);
	mxButton *bBoneScale = new mxButton (wRender, 485, 25, 80, 18, "Scale Bones", 10002);
	lDrawnPolys = new mxLabel (wRender, 430, 65, 110, 18, "Drawn Polys: 0");

	mxWindow *wBody = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wBody, "Body Parts");
	cBodypart = new mxChoice (wBody, 5, 5, 112, 22, IDC_BODYPART);
	mxToolTip::add (cBodypart, "Choose a bodypart");
	BodyPartLabel = new mxLabel (wBody, 120, 8, 150, 20, "Body 0");
	cSubmodel = new mxChoice (wBody, 5, 30, 112, 22, IDC_SUBMODEL);
	mxToolTip::add (cSubmodel, "Choose a submodel of current bodypart");
	mxLabel *SubModeLabel = new mxLabel (wBody, 120, 33, 55, 20, "Sub-model");
	lDrawnPolys2 = new mxLabel (wBody, 190, 58, 110, 18, "Drawn Polys: 0");
	lModelInfo1 = new mxLabel (wBody, 370, 12, 120, 100, "");
	lModelInfo2 = new mxLabel (wBody, 500, 12, 120, 100, "");
	cSkin = new mxChoice (wBody, 5, 55, 112, 22, IDC_SKINS);
	mxToolTip::add (cSkin, "Choose a skin family");
	mxLabel *SkinLabel = new mxLabel (wBody, 120, 58, 30, 20, "Skin");

	mxWindow *wTexture = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wTexture, "Textures");
	cTextures = new mxChoice (wTexture, 5, 18, 150, 22, IDC_TEXTURES);
	mxToolTip::add (cTextures, "Choose a texture");

	new mxButton (wTexture, 510, 5, 80, 18, "Import Texture", IDC_IMPORTTEXTURE);
	new mxButton (wTexture, 510, 25, 80, 18, "Export Texture", IDC_EXPORTTEXTURE);
	new mxButton (wTexture, 510, 45, 80, 18, "Export UV Map", IDC_EXPORT_UVMAP);
	new mxButton (wTexture, 510, 65, 80, 18, "Change Name", IDC_SET_TEXTURE_NAME );
	lTexSize = new mxLabel (wTexture, 5, 3, 140, 14, "Texture");

	cbChrome = new mxCheckBox (wTexture, 180, 3, 80, 22, "Chrome", IDC_CHROME);
	cbAdditive = new mxCheckBox (wTexture, 180, 23, 80, 22, "Additive", IDC_ADDITIVE);
	cbTransparent = new mxCheckBox (wTexture, 180, 43, 80, 22, "Transparent", IDC_TRANSPARENT);

	cbFullbright = new mxCheckBox (wTexture, 270, 3, 80, 22, "Fullbright", IDC_FULLBRIGHT);
	cbFlatshade = new mxCheckBox (wTexture, 270, 23, 80, 22, "Flat Shade", IDC_FLATSHADE);
	cbTwoSided = new mxCheckBox (wTexture, 270, 43, 80, 22, "Two Sided", IDC_TWO_SIDED);
	cbAlphaToCoverage = new mxCheckBox (wTexture, 180, 63, 80, 22, "Alpha To Coverage", IDC_ALPHA_TO_COVERAGE);
	cbSolidGeom = new mxCheckBox (wTexture, 270, 63, 80, 22, "Solid Geometry", IDC_SOLID_GEOM);

	cbShowUVMap = new mxCheckBox (wTexture, 400, 3, 100, 22, "Show UV Map", IDC_SHOW_UV_MAP);
	cbOverlayUVMap = new mxCheckBox (wTexture, 400, 23, 100, 22, "Overlay UV Map", IDC_OVERLAY_UV_MAP);
	cbAntiAliasLines = new mxCheckBox (wTexture, 400, 43, 100, 22, "Anti-Alias Lines", IDC_ANTI_ALIAS_LINES);
	leTextureName = new mxLineEdit( wTexture, 400, 66, 100, 18, "", IDC_EDIT_TEXTURE_NAME ); 

	slTexScale = new mxSlider(wTexture, 0, 60, 160, 18, IDC_TEXTURESCALE);
	mxToolTip::add(slTexScale, "Scale texture size");
	lTexScale = new mxLabel(wTexture, 5, 47, 140, 14, "Scale Texture View (1.00x)");
	slTexScale->setRange(-100, 100);
	slTexScale->setValue(0);

	mxWindow *wSequence = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wSequence, "Sequences");

	mxLabel *AnimSequence = new mxLabel(wSequence, 5, 3, 120, 18, "Animation Sequence");
	cSequence = new mxChoice(wSequence, 5, 18, 200, 22, IDC_SEQUENCE);
	mxToolTip::add(cSequence, "Select Sequence");
	tbStop = new mxButton(wSequence, 5, 46, 60, 18, "Stop", IDC_STOP);
	mxToolTip::add(tbStop, "Stop Playing");
	bPrevFrame = new mxButton(wSequence, 84, 46, 30, 18, "<<", IDC_PREVFRAME);
	bPrevFrame->setEnabled(false);
	mxToolTip::add(bPrevFrame, "Prev Frame");
	leFrame = new mxLineEdit(wSequence, 119, 46, 50, 18, "", IDC_FRAME);
	leFrame->setEnabled(false);
	mxToolTip::add(leFrame, "Set Frame");
	bNextFrame = new mxButton(wSequence, 174, 46, 30, 18, ">>", IDC_NEXTFRAME);
	bNextFrame->setEnabled(false);
	mxToolTip::add(bNextFrame, "Next Frame");
	bCopySequenceName = new mxButton(wSequence, 210, 18, 65, 22, "Copy name", IDC_COPYSEQUENCENAME);
	mxToolTip::add(bCopySequenceName, "Copy current animation sequence name");

	lSequenceInfo = new mxLabel (wSequence, 288, 12, 90, 100, "");

	mxLabel *SpdLabel = new mxLabel (wSequence, 170, 70, 35, 18, "Speed");
	slSpeedScale = new mxSlider (wSequence, 0, 70, 165, 18, IDC_SPEEDSCALE);
	slSpeedScale->setRange (0, 200);
	slSpeedScale->setValue (40);
	mxToolTip::add (slSpeedScale, "Speed Scale");

	slBlender0 = new mxSlider( wSequence, 380, 12, 145, 18, IDC_BLENDER0 );
	slBlender1 = new mxSlider( wSequence, 380, 35, 145, 18, IDC_BLENDER1 );
	cbLoopAnim = new mxCheckBox( wSequence, 385, 60, 90, 22, "Loop Animation", IDC_LOOPANIM );
	new mxLabel( wSequence, 467, 12, 55, 18, "Blender 0" );
	new mxLabel( wSequence, 467, 35, 55, 18, "Blender 1" );
	slBlender0->setRange( 0, 255 );
	slBlender1->setRange( 0, 255 );

	mxWindow *wMisc = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wMisc, "Misc");

	mxLabel *FlagLabel = new mxLabel (wMisc, 5, 3, 120, 20, "Global model flags");
	cbFlagRocket = new mxCheckBox (wMisc, 10, 23, 95, 22, "Rocket Trail", IDC_STUDIO_ROCKET );
	mxToolTip::add (cbFlagRocket, "leave red-orange particle trail + dynamic light at model origin");
	cbFlagGrenade = new mxCheckBox (wMisc, 10, 43, 95, 22, "Grenade Smoke", IDC_STUDIO_GRENADE );
	mxToolTip::add (cbFlagGrenade, "leave gray-black particle trail");
	cbFlagGib = new mxCheckBox (wMisc, 10, 63, 95, 22, "Gib Blood", IDC_STUDIO_GIB );
	mxToolTip::add (cbFlagGib, "leave dark red particle trail that obey gravity");

	cbFlagRotate = new mxCheckBox (wMisc, 110, 3, 95, 22, "Model Rotate", IDC_STUDIO_ROTATE );
	mxToolTip::add (cbFlagRotate, "model will auto-rotate by yaw axis. Useable for items (will be working only in Xash3D)");
	cbFlagTracer = new mxCheckBox (wMisc, 110, 23, 95, 22, "Green Trail", IDC_STUDIO_TRACER );
	mxToolTip::add (cbFlagTracer, "green split trail. e.g. monster_wizard from Quake");
	cbFlagZomgib = new mxCheckBox (wMisc, 110, 43, 95, 22, "Zombie Blood", IDC_STUDIO_ZOMGIB );
	mxToolTip::add (cbFlagZomgib, "small blood trail from zombie gibs");
	cbFlagTracer2 = new mxCheckBox (wMisc, 110, 63, 95, 22, "Orange Trail", IDC_STUDIO_TRACER2 );
	mxToolTip::add (cbFlagTracer2, "orange split trail + rotate");
	cbFlagTracer3 = new mxCheckBox (wMisc, 210, 3, 95, 22, "Purple Trail", IDC_STUDIO_TRACER3 );
	mxToolTip::add (cbFlagTracer3, "purple signle trail");
	cbFlagAmbientLight = new mxCheckBox (wMisc, 210, 23, 100, 22, "No shadelight", IDC_STUDIO_AMBIENT_LIGHT );
	mxToolTip::add (cbFlagAmbientLight, "ignore shadelight and adjust ambient light. Usefully for flying monsters");
	cbFlagTraceHitbox = new mxCheckBox (wMisc, 210, 43, 100, 22, "Hitbox Collision", IDC_STUDIO_TRACE_HITBOX );
	mxToolTip::add (cbFlagTraceHitbox, "allow model complex collision by hitboxes insted of bbox");
	cbFlagForceSkylight = new mxCheckBox (wMisc, 210, 63, 100, 22, "Force Skylight", IDC_STUDIO_FORCE_SKYLIGHT );
	mxToolTip::add (cbFlagForceSkylight, "always get light from environment settings even if sky is not visible or non-present");

	mxLabel *RemapLabel = new mxLabel (wMisc, 325, 3, 120, 20, "Remap colors");
	slTopColor = new mxSlider( wMisc, 320, 20, 145, 18, IDC_TOPCOLOR );
	slBottomColor = new mxSlider( wMisc, 320, 40, 145, 18, IDC_BOTTOMCOLOR );
	new mxLabel( wMisc, 467, 20, 55, 18, "Top Color" );
	new mxLabel( wMisc, 467, 40, 75, 18, "Bottom Color" );
	slTopColor->setRange( 0, 255 );
	slBottomColor->setRange( 0, 255 );
	slTopColor->setValue( m_settings.topcolor );
	slBottomColor->setValue( m_settings.bottomcolor );

	mxWindow *wControls = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wControls, "Controls");

	for( int i = 0; i < NUM_POSEPARAMETERS; i++ )
	{
		int x, y, y2, y3;
		x = 0;
		y = 2 + (i % 4) * 23;
		y2 = 2 + (i % 4) * 23;
		y3 = 3 + (i % 4) * 23;

		cPoseParameter[i] = new mxChoice (wControls, x + 3, y, 134, 18, IDC_POSEPARAMETER+i);	
		cPoseParameter[i]->setEnabled( false );

		slPoseParameter[i] = new mxSlider (wControls, x + 186, y2, 140, 18, IDC_POSEPARAMETER_SCALE+i);
		slPoseParameter[i]->setRange (0.0, 1.0);
		mxToolTip::add (slPoseParameter[i], "Parameter");
		slPoseParameter[i]->setEnabled( false );

		lePoseParameter[i] = new mxLineEdit ( wControls, x + 140, y3, 40, 18, ";-)", IDC_POSEPARAMETER_VALUE+i );
		lePoseParameter[i]->setEnabled( false );
	}

	cController = new mxChoice (wControls, 335, 5, 112, 22, IDC_CONTROLLER);	
	mxToolTip::add (cController, "Choose a bone controller");
	slController = new mxSlider (wControls, 330, 30, 117, 18, IDC_CONTROLLERVALUE);
	slController->setRange (0, 45);
	mxToolTip::add (slController, "Change current bone controller value");
	mxLabel *ControllerLabel = new mxLabel (wControls, 455, 8, 55, 20, "Controller");
	mxLabel *CtrlValueLabel = new mxLabel (wControls, 455, 33, 55, 20, "Value");

	cbEnableIK = new mxCheckBox (wControls, 335, 55, 100, 22, "Run IK", IDC_ENABLE_IK);
	cbEnableIK->setChecked( m_settings.enableIK ? true : false );

	mxWindow *wEdit = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wEdit, "Editor");

	mxLabel *EidtLabel1 = new mxLabel (wEdit, 5, 3, 120, 20, "Edit Type");
	cEditType = new mxChoice (wEdit, 5, 17, 112, 22, IDC_EDIT_TYPE);
	mxToolTip::add (cEditType, "Select item to editing");

	mxLabel *EidtLabel2 = new mxLabel (wEdit, 5, 42, 120, 20, "Edit Mode");
	cEditMode = new mxChoice (wEdit, 5, 58, 112, 22, IDC_EDIT_MODE);
	mxToolTip::add (cEditMode, "Select editor mode (change current model or grab values to put them into QC source)");
	cEditMode->add ("QC Source");
	cEditMode->add ("Real Model");
	cEditMode->select (m_settings.editMode);

	mxLabel *EidtLabel3 = new mxLabel (wEdit, 125, 3, 120, 20, "Step Size");
	leEditStep = new mxLineEdit (wEdit, 125, 19, 50, 18, va( "%g", m_settings.editStep ));
	mxToolTip::add (leEditStep, "Editor movement step size");
	cbEditSize = new mxCheckBox (wEdit, 125, 39, 50, 22, "size", IDC_EDIT_SIZE );
	mxToolTip::add (cbEditSize, "switch between editing the origin and size e.g. for hitboxes");

	mxLabel *EidtLabel4 = new mxLabel (wEdit, 185, 3, 170, 20, "QC source code:");
	leEditString = new mxLineEdit (wEdit, 185, 19, 290, 18, "" );

	if( m_settings.editMode == ViewerSettings::EDIT_SOURCE )
		leEditString->setEnabled( true );
	else 
		leEditString->setEnabled( false );

	tbMovePosX = new mxButton (wEdit, 185, 43, 55, 20, "Move +X", IDC_MOVE_PX );
	tbMoveNegX = new mxButton (wEdit, 185, 64, 55, 20, "Move -X", IDC_MOVE_NX );
	tbMovePosY = new mxButton (wEdit, 245, 43, 55, 20, "Move +Y", IDC_MOVE_PY );
	tbMoveNegY = new mxButton (wEdit, 245, 64, 55, 20, "Move -Y", IDC_MOVE_NY );
	tbMovePosZ = new mxButton (wEdit, 305, 43, 55, 20, "Move +Z", IDC_MOVE_PZ );
	tbMoveNegZ = new mxButton (wEdit, 305, 64, 55, 20, "Move -Z", IDC_MOVE_NZ );

	tbConvertTexCoords = new mxButton (wEdit, 485, 19, 130, 20, "Convert Fixed TexCoords", IDC_CONVERT_TEXCOORDS );

	g_ControlPanel = this;

	iSelectionToSequence = NULL;
	iSequenceToSelection = NULL;
}

ControlPanel::~ControlPanel ()
{
	Mem_Free( iSelectionToSequence );
	Mem_Free( iSequenceToSelection );
}

int ControlPanel::getTableIndex()
{
	return tab->getSelectedIndex ();
}


int
ControlPanel::handleEvent (mxEvent *event)
{
	if (event->event == mxEvent::Size)
	{
		tab->setBounds (0, 0, event->width, event->height);
		return 1;
	}

	if ( event->event == mxEvent::KeyDown )
	{
		if (event->action >= IDC_POSEPARAMETER_VALUE && event->action < IDC_POSEPARAMETER_VALUE + NUM_POSEPARAMETERS)
		{
			int index = event->action - IDC_POSEPARAMETER_VALUE;
			int poseparam = cPoseParameter[index]->getSelectedIndex();

			float value =  atof( lePoseParameter[index]->getLabel() );
			setBlend( poseparam, value );
			slPoseParameter[index]->setValue( m_studioModel.GetPoseParameter( poseparam ) );
			return 1;
		}
		return 1;
	}

	switch (event->action)
	{
		case IDC_TAB:
		{
			m_settings.showTexture = (tab->getSelectedIndex () == TAB_TEXTURES);
		}
		break;

		case IDC_RENDERMODE:
		{
			int index = cRenderMode->getSelectedIndex ();
			if (index >= 0)
			{
				setRenderMode (index);
			}
		}
		break;

		case IDC_TRANSPARENCY:
		{
			int value = slTransparency->getValue ();
			m_settings.transparency = (float) value / 100.0f; 
			lOpacityValue->setLabel( va("Opacity: %d%%", value) ); 
		}
		break;

		case IDC_GROUND:
			setShowGround (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_MIRROR:
			setMirror (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_BACKGROUND:
			setShowBackground (((mxCheckBox *) event->widget)->isChecked ());
			break;

		case IDC_HITBOXES:
			m_settings.showHitBoxes = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_BONES:
			m_settings.showBones = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_ATTACHMENTS:
			m_settings.showAttachments = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_NORMALS:
			m_settings.showNormals = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_WIREFRAME:
			m_settings.showWireframeOverlay = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_ENABLE_IK:
			m_settings.enableIK = ((mxCheckBox *) event->widget)->isChecked ();
			break;

		case IDC_SEQUENCE:
		{
			int index = cSequence->getSelectedIndex ();
			if (index >= 0)
			{
				index = iSelectionToSequence[index];
				setSequence (index);
			}
		}
		break;

		case IDC_SPEEDSCALE:
		{
			int v = ((mxSlider *)event->widget)->getValue ();
			m_settings.speedScale = (float) (v * 5) / 200.0f;
		}
		break;

		case IDC_BLENDER0:
		{
			int v = ((mxSlider *)event->widget)->getValue();
			m_studioModel.SetBlendValue( 0, v );
		}
		break;

		case IDC_BLENDER1:
		{
			int v = ((mxSlider *)event->widget)->getValue();
			m_studioModel.SetBlendValue( 1, v );
		}
		break;

		case IDC_LOOPANIM:
		{
			studiohdr_t *hdr = m_studioModel.getStudioHeader ();
			if( hdr )
			{
				mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *) ((byte *) hdr + hdr->seqindex) + m_settings.sequence;

				if (cbLoopAnim->isChecked ())
					pseqdesc->flags |= STUDIO_LOOPING;
				else
					pseqdesc->flags &= ~STUDIO_LOOPING;

				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_STOP:
		{
			if( !g_bStopPlaying )
			{
				tbStop->setLabel ("Play");
				g_bStopPlaying = true;
				g_nCurrFrame = m_studioModel.SetFrame (-1);
				leFrame->setLabel (va("%d", g_nCurrFrame));
				bPrevFrame->setEnabled (true);
				leFrame->setEnabled (true);
				bNextFrame->setEnabled (true);
			}
			else
			{
				tbStop->setLabel ("Stop");
				g_bStopPlaying = false;
				if( g_bEndOfSequence )
					g_nCurrFrame = m_studioModel.SetFrame( 0 );
				bPrevFrame->setEnabled (false);
				leFrame->setEnabled (false);
				bNextFrame->setEnabled (false);
				g_bEndOfSequence = false;
			}
		}
		break;

		case IDC_PREVFRAME:
		{
			g_nCurrFrame = m_studioModel.SetFrame (g_nCurrFrame - 1);
			leFrame->setLabel (va("%d", g_nCurrFrame));
			g_bEndOfSequence = false;
		}
		break;

		case IDC_FRAME:
		{
			g_nCurrFrame = atoi (leFrame->getLabel ());
			g_nCurrFrame = m_studioModel.SetFrame (g_nCurrFrame);
			g_bEndOfSequence = false;
		}
		break;

		case IDC_NEXTFRAME:
		{
			g_nCurrFrame = m_studioModel.SetFrame (g_nCurrFrame + 1);
			leFrame->setLabel (va("%d", g_nCurrFrame));
			g_bEndOfSequence = false;
		}
		break;

		case IDC_COPYSEQUENCENAME:
		{
			if (cSequence->getItemCount() > 0)
			{
				int index = cSequence->getSelectedIndex();
				studiohdr_t *hdr = m_studioModel.getStudioHeader();
				if (hdr)
				{
					mstudioseqdesc_t *pseqdescs = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex);
					for (int i = 0, k = -1; i < hdr->numseq; i++)
					{
						if (!FBitSet(pseqdescs[i].flags, STUDIO_HIDDEN | STUDIO_AUTOPLAY)) {
							k++;
						}
						if (index == k)
						{
							COM_SetClipboardText(pseqdescs[i].label);
							break;
						}
					}
				}
			}
		}
		break;

		case IDC_BODYPART:
		{
			int index = cBodypart->getSelectedIndex ();
			if (index >= 0)
			{
				setBodypart (index);
			}
		}
		break;

		case IDC_SUBMODEL:
		{
			int index = cSubmodel->getSelectedIndex ();
			if (index >= 0)
			{
				int body = setSubmodel (index);
				BodyPartLabel->setLabel( va("Body %d", body) );
			}
		}
		break;

		case IDC_CONTROLLER:
		{
			int index = cController->getSelectedIndex ();
			if (index >= 0)
				setBoneController (index);
		}
		break;

		case IDC_CONTROLLERVALUE:
		{
			int index = cController->getSelectedIndex ();
			if (index >= 0)
				setBoneControllerValue (index, (float) slController->getValue ());
		}
		break;

		case IDC_SKINS:
		{
			int index = cSkin->getSelectedIndex ();
			if (index >= 0)
			{
				m_studioModel.SetSkin (index);
				m_settings.skin = index;
				d_GlWindow->redraw ();
			}
		}
		break;

		case IDC_TEXTURES:
		{
			int index = cTextures->getSelectedIndex ();
			if (index >= 0)
			{
				m_settings.texture = index;
				studiohdr_t *hdr = m_studioModel.getTextureHeader ();
				if (hdr)
				{
					mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + index;
					lTexSize->setLabel ( va("Texture (size: %d x %d)", ptexture->width, ptexture->height) );
					cbChrome->setChecked ((ptexture->flags & STUDIO_NF_CHROME) == STUDIO_NF_CHROME);
					cbAdditive->setChecked ((ptexture->flags & STUDIO_NF_ADDITIVE) == STUDIO_NF_ADDITIVE);
					cbTransparent->setChecked ((ptexture->flags & STUDIO_NF_MASKED) == STUDIO_NF_MASKED);
					cbFullbright->setChecked ((ptexture->flags & STUDIO_NF_FULLBRIGHT) == STUDIO_NF_FULLBRIGHT);
					cbFlatshade->setChecked ((ptexture->flags & STUDIO_NF_FLATSHADE) == STUDIO_NF_FLATSHADE);
					cbTwoSided->setChecked ((ptexture->flags & STUDIO_NF_TWOSIDE) == STUDIO_NF_TWOSIDE);
					cbAlphaToCoverage->setChecked ((ptexture->flags & STUDIO_NF_ALPHATOCOVERAGE) == STUDIO_NF_ALPHATOCOVERAGE);
					cbSolidGeom->setChecked ((ptexture->flags & STUDIO_NF_SOLIDGEOM) == STUDIO_NF_SOLIDGEOM);
					leTextureName->setLabel( ptexture->name );
					updateTexFlagsCheckBoxes();
				}
				d_GlWindow->redraw ();
			}

			cbShowUVMap->setChecked (m_settings.show_uv_map);
			cbOverlayUVMap->setChecked (m_settings.overlay_uv_map);
			cbAntiAliasLines->setChecked (m_settings.anti_alias_lines);
		}
		break;

		case IDC_CHROME:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbChrome->isChecked ())
				{
					ptexture->flags |= STUDIO_NF_CHROME|STUDIO_NF_FLATSHADE;
					cbFlatshade->setChecked (true);
				}
				else
				{
					ptexture->flags &= ~STUDIO_NF_CHROME;
					ptexture->flags &= ~STUDIO_NF_FLATSHADE;
					cbFlatshade->setChecked (false);
				}
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_ADDITIVE:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbAdditive->isChecked ())
					ptexture->flags |= STUDIO_NF_ADDITIVE;
				else
					ptexture->flags &= ~STUDIO_NF_ADDITIVE;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_TRANSPARENT:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbTransparent->isChecked ())
					ptexture->flags |= STUDIO_NF_MASKED;
				else
					ptexture->flags &= ~STUDIO_NF_MASKED;

				// reload texture in case palette was changed
				m_studioModel.UploadTexture (ptexture, (byte *) hdr + ptexture->index, (byte *) hdr + ptexture->index + ptexture->width * ptexture->height, TEXTURE_COUNT + m_settings.texture );
				m_settings.numModelChanges++;
				updateTexFlagsCheckBoxes();
			}
		}
		break;

		case IDC_FULLBRIGHT:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbFullbright->isChecked ())
					ptexture->flags |= STUDIO_NF_FULLBRIGHT;
				else
					ptexture->flags &= ~STUDIO_NF_FULLBRIGHT;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_FLATSHADE:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbFlatshade->isChecked ())
					ptexture->flags |= STUDIO_NF_FLATSHADE;
				else
					ptexture->flags &= ~STUDIO_NF_FLATSHADE;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_TWO_SIDED:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbTwoSided->isChecked ())
					ptexture->flags |= STUDIO_NF_TWOSIDE;
				else
					ptexture->flags &= ~STUDIO_NF_TWOSIDE;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_ALPHA_TO_COVERAGE:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbAlphaToCoverage->isChecked ())
					ptexture->flags |= STUDIO_NF_ALPHATOCOVERAGE;
				else
					ptexture->flags &= ~STUDIO_NF_ALPHATOCOVERAGE;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_SOLID_GEOM:
		{
			studiohdr_t *hdr = m_studioModel.getTextureHeader ();
			if (hdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex) + m_settings.texture;
				if (cbSolidGeom->isChecked ())
					ptexture->flags |= STUDIO_NF_SOLIDGEOM;
				else
					ptexture->flags &= ~STUDIO_NF_SOLIDGEOM;
				m_settings.numModelChanges++;
			}
		}
		break;

		case IDC_SHOW_UV_MAP:
		{
			if (cbShowUVMap->isChecked ())
				m_settings.show_uv_map = true;
			else 
				m_settings.show_uv_map = false;
		}
		break;

		case IDC_OVERLAY_UV_MAP:
		{
			if (cbOverlayUVMap->isChecked ())
				m_settings.overlay_uv_map = true;
			else 
				m_settings.overlay_uv_map = false;
		}
		break;

		case IDC_ANTI_ALIAS_LINES:
		{
			if (cbAntiAliasLines->isChecked ())
				m_settings.anti_alias_lines = true;
			else 
				m_settings.anti_alias_lines = false;
		}
		break;

		case IDC_EXPORTTEXTURE:
		{
			char *ptr = (char *) mxGetSaveFileName (this, "", "Windows Bitmap (*.bmp)");
			if (!ptr)
				break;

			char filename[256];
			char ext[16];

			strcpy (filename, ptr);
			strcpy (ext, mx_getextension (filename));
			if (mx_strcasecmp (ext, ".bmp"))
				strcat (filename, ".bmp");

			studiohdr_t *phdr = m_studioModel.getTextureHeader ();
			if (phdr)
			{
				mxImage image;
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) phdr + phdr->textureindex) + m_settings.texture;
				image.width = ptexture->width;
				image.height = ptexture->height;
				image.bpp = 8;
				image.data = (void *) ((byte *) phdr + ptexture->index);
				image.palette = (void *) ((byte *) phdr + ptexture->width * ptexture->height + ptexture->index);
				if (!mxBmpWrite (filename, &image))
					mxMessageBox (this, "Error writing .BMP texture.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
				image.data = 0;
				image.palette = 0;
			}
		}
		break;

		case IDC_IMPORTTEXTURE:
		{
			char *ptr = (char *) mxGetOpenFileName (this, "", "Any supported texture (*.bmp; *.tga; *.dds; *.png)");
			if (!ptr)
				break;

			char filename[256];

			strcpy (filename, ptr);

			mxImage *image = d_GlWindow->readTextureFromFile(filename);
			if (!image)
			{
				mxMessageBox (this, "Error loading texture.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
				return 1;
			}

			if (!image->palette)
			{
				d_GlWindow->imageFree(image);
				mxMessageBox (this, "Error loading texture. Must be 8-bit!", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
				return 1;
			}

			studiohdr_t *phdr = m_studioModel.getTextureHeader ();
			if (phdr)
			{
				mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) phdr + phdr->textureindex) + m_settings.texture;
				if (image->width == ptexture->width && image->height == ptexture->height)
				{
					memcpy ((byte *) phdr + ptexture->index, image->data, image->width * image->height);
					memcpy ((byte *) phdr + ptexture->index + image->width * image->height, image->palette, 768);

					m_studioModel.UploadTexture (ptexture, (byte *) phdr + ptexture->index, (byte *) phdr + ptexture->index + image->width * image->height, TEXTURE_COUNT + m_settings.texture );
					m_settings.numModelChanges++;
				}
				else
					mxMessageBox (this, "Texture must be of same size.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
			}

			d_GlWindow->imageFree(image);
			d_GlWindow->redraw ();
		}
		break;

		case IDC_EXPORT_UVMAP:
		{
			char *ptr = (char *) mxGetSaveFileName (this, "", "Windows Bitmap (*.bmp)");
			if (!ptr)
				break;

			char ext[16];

			m_settings.uvmapPath = ptr;
			strncpy(ext, mx_getextension(m_settings.uvmapPath.c_str()), sizeof(ext));

			if (mx_strcasecmp(ext, ".bmp"))
				m_settings.uvmapPath = ".bmp";

			m_settings.pending_export_uvmap = true;
		}
		break;

		case IDC_SET_TEXTURE_NAME:
		{
			studiohdr_t *phdr = m_studioModel.getTextureHeader ();
			mstudiotexture_t *ptexture = (mstudiotexture_t *) ((byte *) phdr + phdr->textureindex) + m_settings.texture;

			// compare with original name
			if( Q_strncmp( ptexture->name, leTextureName->getLabel(), 64 ))
			{
				Q_strncpy( ptexture->name, leTextureName->getLabel(), 64 );
				m_settings.numModelChanges++;

				// refresh choise
				cTextures->removeAll ();
				mstudiotexture_t *ptextures = (mstudiotexture_t *) ((byte *)phdr + phdr->textureindex);
				for( int i = 0; i < phdr->numtextures; i++ )
					cTextures->add( ptextures[i].name );
					cTextures->select( m_settings.texture );
			}
		}
		break;

		case IDC_TEXTURESCALE:
		{
			float sliderFraction = ((mxSlider *)event->widget)->getValue() / 100.0f * 4.0f;
			if (sliderFraction >= 0.0f)
				m_settings.textureScale = sliderFraction + 1.0f;
			else
				m_settings.textureScale = 1.0f / (1.0f - sliderFraction);

			lTexScale->setLabel( va("Scale Texture View (%.2fx)", m_settings.textureScale) );
			d_GlWindow->redraw ();
		}
		break;

		case IDC_MISC:
		{
			slTopColor->setValue( m_settings.topcolor );
			slBottomColor->setValue( m_settings.bottomcolor );
		}
		break;

		case IDC_STUDIO_ROCKET:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagRocket->isChecked ())
					phdr->flags |= STUDIO_ROCKET;
				else phdr->flags &= ~STUDIO_ROCKET;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_GRENADE:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagGrenade->isChecked ())
					phdr->flags |= STUDIO_GRENADE;
				else phdr->flags &= ~STUDIO_GRENADE;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_GIB:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagGib->isChecked ())
					phdr->flags |= STUDIO_GIB;
				else phdr->flags &= ~STUDIO_GIB;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_ROTATE:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagRotate->isChecked ())
					phdr->flags |= STUDIO_ROTATE;
				else phdr->flags &= ~STUDIO_ROTATE;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_TRACER:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagTracer->isChecked ())
					phdr->flags |= STUDIO_TRACER;
				else phdr->flags &= ~STUDIO_TRACER;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_ZOMGIB:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagZomgib->isChecked ())
					phdr->flags |= STUDIO_ZOMGIB;
				else phdr->flags &= ~STUDIO_ZOMGIB;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_TRACER2:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagTracer2->isChecked ())
					phdr->flags |= STUDIO_TRACER2;
				else phdr->flags &= ~STUDIO_TRACER2;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_TRACER3:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagTracer3->isChecked ())
					phdr->flags |= STUDIO_TRACER3;
				else phdr->flags &= ~STUDIO_TRACER3;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_AMBIENT_LIGHT:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagAmbientLight->isChecked ())
					phdr->flags |= STUDIO_AMBIENT_LIGHT;
				else phdr->flags &= ~STUDIO_AMBIENT_LIGHT;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_TRACE_HITBOX:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagTraceHitbox->isChecked ())
					phdr->flags |= STUDIO_TRACE_HITBOX;
				else phdr->flags &= ~STUDIO_TRACE_HITBOX;
				m_settings.numModelChanges++;
			}
		}

		case IDC_STUDIO_FORCE_SKYLIGHT:
		{
			studiohdr_t *phdr = m_studioModel.getStudioHeader ();
			if( phdr )
			{
				if (cbFlagForceSkylight->isChecked ())
					phdr->flags |= STUDIO_FORCE_SKYLIGHT;
				else phdr->flags &= ~STUDIO_FORCE_SKYLIGHT;
				m_settings.numModelChanges++;
			}
		}

		case IDC_TOPCOLOR:
		{
			int v = ((mxSlider *)event->widget)->getValue();
			m_studioModel.SetTopColor( v );
		}
		break;

		case IDC_BOTTOMCOLOR:
		{
			int v = ((mxSlider *)event->widget)->getValue();
			m_studioModel.SetBottomColor( v );
		}
		break;

		case IDC_EDIT_MODE:
		{
			int index = cEditMode->getSelectedIndex ();
			if (index >= 0)
			{
				if( !m_studioModel.SetEditMode (index))
				{
					// edit mode is cancelled
					cEditMode->select (m_settings.editMode);
				}
				else
				{
					if( m_settings.editMode == ViewerSettings::EDIT_SOURCE )
						leEditString->setEnabled( true );
					else leEditString->setEnabled( false );
					leEditString->setLabel( m_studioModel.getQCcode( ));
				}
			}
		}
		break;

		case IDC_EDIT_TYPE:
		{
			int index = cEditType->getSelectedIndex ();
			if (index >= 0)
			{
				bool editSize = m_studioModel.SetEditType (index);
				leEditString->setLabel( m_studioModel.getQCcode( ));

				if( !editSize && m_settings.editSize )
				{
					cbEditSize->setChecked( false );
					toggleMoveSize( false );
				}
				cbEditSize->setEnabled( editSize );
			}
		}
		break;
		case IDC_MOVE_PX:
		case IDC_MOVE_NX:
		case IDC_MOVE_PY:
		case IDC_MOVE_NY:
		case IDC_MOVE_PZ:
		case IDC_MOVE_NZ:
		{
			m_settings.editStep = (float)atof (leEditStep->getLabel ());
			m_studioModel.editPosition( m_settings.editStep, event->action );
			leEditString->setLabel( m_studioModel.getQCcode( ));
		}
		break;

		case IDC_CONVERT_TEXCOORDS:
		{
			char	str[1024];

			Q_strcpy( str, "there is no reliable methods to autodetect changes between half-float and old fixed point.\nIf model will seems to be broken after proceed don't save her!\nIf you not sure what it is should doing, just press 'No' and forget about this tool" );
			if( mxMessageBox( g_GlWindow, str, APP_TITLE_STR, MX_MB_YESNO | MX_MB_QUESTION | MX_MB_WARNING ) == 0 )
				m_studioModel.ConvertTexCoords();
		}
		break;

		case IDC_EDIT_SIZE:
			toggleMoveSize( ((mxCheckBox *) event->widget)->isChecked ());
		break;

		case 10001:
		{
			float scale = (float) atof (leMeshScale->getLabel ());
			if (scale > 0.0f)
			{
				m_studioModel.scaleMeshes (scale);
			}
		}
		break;

		case 10002:
		{
			float scale = (float) atof (leBoneScale->getLabel ());
			if (scale > 0.0f)
			{
				m_studioModel.scaleBones (scale);
			}
		}
		break;

		default:
		{
			if (event->action >= IDC_POSEPARAMETER && event->action < IDC_POSEPARAMETER + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				float flMin, flMax;
				if (m_studioModel.GetPoseParameterRange( poseparam, &flMin, &flMax ))
				{
					slPoseParameter[index]->setRange( flMin, flMax );
					slPoseParameter[index]->setValue( m_studioModel.GetPoseParameter( poseparam ) );
					lePoseParameter[index]->setLabel( va("%.1f", m_studioModel.GetPoseParameter( poseparam )) );
				}
			}
			else if (event->action >= IDC_POSEPARAMETER_SCALE && event->action < IDC_POSEPARAMETER_SCALE + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER_SCALE;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				setBlend( poseparam, ((mxSlider *) event->widget)->getValue() );
				lePoseParameter[index]->setLabel( va("%.1f", ((mxSlider *) event->widget)->getValue()) );

				// move also sliders with same poseparams
				for( int i = 0; i < NUM_POSEPARAMETERS; i++ )
				{
					if( i == index ) continue;

					if( cPoseParameter[i]->getSelectedIndex() == poseparam )
					{
						setBlend( poseparam, ((mxSlider *) event->widget)->getValue() );
						slPoseParameter[i]->setValue( m_studioModel.GetPoseParameter( poseparam ) );
					}
				}
			}
		}
		break;
	}

	return 1;
}

void ControlPanel::toggleMoveSize ( bool size )
{
	if( size )
	{
		tbMovePosX->setLabel ("Size +X");
		tbMoveNegX->setLabel ("Size -X");
		tbMovePosY->setLabel ("Size +Y");
		tbMoveNegY->setLabel ("Size -Y");
		tbMovePosZ->setLabel ("Size +Z");
		tbMoveNegZ->setLabel ("Size -Z");
		m_settings.editSize = true;
	}
	else
	{
		tbMovePosX->setLabel ("Move +X");
		tbMoveNegX->setLabel ("Move -X");
		tbMovePosY->setLabel ("Move +Y");
		tbMoveNegY->setLabel ("Move -Y");
		tbMovePosZ->setLabel ("Move +Z");
		tbMoveNegZ->setLabel ("Move -Z");
		m_settings.editSize = false;
	}
}

void ControlPanel::dumpModelInfo ()
{
#if 0
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		DeleteFile ("midump.txt");
		FILE *file = fopen ("midump.txt", "wt");
		if (file)
		{
			byte *phdr = (byte *) hdr;
			int i;

			fprintf (file, "id: %c%c%c%c\n", phdr[0], phdr[1], phdr[2], phdr[3]);
			fprintf (file, "version: %d\n", hdr->version);
			fprintf (file, "name: \"%s\"\n", hdr->name);
			fprintf (file, "length: %d\n\n", hdr->length);

			fprintf (file, "eyeposition: %f %f %f\n", hdr->eyeposition[0], hdr->eyeposition[1], hdr->eyeposition[2]);
			fprintf (file, "min: %f %f %f\n", hdr->min[0], hdr->min[1], hdr->min[2]);
			fprintf (file, "max: %f %f %f\n", hdr->max[0], hdr->max[1], hdr->max[2]);
			fprintf (file, "bbmin: %f %f %f\n", hdr->bbmin[0], hdr->bbmin[1], hdr->bbmin[2]);
			fprintf (file, "bbmax: %f %f %f\n", hdr->bbmax[0], hdr->bbmax[1], hdr->bbmax[2]);
			
			fprintf (file, "flags: %d\n\n", hdr->flags);

			fprintf (file, "numbones: %d\n", hdr->numbones);
			for (i = 0; i < hdr->numbones; i++)
			{
				mstudiobone_t *pbones = (mstudiobone_t *) (phdr + hdr->boneindex);
				fprintf (file, "\nbone %d.name: \"%s\"\n", i + 1, pbones[i].name);
				fprintf (file, "bone %d.parent: %d\n", i + 1, pbones[i].parent);
				fprintf (file, "bone %d.flags: %d\n", i + 1, pbones[i].flags);
				fprintf (file, "bone %d.bonecontroller: %d %d %d %d %d %d\n", i + 1, pbones[i].bonecontroller[0], pbones[i].bonecontroller[1], pbones[i].bonecontroller[2], pbones[i].bonecontroller[3], pbones[i].bonecontroller[4], pbones[i].bonecontroller[5]);
				fprintf (file, "bone %d.value: %f %f %f %f %f %f\n", i + 1, pbones[i].value[0], pbones[i].value[1], pbones[i].value[2], pbones[i].value[3], pbones[i].value[4], pbones[i].value[5]);
				fprintf (file, "bone %d.scale: %f %f %f %f %f %f\n", i + 1, pbones[i].scale[0], pbones[i].scale[1], pbones[i].scale[2], pbones[i].scale[3], pbones[i].scale[4], pbones[i].scale[5]);
			}

			fprintf (file, "\nnumbonecontrollers: %d\n", hdr->numbonecontrollers);
			for (i = 0; i < hdr->numbonecontrollers; i++)
			{
				mstudiobonecontroller_t *pbonecontrollers = (mstudiobonecontroller_t *) (phdr + hdr->bonecontrollerindex);
				fprintf (file, "\nbonecontroller %d.bone: %d\n", i + 1, pbonecontrollers[i].bone);
				fprintf (file, "bonecontroller %d.type: %d\n", i + 1, pbonecontrollers[i].type);
				fprintf (file, "bonecontroller %d.start: %f\n", i + 1, pbonecontrollers[i].start);
				fprintf (file, "bonecontroller %d.end: %f\n", i + 1, pbonecontrollers[i].end);
				fprintf (file, "bonecontroller %d.rest: %d\n", i + 1, pbonecontrollers[i].rest);
				fprintf (file, "bonecontroller %d.index: %d\n", i + 1, pbonecontrollers[i].index);
			}

			fprintf (file, "\nnumhitboxes: %d\n", hdr->numhitboxes);
			for (i = 0; i < hdr->numhitboxes; i++)
			{
				mstudiobbox_t *pbboxes = (mstudiobbox_t *) (phdr + hdr->hitboxindex);
				fprintf (file, "\nhitbox %d.bone: %d\n", i + 1, pbboxes[i].bone);
				fprintf (file, "hitbox %d.group: %d\n", i + 1, pbboxes[i].group);
				fprintf (file, "hitbox %d.bbmin: %f %f %f\n", i + 1, pbboxes[i].bbmin[0], pbboxes[i].bbmin[1], pbboxes[i].bbmin[2]);
				fprintf (file, "hitbox %d.bbmax: %f %f %f\n", i + 1, pbboxes[i].bbmax[0], pbboxes[i].bbmax[1], pbboxes[i].bbmax[2]);
			}

			fprintf (file, "\nnumseq: %d\n", hdr->numseq);
			for (i = 0; i < hdr->numseq; i++)
			{
				mstudioseqdesc_t *pseqdescs = (mstudioseqdesc_t *) (phdr + hdr->seqindex);
				fprintf (file, "\nseqdesc %d.label: \"%s\"\n", i + 1, pseqdescs[i].label);
				fprintf (file, "seqdesc %d.fps: %f\n", i + 1, pseqdescs[i].fps);
				fprintf (file, "seqdesc %d.flags: %d\n", i + 1, pseqdescs[i].flags);
				fprintf (file, "<...>\n");
			}
/*
			fprintf (file, "\nnumseqgroups: %d\n", hdr->numseqgroups);
			for (i = 0; i < hdr->numseqgroups; i++)
			{
				mstudioseqgroup_t *pseqgroups = (mstudioseqgroup_t *) (phdr + hdr->seqgroupindex);
				fprintf (file, "\nseqgroup %d.label: \"%s\"\n", i + 1, pseqgroups[i].label);
				fprintf (file, "\nseqgroup %d.namel: \"%s\"\n", i + 1, pseqgroups[i].name);
				fprintf (file, "\nseqgroup %d.data: %d\n", i + 1, pseqgroups[i].data);
			}
*/
			hdr = m_studioModel.getTextureHeader ();
			fprintf (file, "\nnumtextures: %d\n", hdr->numtextures);
			fprintf (file, "textureindex: %d\n", hdr->textureindex);
			fprintf (file, "texturedataindex: %d\n", hdr->texturedataindex);
			for (i = 0; i < hdr->numtextures; i++)
			{
				mstudiotexture_t *ptextures = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex);
				fprintf (file, "\ntexture %d.name: \"%s\"\n", i + 1, ptextures[i].name);
				fprintf (file, "texture %d.flags: %d\n", i + 1, ptextures[i].flags);
				fprintf (file, "texture %d.width: %d\n", i + 1, ptextures[i].width);
				fprintf (file, "texture %d.height: %d\n", i + 1, ptextures[i].height);
				fprintf (file, "texture %d.index: %d\n", i + 1, ptextures[i].index);
			}

			hdr = m_studioModel.getStudioHeader ();
			fprintf (file, "\nnumskinref: %d\n", hdr->numskinref);
			fprintf (file, "numskinfamilies: %d\n", hdr->numskinfamilies);

			fprintf (file, "\nnumbodyparts: %d\n", hdr->numbodyparts);
			for (i = 0; i < hdr->numbodyparts; i++)
			{
				mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *) ((byte *) hdr + hdr->bodypartindex);
				fprintf (file, "\nbodypart %d.name: \"%s\"\n", i + 1, pbodyparts[i].name);
				fprintf (file, "bodypart %d.nummodels: %d\n", i + 1, pbodyparts[i].nummodels);
				fprintf (file, "bodypart %d.base: %d\n", i + 1, pbodyparts[i].base);
				fprintf (file, "bodypart %d.modelindex: %d\n", i + 1, pbodyparts[i].modelindex);
			}

			fprintf (file, "\nnumattachments: %d\n", hdr->numattachments);
			for (i = 0; i < hdr->numattachments; i++)
			{
				mstudioattachment_t *pattachments = (mstudioattachment_t *) ((byte *) hdr + hdr->attachmentindex);
				fprintf (file, "attachment %d.name: \"%s\"\n", i + 1, pattachments[i].name);
			}

			fclose (file);

			ShellExecute ((HWND) getHandle (), "open", "midump.txt", 0, 0, SW_SHOW);
		}
	}
#endif
}



int ControlPanel::loadModel (const char *filename, bool centering)
{
	m_studioModel.FreeModel ();
	if (m_studioModel.LoadModel ((char *) filename))
	{
		if (m_studioModel.PostLoadModel ((char *) filename))
		{
			char str[256], basename[64], basepath[256];

			initSequences ();
			initBodyparts ();
			initBoneControllers ();
			initSkins ();
			initTextures ();
			if( centering )
				centerView (false);
			initEditModes ();
			setModelFlags ();
			m_settings.modelFile = filename;
			m_settings.modelPath = filename;
			setModelInfo ();
			m_settings.sequence = 0;
			m_settings.speedScale = 1.0f;
			slSpeedScale->setValue (40);

			for (int i = 0; i < m_settings.submodels.size(); i++)
				m_settings.submodels[i] = 0;

			mx_setcwd (mx_getpath (filename));
			setSequence( 0 );

			COM_FileBase( filename, basename );
			Q_snprintf( str, sizeof( str ), "%s - %s.mdl", APP_TITLE_STR, basename );
			g_MDLViewer->setLabel( str );

			COM_ExtractFilePath( filename, basepath );
//			Sys_InitLog( va( "%s/%s.log", basepath, basename ));
			ListDirectory( m_settings );

			return 1;
		}
		else
		{
			m_settings.modelPath = filename;
			mxMessageBox (this, "Error post-loading model.", APP_TITLE_STR, MX_MB_ERROR | MX_MB_OK);
		}
	}
	else
	{
//		mxMessageBox (this, "Error loading model.", APP_TITLE_STR, MX_MB_ERROR | MX_MB_OK);
	}

	return 0;
}

void ControlPanel :: resetPlayingSequence( void )
{
	static char str[128];

	if( !g_bStopPlaying )
	{
		tbStop->setLabel ("Play");
		g_bStopPlaying = true;
		g_nCurrFrame = m_studioModel.SetFrame( -1 );
		sprintf (str, "%d", g_nCurrFrame);
		leFrame->setLabel (str);
		bPrevFrame->setEnabled (true);
		leFrame->setEnabled (true);
		bNextFrame->setEnabled (true);
		g_bEndOfSequence = true;
	}
}

void ControlPanel :: setPlaySequence( void )
{
	static char str[128];

	if( g_bStopPlaying )
	{
		tbStop->setLabel ("Stop");
		g_bStopPlaying = false;
		g_nCurrFrame = m_studioModel.SetFrame( 0 );
		sprintf (str, "%d", g_nCurrFrame);
		leFrame->setLabel (str);
		bPrevFrame->setEnabled (false);
		leFrame->setEnabled (false);
		bNextFrame->setEnabled (false);
		g_bEndOfSequence = false;
	}
}

void ControlPanel::setRenderMode( int mode )
{
	m_settings.renderMode = static_cast<ViewerSettings::RenderMode>(mode);
	d_GlWindow->redraw ();
}

void
ControlPanel::updatePoseParameters( )
{
	for (int i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		if (slPoseParameter[i]->isEnabled())
		{
			int j = cPoseParameter[i]->getSelectedIndex();
			float value = m_studioModel.GetPoseParameter( j );

			float temp = atof( lePoseParameter[i]->getLabel( ) );

			if (fabs( temp - value ) > 0.1)
			{
				slPoseParameter[i]->setValue( value );
				lePoseParameter[i]->setLabel( va("%.1f", value) );
			}
		}
	}
}

void
ControlPanel::setShowGround (bool b)
{
	m_settings.showGround = b;
	cbGround->setChecked (b);
	if (!b)
	{
		cbMirror->setChecked (b);
		m_settings.mirror = b;
	}
}



void
ControlPanel::setMirror (bool b)
{
	m_settings.useStencil = b;
	m_settings.mirror = b;
	cbMirror->setChecked (b);
	if (b)
	{
		cbGround->setChecked (b);
		m_settings.showGround = b;
	}
}



void
ControlPanel::setShowBackground (bool b)
{
	m_settings.showBackground = b;
	cbBackground->setChecked (b);
}



void
ControlPanel::initSequences ()
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		mstudioseqdesc_t *pseqdescs = (mstudioseqdesc_t *) ((byte *) hdr + hdr->seqindex);

		if( iSelectionToSequence ) Mem_Free( iSelectionToSequence );
		iSelectionToSequence = (int *)Mem_Alloc( hdr->numseq * sizeof( int ));

		if( iSequenceToSelection ) Mem_Free( iSequenceToSelection );
		iSequenceToSelection = (int*)Mem_Alloc( hdr->numseq * sizeof( int ));

		cSequence->removeAll ();
		int k = 0;

		for (int i = 0; i < hdr->numseq; i++)
		{
			if( !FBitSet( pseqdescs[i].flags, STUDIO_HIDDEN|STUDIO_AUTOPLAY ))
			{
				iSelectionToSequence[k] = i;
				iSequenceToSelection[i] = k;
				cSequence->add( pseqdescs[i].label );
				k++;
			}
			else
			{
				// previous valid selection
				iSequenceToSelection[i] = (k > 0) ? (k - 1) : 0;
			}
		}

		cSequence->select (0);
	}

	float flMin, flMax;
	for (int i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		if (m_studioModel.GetPoseParameterRange( i, &flMin, &flMax ))
		{
			cPoseParameter[i]->removeAll();
			for (int j = 0; j < m_studioModel.m_boneSetup.CountPoseParameters(); j++)
			{
				cPoseParameter[i]->add( m_studioModel.m_boneSetup.pPoseParameter(j)->name );
			}
			cPoseParameter[i]->select( i );
			cPoseParameter[i]->setEnabled( true );

			slPoseParameter[i]->setEnabled( true );
			slPoseParameter[i]->setRange( flMin, flMax );
			mxToolTip::add (slPoseParameter[i], m_studioModel.m_boneSetup.pPoseParameter(i)->name );
			lePoseParameter[i]->setLabel( va("%.1f", 0.0) );
			lePoseParameter[i]->setEnabled( true );
		}
		else
		{
			cPoseParameter[i]->setEnabled( false );
			slPoseParameter[i]->setEnabled( false );
			lePoseParameter[i]->setEnabled( false );
		}
		slPoseParameter[i]->setValue( 0.0 );
		setBlend( i, 0.0 );
	}

	if ( hdr )
	{
		for (int i = 0; i < m_studioModel.m_boneSetup.CountPoseParameters(); i++)
		{
			setBlend( i, 0.0 );
		}
	}
}

void ControlPanel :: setSequence( int index )
{
	cSequence->select( iSequenceToSelection[index] );
	m_studioModel.SetSequence( index );
	m_settings.sequence = index;

	int numSequenceBlends = m_studioModel.getNumBlendings();

	if( m_studioModel.hasLocalBlending( ))
	{
		// reset blending values and move sliders into 'default' positions
		slBlender0->setValue( m_studioModel.SetBlending( 0, 0.0 ));
		slBlender1->setValue( m_studioModel.SetBlending( 1, 0.0 ));
	}
	else numSequenceBlends = 0;

	// counter strike blending
	if( numSequenceBlends == 4 || numSequenceBlends == 9 )
	{
		slBlender0->setEnabled( true );
		slBlender1->setEnabled( true );
	}
	else if( numSequenceBlends > 1 )
	{
		slBlender0->setEnabled( true );
		slBlender1->setEnabled( false );
	}
	else
	{
		slBlender0->setEnabled( false );
		slBlender1->setEnabled( false );
	}

	// update animation looping
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if( hdr )
	{
		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *) ((byte *) hdr + hdr->seqindex) + m_settings.sequence;
		cbLoopAnim->setChecked( pseqdesc->flags & STUDIO_LOOPING );
	}

	setSequenceInfo();
	setPlaySequence();
}

void ControlPanel::addEditType ( const char *name, int type, int id )
{
	if( m_studioModel.AddEditField( type, id ))
		cEditType->add (name);
}

void ControlPanel::initEditModes( void )
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		char	str[256];
		int	i;

		cEditType->removeAll ();
		addEditType ("Model origin", TYPE_ORIGIN );
		addEditType ("Bound Box", TYPE_BBOX );
		addEditType ("Clip Box", TYPE_CBOX );
		addEditType ("Eye Position", TYPE_EYEPOSITION);

		for( i = 0; i < hdr->numattachments; i++ )
		{
			mstudioattachment_t *pattachment = (mstudioattachment_t *)((byte *)hdr + hdr->attachmentindex) + i;

			// local attachments contain the matrix and can't be edited directly
			if( FBitSet( pattachment->flags, STUDIO_ATTACHMENT_LOCAL ))
				continue;

			if( Q_strlen( pattachment->name ) && !Q_isdigit( pattachment->name ))
				sprintf( str, "%s", pattachment->name );
			else sprintf( str, "Attachment %i", i + 1 );
			addEditType (str, TYPE_ATTACHMENT, i );
		}

		for( i = 0; i < hdr->numhitboxes; i++ )
		{
			sprintf( str, "Hitbox %i", i + 1 );
			addEditType (str, TYPE_HITBOX, i );
		}

		m_studioModel.SetEditType (0);
		cEditType->select (0);

		leEditString->setLabel( m_studioModel.getQCcode( ));
		cbEditSize->setEnabled( false );
		toggleMoveSize( false );
	}
}

void ControlPanel::initBodyparts ()
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		int i;
		mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *) ((byte *) hdr + hdr->bodypartindex);

		cBodypart->removeAll ();
		if (hdr->numbodyparts > 0)
		{
			for (i = 0; i < hdr->numbodyparts; i++)
				cBodypart->add (pbodyparts[i].name);

			cBodypart->select (0);

			cSubmodel->removeAll ();
			for (i = 0; i < pbodyparts[0].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}

			cSubmodel->select (0);
			setSequenceInfo();
		}
	}
}



void
ControlPanel::setBodypart (int index)
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		//cBodypart->setEn
		cBodypart->select (index);
		if (index < hdr->numbodyparts)
		{
			mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *) ((byte *) hdr + hdr->bodypartindex);
			cSubmodel->removeAll ();
		
			for (int i = 0; i < pbodyparts[index].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
			//m_studioModel.SetBodygroup (index, 0);
		}
	}
}



int ControlPanel::setSubmodel (int index)
{
	m_settings.submodels[cBodypart->getSelectedIndex ()] = index;
	return m_studioModel.SetBodygroup (cBodypart->getSelectedIndex (), index);
}

void ControlPanel :: initBoneControllers( void )
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		int	i, controllerused[MAX_BONECTRLS];
		int	count = 0;

		cController->setEnabled (hdr->numbonecontrollers > 0);
		slController->setEnabled (hdr->numbonecontrollers > 0);
		cController->removeAll ();

		memset( controllerindex, 0, sizeof( controllerindex ));
		memset( controllervalues, 0, sizeof( controllervalues ));
		memset( controllerused, 0, sizeof( controllerused ));
		mstudiobonecontroller_t *pbonecontrollers = (mstudiobonecontroller_t *) ((byte *) hdr + hdr->bonecontrollerindex);

		// mark used controllers
		for (i = 0; i < hdr->numbonecontrollers; i++)
		{
			int inputfield = pbonecontrollers[i].index;
			inputfield = bound( 0, inputfield, STUDIO_MOUTH );
			controllerused[inputfield] = 1;
		}

		for (i = 0; i < MAX_BONECTRLS; i++)
		{
			if( !controllerused[i] )
				continue;

			char str[32];
			if( i == STUDIO_MOUTH )
				sprintf( str, "Mouth" );
			else sprintf( str, "Controller %d", i );
			controllerindex[count] = i;
			cController->add( str );
			count++;
		}

		if (count > 0)
		{
			cController->select (0);
			setBoneController (0);
		}

	}
}

void ControlPanel :: setBoneController( int index )
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if( hdr )
	{
		mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *) ((byte *) hdr + hdr->bonecontrollerindex);

		// find first controller that matches the index
		for (int i = 0; i < hdr->numbonecontrollers; i++, pbonecontroller++)
		{
			if (pbonecontroller->index == controllerindex[index])
			{
				slController->setRange ((int) pbonecontroller->start, (int) pbonecontroller->end);
				break;
			}
		}
		slController->setValue (controllervalues[index]);
	}
}

void ControlPanel :: setBoneControllerValue( int index, float value )
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		index = controllerindex[index];

		if (index == STUDIO_MOUTH) m_studioModel.SetMouth (value);
		else m_studioModel.SetController (index, value);
		controllervalues[index] = value;
	}
}

void ControlPanel::setBlend(int index, float value )
{
	m_studioModel.SetPoseParameter( index, value );
	// reset number of frames....
//	updateFrameSelection( );

//	updateGroundSpeed( );
}

void ControlPanel::initSkins ()
{
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();
	if (hdr)
	{
		cSkin->setEnabled (hdr->numskinfamilies > 0);
		cSkin->removeAll ();

		for (int i = 0; i < hdr->numskinfamilies; i++)
		{
			char str[32];
			sprintf (str, "Skin %d", i + 1);
			cSkin->add (str);
		}

		cSkin->select (0);
		m_studioModel.SetSkin (0);
		m_settings.skin = 0;
	}
}

void ControlPanel :: updateDrawnPolys( void )
{
	static char str[64];

	sprintf (str, "Drawn Polys: %d", m_settings.drawn_polys );
	lDrawnPolys->setLabel( str ); 
	lDrawnPolys2->setLabel( str );
}

void ControlPanel::updateTexFlagsCheckBoxes()
{
	cbSolidGeom->setEnabled(cbTransparent->isChecked());
	cbAlphaToCoverage->setEnabled(cbTransparent->isChecked());
}

void ControlPanel::setModelInfo ()
{
	static char str[2048];
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();

	if (!hdr)
		return;

	cbAttachments->setEnabled( hdr->numattachments > 0 );

	sprintf (str,
		"Bones: %d\n"
		"Bone Controllers: %d\n"
		"Hit Boxes: %d\n"
		"Sequences: %d\n"
		"Sequence Groups: %d\n",
		hdr->numbones,
		hdr->numbonecontrollers,
		hdr->numhitboxes,
		hdr->numseq,
		hdr->numseqgroups
		);

	lModelInfo1->setLabel (str);

	sprintf (str,
		"Textures: %d\n"
		"Skin Families: %d\n"
		"Bodyparts: %d\n"
		"Attachments: %d\n"
		"Transitions: %d\n",
		hdr->numtextures,
		hdr->numskinfamilies,
		hdr->numbodyparts,
		hdr->numattachments,
		hdr->numtransitions);

	lModelInfo2->setLabel (str);
}

void ControlPanel::setSequenceInfo ()
{
	static char str[2048];
	studiohdr_t *hdr = m_studioModel.getStudioHeader ();

	if (!hdr)
		return;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + m_settings.sequence;

	sprintf (str,
		"Sequence#: %d\n"
		"Frames: %d\n"
		"FPS: %.f\n"
		"Blends: %d\n"
		"# of events: %d\n",
		m_settings.sequence,
		pseqdesc->numframes,
		pseqdesc->fps,
		pseqdesc->numblends,
		pseqdesc->numevents
		);

	lSequenceInfo->setLabel (str);
}

void ControlPanel::setModelFlags( void )
{
	studiohdr_t *phdr = m_studioModel.getStudioHeader ();
	if (phdr)
	{
		cbFlagRocket->setChecked ((phdr->flags & STUDIO_ROCKET) == STUDIO_ROCKET);
		cbFlagGrenade->setChecked ((phdr->flags & STUDIO_GRENADE) == STUDIO_GRENADE);
		cbFlagGib->setChecked ((phdr->flags & STUDIO_GIB) == STUDIO_GIB);
		cbFlagRotate->setChecked ((phdr->flags & STUDIO_ROTATE) == STUDIO_ROTATE);
		cbFlagTracer->setChecked ((phdr->flags & STUDIO_TRACER) == STUDIO_TRACER);
		cbFlagZomgib->setChecked ((phdr->flags & STUDIO_ZOMGIB) == STUDIO_ZOMGIB);
		cbFlagTracer2->setChecked ((phdr->flags & STUDIO_TRACER2) == STUDIO_TRACER2);
		cbFlagTracer3->setChecked ((phdr->flags & STUDIO_TRACER3) == STUDIO_TRACER3);
		cbFlagAmbientLight->setChecked ((phdr->flags & STUDIO_AMBIENT_LIGHT) == STUDIO_AMBIENT_LIGHT);
		cbFlagTraceHitbox->setChecked ((phdr->flags & STUDIO_TRACE_HITBOX) == STUDIO_TRACE_HITBOX);
		cbFlagForceSkylight->setChecked ((phdr->flags & STUDIO_FORCE_SKYLIGHT) == STUDIO_FORCE_SKYLIGHT);
	}
}

void
ControlPanel::initTextures ()
{
	studiohdr_t *hdr = m_studioModel.getTextureHeader ();
	if (hdr)
	{
		cTextures->removeAll ();
		mstudiotexture_t *ptextures = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex);
		for (int i = 0; i < hdr->numtextures; i++)
			cTextures->add (ptextures[i].name);
		cTextures->select (0);
		m_settings.texture = 0;
		if (hdr->numtextures > 0)
		{
			char str[32];
			sprintf (str, "Texture (size: %d x %d)", ptextures[0].width, ptextures[0].height);
			lTexSize->setLabel (str);
			cbChrome->setChecked ((ptextures[0].flags & STUDIO_NF_CHROME) == STUDIO_NF_CHROME);
			cbAdditive->setChecked ((ptextures[0].flags & STUDIO_NF_ADDITIVE) == STUDIO_NF_ADDITIVE);
			cbTransparent->setChecked ((ptextures[0].flags & STUDIO_NF_MASKED) == STUDIO_NF_MASKED);
			cbFullbright->setChecked ((ptextures[0].flags & STUDIO_NF_FULLBRIGHT) == STUDIO_NF_FULLBRIGHT);
			cbFlatshade->setChecked ((ptextures[0].flags & STUDIO_NF_FLATSHADE) == STUDIO_NF_FLATSHADE);
			cbTwoSided->setChecked ((ptextures[0].flags & STUDIO_NF_TWOSIDE) == STUDIO_NF_TWOSIDE);
			cbAlphaToCoverage->setChecked ((ptextures[0].flags & STUDIO_NF_ALPHATOCOVERAGE) == STUDIO_NF_ALPHATOCOVERAGE);
			cbSolidGeom->setChecked ((ptextures[0].flags & STUDIO_NF_SOLIDGEOM) == STUDIO_NF_SOLIDGEOM);
			leTextureName->setLabel( ptextures[0].name );
			updateTexFlagsCheckBoxes();
		}
#if 0
		if( hdr->numbones <= 0 )
		{
			m_settings.showTexture = true;
			tab->select( 2 ); // probably we load "T.mdl" force to select textures view
		}
#endif
	}
}

float ControlPanel :: getEditStep( void )
{
	return (float)atof( leEditStep->getLabel ());
}

void ControlPanel :: centerView( bool reset )
{
	m_studioModel.centerView( reset );
	d_GlWindow->redraw ();
}
