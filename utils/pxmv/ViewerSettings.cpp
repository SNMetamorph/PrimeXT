//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.cpp
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
#include "ViewerSettings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mx.h>
#include <fstream>
#include <filesystem>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>
#include "StudioModel.h"
#include "conprint.h"
#include "file_system.h"

#if XASH_WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

ViewerSettings::ViewerSettings() :
	rot(Vector(0.f, 0.f, 0.f)),
	trans(Vector(0.f, 0.f, 0.f)),
	movementScale(1.0f),
	editStep(1.0f),
	editMode(EDIT_SOURCE),
	editSize(false),
	renderMode(RM_TEXTURED),
	transparency(1.0f),
	showBackground(false),
	showGround(false),
	showHitBoxes(false),
	showBones(false),
	showTexture(false),
	showAttachments(false),
	showNormals(false),
	showWireframeOverlay(false),
	enableIK(false),
	texture(0),
	textureScale(1.0f),
	skin(0),
	mirror(false),
	useStencil(false),
	pending_export_uvmap(false),
	sequence(0),
	speedScale(1.0f),
	submodels{ 0 },
	width(0),
	height(0),
	cds(false),
	showMaximized(false),
	bgColor{ 0.2f, 0.5f, 0.7f, 0.0f },
	lColor{ 1.0f, 1.0f, 1.0f, 0.0f },
	gColor{ 0.85f, 0.85f, 0.69f, 0.0f },
	gLightVec{ 0.0f, 0.0f, -1.0f },
	sequence_autoplay(true),
	studio_blendweights(true),
	topcolor(0),
	bottomcolor(0),
	show_uv_map(false),
	overlay_uv_map(false),
	anti_alias_lines(false),
	textureLimit(256),
	pause(false),
	drawn_polys(0),
	numModelChanges(0),
	numSourceChanges(0),
	modelFile{ 0 },
	modelPath{ 0 },
	oldModelPath{ 0 },
	backgroundTexFile{ 0 },
	groundTexFile{ 0 },
	uvmapPath{ 0 },
	modelPathList{ },
	recentFiles{ "", "", "", "", "", "", "", "" },
	numModelPathes(0)
{
	if (std::filesystem::exists(ViewerSettings::fileName)) {
		Load();
	}
	else {
		Save();
	}
}

static std::filesystem::path GetExecutableDirectory()
{
	char path[512] = { 0 };
#ifdef _WIN32
    GetModuleFileNameA(nullptr, path, sizeof(path));
    return std::filesystem::path(path).parent_path().string();
#else
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    return std::filesystem::path(std::string(path, (count > 0) ? count: 0)).parent_path().string();
#endif
}

void JsonSaveString( rapidjson::Document &doc, const char *pName, std::string_view pValue )
{
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetString(rapidjson::StringRef(pValue.data(), pValue.size()));
	doc.AddMember(rapidjson::StringRef(pName), jsonValue, allocator);
}

void JsonLoadString( const rapidjson::Document &doc, const char *pName, std::string &pValue )
{
	pValue = doc[pName].GetString();
}

void JsonSaveVector4D( rapidjson::Document &doc, const char *pName, float *pValue )
{
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetArray()
		.PushBack(pValue[0], allocator)
		.PushBack(pValue[1], allocator)
		.PushBack(pValue[2], allocator)
		.PushBack(pValue[3], allocator);
	doc.AddMember(rapidjson::StringRef(pName), jsonValue, allocator);
}

void JsonLoadVector4D( const rapidjson::Document &doc, const char *pName, float *pValue )
{
	const rapidjson::Value& array = doc[pName];
	pValue[0] = array[0].GetFloat();
	pValue[1] = array[1].GetFloat();
	pValue[2] = array[2].GetFloat();
	pValue[3] = array[3].GetFloat();
}

void JsonSaveVector3D( rapidjson::Document &doc, const char *pName, float *pValue )
{
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetArray()
		.PushBack(pValue[0], allocator)
		.PushBack(pValue[1], allocator)
		.PushBack(pValue[2], allocator);
	doc.AddMember(rapidjson::StringRef(pName), jsonValue, allocator);
}

