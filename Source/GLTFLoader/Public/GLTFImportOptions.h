/// @file GLTFImportOptions.h by Robert Poncelet

#pragma once

//#include "Editor/UnrealEd/Classes/Factories/FbxStaticMeshImportData.h"
#include "UnrealString.h"
#include "Vector.h"
#include "Rotator.h"
#include "Color.h"
#include "NameTypes.h"

/// Used to store the information passed from the user to the import process - some of which is unused. This struct is adapted from FBXImportOptions.
struct GLTFImportOptions
{
	// General options
	bool bImportMaterials;
	bool bInvertNormalMap;
	bool bImportTextures;
	bool bImportLOD;
	bool bUsedAsFullName;
	bool bConvertScene;
	bool bRemoveNameSpace;
	bool bPreserveLocalTransform;
	FVector ImportTranslation;
	FRotator ImportRotation;
	float ImportUniformScale;
	bool bCorrectUpDirection;

	// Static Mesh options
	bool bCombineToSingle;
	FColor VertexOverrideColor;
	bool bRemoveDegenerates;
	bool bBuildAdjacencyBuffer;
	bool bGenerateLightmapUVs;
	bool bOneConvexHullPerUCX;
	bool bAutoGenerateCollision;

	FName StaticMeshLODGroup;

	static GLTFImportOptions Default()
	{
		GLTFImportOptions ImportOptions;
		ImportOptions.bAutoGenerateCollision = false;
		ImportOptions.bBuildAdjacencyBuffer = true;
		ImportOptions.bCombineToSingle = true;
		ImportOptions.bConvertScene = false;
		ImportOptions.bGenerateLightmapUVs = false;
		ImportOptions.bImportLOD = false;
		ImportOptions.bImportMaterials = false;
		ImportOptions.bImportTextures = false;
		ImportOptions.bInvertNormalMap = false;
		ImportOptions.bOneConvexHullPerUCX = true;
		ImportOptions.bPreserveLocalTransform = true;
		ImportOptions.bRemoveDegenerates = false;
		ImportOptions.bRemoveNameSpace = true;
		ImportOptions.bUsedAsFullName = false;
		ImportOptions.ImportRotation = FRotator(0.0f, 0.0f, 0.0f);
		ImportOptions.ImportTranslation = FVector::ZeroVector;
		ImportOptions.ImportUniformScale = 1.0f;
		ImportOptions.StaticMeshLODGroup = NAME_None;
		ImportOptions.VertexOverrideColor = FColor::White;
		ImportOptions.bCorrectUpDirection = true;
		return ImportOptions;
	}
};

