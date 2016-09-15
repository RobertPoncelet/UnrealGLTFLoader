// Fill out your copyright notice in the Description page of Project Settings.

#include "GLTFLoaderPrivatePCH.h"
#include "GLTFFactory.h"

#include "Engine.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "GLTFMeshBuilder.h"

#include "UnrealEd.h"
#include "Factories.h"
#include "BusyCursor.h"
#include "SSkeletonWidget.h"

#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"

#include "FbxErrors.h"

#define LOCTEXT_NAMESPACE "GLTFFactory"

UGLTFFactory::UGLTFFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("gltf;GLTF meshes"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	bOperationCanceled = false;
	bDetectImportTypeOnImport = false;
}

//This function is adapted from UFbxFactory::CreateBinary()
UObject* UGLTFFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn,
	bool&				bOutOperationCanceled
)
{
	if (bOperationCanceled)
	{
		bOutOperationCanceled = true;
		FEditorDelegates::OnAssetPostImport.Broadcast(this, NULL);
		return NULL;
	}

	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	UObject* NewObject = NULL;

	GLTFMeshBuilder Builder(*UFactory::CurrentFilename);

	bool bShowImportDialog = bShowOption && !GIsAutomationTesting;
	bool bImportAll = false;
	auto ImportOptions = FGLTFLoaderModule::ImportOptions;
	bOutOperationCanceled = bOperationCanceled;

	if (bImportAll)
	{
		// If the user chose to import all, we don't show the dialog again and use the same settings for each object until importing another set of files
		bShowOption = false;
	}

	// For multiple files, use the same settings
	bDetectImportTypeOnImport = false;

	Warn->BeginSlowTask(NSLOCTEXT("GLTFFactory", "BeginImportingGLTFMeshTask", "Importing GLTF mesh"), true);
	if (!Builder.LoadedSuccessfully())
	{
		// Log the error message and fail the import.
		Warn->Log(ELogVerbosity::Error, Builder.GetError());
	}
	else
	{
		// Log the import message and import the mesh.
		const FString errorMessage = Builder.GetError();
		if (errorMessage.Len() > 0)
		{
			Warn->Log(errorMessage);
		}

		FString RootNodeToImport = "";
		RootNodeToImport = Builder.GetRootNode();

		// For animation and static mesh we assume there is at lease one interesting node by default
		int32 InterestingNodeCount = 1;

		bool bImportStaticMeshLODs = /*ImportUI->StaticMeshImportData->bImportMeshLODs*/ false;
		bool bCombineMeshes = /*ImportUI->bCombineMeshes*/ true;

		if (bCombineMeshes && !bImportStaticMeshLODs)
		{
			// If Combine meshes and dont import mesh LODs, the interesting node count should be 1 so all the meshes are grouped together into one static mesh
			InterestingNodeCount = 1;
		}
		else
		{
			// count meshes in lod groups if we dont care about importing LODs
			bool bCountLODGroupMeshes = !bImportStaticMeshLODs;
			int32 NumLODGroups = 0;
			InterestingNodeCount = Builder.GetMeshCount(RootNodeToImport/*, bCountLODGroupMeshes, NumLODGroups*/);

			// if there were LODs in the file, do not combine meshes even if requested
			if (bImportStaticMeshLODs && bCombineMeshes)
			{
				bCombineMeshes = NumLODGroups == 0;
			}
		}

		const FString Filename(UFactory::CurrentFilename);
		if (RootNodeToImport.Len() != 0 && InterestingNodeCount > 0)
		{
			int32 NodeIndex = 0;

			int32 ImportedMeshCount = 0;
			UStaticMesh* NewStaticMesh = NULL;
	
			if (bCombineMeshes)
			{
				auto MeshNames = Builder.GetMeshNames(RootNodeToImport);
				if (MeshNames.Num() > 0)
				{
					NewStaticMesh = Builder.ImportStaticMeshAsSingle(InParent, MeshNames, Name, Flags/*, ImportUI->StaticMeshImportData*/, NULL/*, 0*/);
					for (auto Mesh : MeshNames)
					{
						Warn->Log(FString("Found mesh: ") + Mesh);
					}
				}

				ImportedMeshCount = NewStaticMesh ? 1 : 0;
			}

			NewObject = NewStaticMesh;
		}

		else
		{
			if (RootNodeToImport == "")
			{
				Builder.AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidRoot", "Could not find root node.")), FFbxErrors::SkeletalMesh_InvalidRoot);
			}
			else
			{
				Builder.AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidNode", "Could not find any node.")), FFbxErrors::SkeletalMesh_InvalidNode);
			}
		}
	}

	if (NewObject == NULL)
	{
		// Import fail error message
		Builder.AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoObject", "Import failed.")), FFbxErrors::Generic_ImportingNewObjectFailed);
		Warn->Log(ELogVerbosity::Warning, "Failed to import GLTF mesh");
	}

	Warn->EndSlowTask();

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewObject);

	return NewObject;
}

bool UGLTFFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass());
}

bool UGLTFFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("gltf"))
	{
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE