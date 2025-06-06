//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.h
// last modified:  May 29 1999, Mete Ciragan
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
#pragma once
#include "vector.h"
#include "studio.h"
#include <array>
#include <string>
#include <string_view>

class ViewerSettings
{
public:
	static constexpr std::string_view fileName = "settings.json";

	enum EditMode
	{
		EDIT_SOURCE = 0,
		EDIT_MODEL,
	};

	enum RenderMode
	{
		RM_WIREFRAME,
		RM_FLATSHADED,
		RM_SMOOTHSHADED,
		RM_TEXTURED,
		RM_BONEWEIGHTS,
		RM_NORMALS
	};

	ViewerSettings();
	bool Save();
	bool Load();

	// model 
	Vector rot;
	Vector trans;
	float movementScale;
	float editStep;
	EditMode editMode;
	bool editSize;

	// render
	RenderMode renderMode;
	float transparency;
	bool showBackground;
	bool showGround;
	bool showHitBoxes;
	bool showBones;
	bool showTexture;
	bool showAttachments;
	bool showNormals;
	bool showWireframeOverlay;
	bool enableIK;
	int texture;
	float textureScale;
	int skin;
	bool mirror;
	bool useStencil;	// if 3dfx fullscreen set false
	bool pending_export_uvmap;

	// animation
	int sequence;
	float speedScale;

	// bodyparts and bonecontrollers
	std::array<int, MAXSTUDIOMODELS> submodels;

	// fullscreen
	int width, height;
	bool cds;
	int showMaximized;

	// colors
	float bgColor[4];
	float lColor[4];
	float gColor[4];
	float gLightVec[3];

	bool sequence_autoplay;
	bool studio_blendweights;
	int topcolor;
	int bottomcolor;

	bool show_uv_map;
	bool overlay_uv_map;
	bool anti_alias_lines;

	// misc
	int textureLimit;
	bool pause;
	int drawn_polys;
	int numModelChanges;	// if user touch settings that directly stored into the model
	int numSourceChanges;	// editor counter

	// only used for fullscreen mode
	std::string modelFile;
	std::string modelPath;
	std::string oldModelPath;
	std::string backgroundTexFile;
	std::string groundTexFile;
	std::string uvmapPath;

	std::array<std::string, 2048> modelPathList;
	std::array<char[512], 8> recentFiles;
	int numModelPathes;
};