void JsonLoadVector3D( const rapidjson::Document &doc, const char *pName, float *pValue )
{
	const rapidjson::Value& array = doc[pName];
	pValue[0] = array[0].GetFloat();
	pValue[1] = array[1].GetFloat();
	pValue[2] = array[2].GetFloat();
}

void JsonSaveFloat( rapidjson::Document &doc, const char *pName, float fValue )
{
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetFloat(fValue);
	doc.AddMember(rapidjson::StringRef(pName), jsonValue, allocator);
}

void JsonLoadFloat( const rapidjson::Value &doc, const char *pName, float &fValue )
{
	fValue = doc[pName].GetFloat();
}

template<class T>
void JsonSaveInt( rapidjson::Document &doc, const char *pName, const T &iValue )
{
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetInt(static_cast<int>(iValue));
	doc.AddMember(rapidjson::StringRef(pName), jsonValue, allocator);
}

template<class T>
void JsonLoadInt( const rapidjson::Document &doc, const char *pName, T &iValue )
{
	iValue = static_cast<T>(doc[pName].GetInt());
}

bool ViewerSettings::Load()
{
	std::ifstream file;
	rapidjson::Document doc;
	std::filesystem::path configFilePath = GetExecutableDirectory() / ViewerSettings::fileName.data();

	file.open(configFilePath, std::ios::in | std::ios::binary);
	rapidjson::IStreamWrapper isw(file);
	doc.ParseStream(isw);

	JsonLoadVector4D(doc, "backgroundColor", bgColor);
	JsonLoadVector4D(doc, "lightColor", lColor);
	JsonLoadVector4D(doc, "groundColor", gColor);
	JsonLoadVector3D(doc, "lightVector", gLightVec);
	JsonLoadInt(doc, "sequenceAutoPlay", sequence_autoplay);
	JsonLoadInt(doc, "studioBlendWeights", studio_blendweights);
	JsonLoadInt(doc, "topColor", topcolor);
	JsonLoadInt(doc, "bottomColor", bottomcolor);
	JsonLoadFloat(doc, "editStep", editStep);
	JsonLoadInt(doc, "editMode", editMode);
	JsonLoadInt(doc, "allowIK", enableIK);
	JsonLoadInt(doc, "showGround", showGround);
	JsonLoadString(doc, "groundTexPath", groundTexFile);
	JsonLoadInt(doc, "showMaximized", showMaximized);

	// load recent files array
	rapidjson::Value &list = doc["recentFiles"];
	for (int i = 0; i < recentFiles.size(); i++) {
		std::strncpy(recentFiles[i], list[i].GetString(), sizeof(recentFiles[i]));
	}

	return 1;
}

bool ViewerSettings::Save()
{
	std::ofstream file;
	rapidjson::Document doc;
	std::filesystem::path configFilePath = GetExecutableDirectory() / ViewerSettings::fileName.data();
	doc.SetObject();

	JsonSaveVector4D(doc, "backgroundColor", bgColor);
	JsonSaveVector4D(doc, "lightColor", lColor);
	JsonSaveVector4D(doc, "groundColor", gColor);
	JsonSaveVector3D(doc, "lightVector", gLightVec);
	JsonSaveInt(doc, "sequenceAutoPlay", sequence_autoplay);
	JsonSaveInt(doc, "studioBlendWeights", studio_blendweights);
	JsonSaveInt(doc, "topColor", topcolor);
	JsonSaveInt(doc, "bottomColor", bottomcolor);
	JsonSaveFloat(doc, "editStep", editStep);
	JsonSaveInt(doc, "editMode", editMode);
	JsonSaveInt(doc, "allowIK", enableIK);
	JsonSaveInt(doc, "showGround", showGround);
	JsonSaveString(doc, "groundTexPath", groundTexFile);
	JsonSaveInt(doc, "showMaximized", showMaximized);

	// save recent files list
	rapidjson::Value jsonValue;
	auto& allocator = doc.GetAllocator();
	jsonValue.SetArray();
	for (int i = 0; i < recentFiles.size(); i++) {
		jsonValue.PushBack(rapidjson::StringRef(recentFiles[i]), allocator);
	}
	doc.AddMember("recentFiles", jsonValue, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file.open(configFilePath, std::ios::out | std::ios::binary);
	if (file.is_open())
	{
		file.write(buffer.GetString(), buffer.GetSize());
		file.close();
	}
	return 1;
}
