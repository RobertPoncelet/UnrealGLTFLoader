#include "GLTFLoaderPrivatePCH.h"

#include <locale>
#include <codecvt>

#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf_loader.h"

#include "GLTFMeshBuilder.h"
#include "GLTFLoaderCommands.h"

//#include "Editor/UnrealEd/Classes/Factories/Factory.h"

// These replace TargetPlatform.h since it can't seem to find the right paths from here
#include "ModuleManager.h"
#include "Developer/TargetPlatform/Public/Interfaces/TargetDeviceId.h"
#include "Developer/TargetPlatform/Public/Interfaces/ITargetDevice.h"
#include "Developer/TargetPlatform/Public/Interfaces/ITargetPlatform.h"
#include "Developer/TargetPlatform/Public/Interfaces/ITargetPlatformModule.h"
#include "Developer/TargetPlatform/Public/Interfaces/ITargetPlatformManagerModule.h"

#include "UnrealEd.h"
#include "Developer/RawMesh/Public/RawMesh.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"

#include "Engine.h"
#include "StaticMeshResources.h"
#include "TextureLayout.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Editor/UnrealEd/Classes/Factories/FbxStaticMeshImportData.h"
#include "../Private/GeomFitUtils.h"
#include "FbxErrors.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Polys.h"
#include "PhysicsEngine/BodySetup.h"

/// @cond
// Syntactic sugar to neatly map the TinyGLTF enum to the corresponding data type
// Adapted from http://stackoverflow.com/questions/1735796/is-it-possible-to-choose-a-c-generic-type-parameter-at-runtime
template<int Type> struct GLTFType;
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_BYTE>			{ typedef int8      Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>  { typedef uint8     Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_SHORT>			{ typedef int16     Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	{ typedef uint16    Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_INT>			{ typedef int32     Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	{ typedef uint32    Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_FLOAT>			{ typedef float     Type; };
template<> struct GLTFType <TINYGLTF_COMPONENT_TYPE_DOUBLE>			{ typedef double    Type; };

template<> struct GLTFType <TINYGLTF_TYPE_VEC2>						{ typedef FVector2D Type; };
template<> struct GLTFType <TINYGLTF_TYPE_VEC3>						{ typedef FVector   Type; };
template<> struct GLTFType <TINYGLTF_TYPE_VEC4>						{ typedef FVector4  Type; };
/// @endcond

template <typename T>
bool GLTFMeshBuilder::ConvertAttrib(TArray<T> &OutArray, tinygltf::Mesh* Mesh, std::string AttribName, bool UseWedgeIndices, bool AutoSetArraySize)
{
	if (AttribName != "__WedgeIndices" && !HasAttribute(Mesh, AttribName))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("%s"), *(FText::Format(FText::FromString("Importing data for attribute \"{0}\""), FText::FromString(ToFString(AttribName)))).ToString());

	if (AutoSetArraySize)// This should always be false since for now we are just extending the array each time we want to add an element															
	{																						
		int32 Size = 0;
		if (UseWedgeIndices)																
		{																					
			for (auto Prim : Mesh->primitives)												
			{																				
				Size += GetNumWedges(&Prim); // Number of wedges				
			}
		}																					
		else																				
		{																					
			for (auto Prim : Mesh->primitives)												
			{																				
				Size += Scene->accessors[Prim.attributes.begin()->second].count; // Number of vertices		
			}																				
		}																					

		OutArray.SetNumUninitialized(Size);													
	}				

	// Getting an attribute for individual triangle corners ("wedges")
	else if (UseWedgeIndices && AttribName != "__WedgeIndices")// Make sure we don't try to access indices for the index array itself!
	{
		for (auto Prim : Mesh->primitives)
		{
			std::string IndexAccessorName = Prim.indices;
			std::string AttribAccessorName = Prim.attributes[AttribName];
			tinygltf::Accessor* IndexAccessor = &Scene->accessors[IndexAccessorName];
			tinygltf::Accessor* AttribAccessor = &Scene->accessors[AttribAccessorName];

			if (!IndexAccessor || !AttribAccessor)
			{
				AddTokenizedErrorMessage(
					FTokenizedMessage::Create(
					EMessageSeverity::Warning,
					FText::FromString(FString("Invalid accessor"))),
					FFbxErrors::Generic_Mesh_NoGeometry);
				return false;
			}

			TArray<int32> IndexArray;
			TArray<T> VertArray;

			if (!GetBufferData(IndexArray, IndexAccessor) || !GetBufferData(VertArray, AttribAccessor))
			{
				return false;
			}

			switch (Prim.mode)
			{
				case TINYGLTF_MODE_TRIANGLES:
					for (auto Index : IndexArray)
					{
						OutArray.Add(VertArray[Index]);
					}
					break;

				case TINYGLTF_MODE_TRIANGLE_STRIP:
					OutArray.Add(VertArray[IndexArray[0]]);
					OutArray.Add(VertArray[IndexArray[1]]);
					OutArray.Add(VertArray[IndexArray[2]]);
					for (int i = 2; i < IndexAccessor->count - 2; i += 2)
					{
						// First triangle
						OutArray.Add(VertArray[IndexArray[ i ]]);
						OutArray.Add(VertArray[IndexArray[i-1]]);
						OutArray.Add(VertArray[IndexArray[i+1]]);
						// Second triangle
						OutArray.Add(VertArray[IndexArray[ i ]]);
						OutArray.Add(VertArray[IndexArray[i+1]]);
						OutArray.Add(VertArray[IndexArray[i+2]]);
					}
					break;

				case TINYGLTF_MODE_TRIANGLE_FAN:
					for (int i = 1; i < IndexAccessor->count - 1; ++i)
					{
						// Triangle
						OutArray.Add(VertArray[IndexArray[ 0 ]]);
						OutArray.Add(VertArray[IndexArray[ i ]]);
						OutArray.Add(VertArray[IndexArray[i+1]]);
					}
					break;

				default:
					return false;
			}
		}
	}
	// Getting a vertex attribute
	else
	{
		for (auto Prim : Mesh->primitives)
		{
			std::string AccessorName;
			if (AttribName == "__WedgeIndices")
			{
				AccessorName = Prim.indices;
			}
			else
			{
				AccessorName = Prim.attributes[AttribName];
			}
			if (!GetBufferData(OutArray, &Scene->accessors[AccessorName], true))
			{
				return false;
			}
		}
	}
	return true;																			
}

// Retrieve a value from the buffer, implicitly accounting for endianness
// Adapted from http://stackoverflow.com/questions/13001183/how-to-read-little-endian-integers-from-file-in-c
template <typename T> T GLTFMeshBuilder::BufferValue(void* Data/*, uint8 Size*/)
{
	T Ret = T(0);

	auto NewData = reinterpret_cast<unsigned char*>(Data);
	for (int i = 0; i < sizeof(T); ++i)
	{
		Ret |= (T)(NewData[i]) << (8 * i);
	}
	return Ret;
}

// Use unions for floats and doubles since they don't have a bitwise OR operator
template <> float GLTFMeshBuilder::BufferValue(void* Data)
{
	assert(sizeof(float) == sizeof(int32));

	union
	{
		float Ret;
		int32 IntRet;
	};

	Ret = 0.0f;

	auto NewData = reinterpret_cast<unsigned char*>(Data);
	for (int i = 0; i < sizeof(int32); ++i)
	{
		IntRet |= (int32)(NewData[i]) << (8 * i);
	}
	return Ret;
}

template <> double GLTFMeshBuilder::BufferValue(void* Data)
{
	assert(sizeof(float) == sizeof(int64));

	union
	{
		double Ret;
		int64 IntRet;
	};

	Ret = 0.0;

	auto NewData = reinterpret_cast<unsigned char*>(Data);
	for (int i = 0; i < sizeof(int64); ++i)
	{
		IntRet |= (int64)(NewData[i]) << (8 * i);
	}
	return Ret;
}

/// @cond
struct MaterialPair
{
	tinygltf::Material* GLTFMaterial;
	UMaterialInterface* Material;
};
/// @endcond

GLTFMeshBuilder::GLTFMeshBuilder(FString FilePath)
{
	Loader = new tinygltf::TinyGLTFLoader;
	Scene = new tinygltf::Scene;

	std::string TempError;
	LoadSuccess = Loader->LoadFromFile((*Scene), TempError, ToStdString(FilePath));
	Error = ToFString(TempError);
}

GLTFMeshBuilder::~GLTFMeshBuilder()
{
	delete Loader;
	delete Scene;
}

int32 GLTFMeshBuilder::GetMeshCount(FString NodeName)
{
	return (int32)Scene->nodes[ToStdString(NodeName)].meshes.size();
}

FString GLTFMeshBuilder::GetRootNode()
{
	for (auto ThisNode : Scene->nodes)
	{
		bool ShouldReturn = false;
		for (auto ThatNode : Scene->nodes)
		{
			if (FindInStdVector<std::string>(ThatNode.second.children, ThisNode.first) == -1) // If this node's name doesn't appear in any node's list of children
			{
				ShouldReturn = true;
			}
		}

		if (ShouldReturn)
		{
			return ToFString(ThisNode.first);
		}
	}
	return FString("");
}

tinygltf::Node* GLTFMeshBuilder::GetMeshParentNode(tinygltf::Mesh* InMesh)
{
	for (auto &Node : Scene->nodes)
	{
		for (auto &MeshName : Node.second.meshes)
		{
			if (&Scene->meshes[MeshName] == InMesh)
			{
				return &Node.second;
			}
		}
	}
	return NULL;
}

TArray<FString> GLTFMeshBuilder::GetMeshNames(FString NodeName, bool GetChildren)
{
	TArray<FString> MeshNameArray;

	for (auto Mesh : Scene->nodes[ToStdString(NodeName)].meshes)
	{
		MeshNameArray.Add(ToFString(Mesh));
	}

	if (GetChildren)
	{
		for (auto ChildName : Scene->nodes[ToStdString(NodeName)].children)
		{
			MeshNameArray.Append(GetMeshNames(ToFString(ChildName)));
		}
	}

	return MeshNameArray;
}

UStaticMesh* GLTFMeshBuilder::ImportStaticMeshAsSingle(UObject* InParent, TArray<FString>& MeshNameArray, const FName InName, EObjectFlags Flags, UStaticMesh* InStaticMesh)
{
	auto ImportOptions = FGLTFLoaderModule::ImportOptions;

	int LODIndex = 0;

	bool bBuildStatus = true;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (MeshNameArray.Num() == 0)
	{
		return NULL;
	}

	Parent = InParent;
	
	FString MeshName = ObjectTools::SanitizeObjectName(InName.ToString());

	// Parent package to place new meshes
	UPackage* Package = NULL;

	// create empty mesh
	UStaticMesh* StaticMesh = NULL;

	UStaticMesh* ExistingMesh = NULL;
	UObject* ExistingObject = NULL;

	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;

	FString NewPackageName;

	if( InStaticMesh == NULL || LODIndex == 0 )
	{
		// Create a package for each mesh
		NewPackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) + TEXT("/") + MeshName;
		NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
		Package = CreatePackage(NULL, *NewPackageName);

		ExistingMesh = FindObject<UStaticMesh>( Package, *MeshName );
		ExistingObject = FindObject<UObject>( Package, *MeshName );		
	}

	if (ExistingMesh)
	{
		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingMesh->PreEditChange(NULL);
	}
	else if (ExistingObject)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		bool bDeleteSucceeded = ObjectTools::DeleteSingleObject( ExistingObject );

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Create a package for each mesh
			Package = CreatePackage(NULL, *NewPackageName);

			// Require the parent because it will have been invalidated from the garbage collection
			Parent = Package;
		}
		else
		{
			// failed to delete
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(FText::FromString(FString("{0} wasn't created.\n\nThe asset is referenced by other content.")), FText::FromString(MeshName))), FFbxErrors::Generic_CannotDeleteReferenced);
			return NULL;
		}

	}
	
	if( InStaticMesh != NULL && LODIndex > 0 )
	{
		StaticMesh = InStaticMesh;
	}
	else
	{
		StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), Flags | RF_Public);
	}

	if (StaticMesh->SourceModels.Num() < LODIndex+1)
	{
		// Add one LOD 
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		
		if (StaticMesh->SourceModels.Num() < LODIndex+1)
		{
			LODIndex = StaticMesh->SourceModels.Num() - 1;
		}
	}
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
	if( InStaticMesh != NULL && LODIndex > 0 && !SrcModel.RawMeshBulkData->IsEmpty() )
	{
		// clear out the old mesh data
		FRawMesh EmptyRawMesh;
		SrcModel.RawMeshBulkData->SaveRawMesh( EmptyRawMesh );
	}
	
	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 64;
	StaticMesh->LightMapCoordinateIndex = 1;

	FRawMesh NewRawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(NewRawMesh);

	for (auto Name : MeshNameArray)
	{
		tinygltf::Mesh* Mesh = &Scene->meshes[ToStdString(Name)];

		if (Mesh)
		{
			for (auto Prim : Mesh->primitives)
			{
				MeshMaterials.AddUnique(ToFString(Prim.material));
			}
		}
	}

	for (auto Name : MeshNameArray)
	{
		tinygltf::Mesh* Mesh = &Scene->meshes[ToStdString(Name)];

		if (Mesh)
		{
			if (!BuildStaticMeshFromGeometry(Mesh, StaticMesh, LODIndex, NewRawMesh))
			{
				bBuildStatus = false;
				break;
			}
		}
	}

	// Store the new raw mesh.
	SrcModel.RawMeshBulkData->SaveRawMesh(NewRawMesh);


	if (bBuildStatus)
	{
		// Compress the materials array by removing any duplicates.
		bool bDoRemap = false;
		TArray<int32> MaterialMap;
		TArray<tinygltf::Material*> UniqueMaterials;
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			bool bUnique = true;
			for (int32 OtherMaterialIndex = MaterialIndex - 1; OtherMaterialIndex >= 0; --OtherMaterialIndex)
			{
				if (MeshMaterials[MaterialIndex] == MeshMaterials[OtherMaterialIndex])
				{
					int32 UniqueIndex = MaterialMap[OtherMaterialIndex];

					MaterialMap.Add(UniqueIndex);
					bDoRemap = true;
					bUnique = false;
					break;
				}
			}
			if (bUnique)
			{
				int32 UniqueIndex = UniqueMaterials.Add(&Scene->materials[ToStdString(MeshMaterials[MaterialIndex])]);

				MaterialMap.Add( UniqueIndex );
			}
		}

		if (UniqueMaterials.Num() > 8)
		{
			AddTokenizedErrorMessage(
				FTokenizedMessage::Create(
				EMessageSeverity::Warning,
				FText::Format(FText::FromString(FString("StaticMesh has a large number({1}) of materials and may render inefficently. Consider breaking up the mesh into multiple Static Mesh Assets.")),
				FText::AsNumber(UniqueMaterials.Num())
				)), 
				FFbxErrors::StaticMesh_TooManyMaterials);
		}

		// Sort materials based on _SkinXX in the name.
		TArray<uint32> SortedMaterialIndex;
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			int32 SkinIndex = 0xffff;
			int32 RemappedIndex = MaterialMap[MaterialIndex];
			if (!SortedMaterialIndex.IsValidIndex(RemappedIndex))
			{
				FString GLTFMatName = MeshMaterials[RemappedIndex];

				int32 Offset = GLTFMatName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (Offset != INDEX_NONE)
				{
					// Chop off the material name so we are left with the number in _SKINXX
					FString SkinXXNumber = GLTFMatName.Right(GLTFMatName.Len() - (Offset + 1)).RightChop(4);

					if (SkinXXNumber.IsNumeric())
					{
						SkinIndex = FPlatformString::Atoi( *SkinXXNumber );
						bDoRemap = true;
					}
				}

				SortedMaterialIndex.Add(((uint32)SkinIndex << 16) | ((uint32)RemappedIndex & 0xffff));
			}
		}
		SortedMaterialIndex.Sort();

		TArray<tinygltf::Material*> SortedMaterials;
		for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
		{
			int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
			SortedMaterials.Add(UniqueMaterials[RemappedIndex]);
		}
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialMap.Num(); ++MaterialIndex)
		{
			for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
			{
				int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
				if (MaterialMap[MaterialIndex] == RemappedIndex)
				{
					MaterialMap[MaterialIndex] = SortedIndex;
					break;
				}
			}
		}
		
		// Remap material indices.
		int32 MaxMaterialIndex = 0;
		int32 FirstOpenUVChannel = 1;
		{
			FRawMesh LocalRawMesh;
			SrcModel.RawMeshBulkData->LoadRawMesh(LocalRawMesh);

			if (bDoRemap)
			{
				for (int32 TriIndex = 0; TriIndex < LocalRawMesh.FaceMaterialIndices.Num(); ++TriIndex)
				{
					LocalRawMesh.FaceMaterialIndices[TriIndex] = MaterialMap[LocalRawMesh.FaceMaterialIndices[TriIndex]];
				}
			}

			// Compact material indices so that we won't have any sections with zero triangles.
			LocalRawMesh.CompactMaterialIndices();

			// Also compact the sorted materials array.
			if (LocalRawMesh.MaterialIndexToImportIndex.Num() > 0)
			{
				TArray<tinygltf::Material*> OldSortedMaterials;

				Exchange(OldSortedMaterials,SortedMaterials);
				SortedMaterials.Empty(LocalRawMesh.MaterialIndexToImportIndex.Num());
				for (int32 MaterialIndex = 0; MaterialIndex < LocalRawMesh.MaterialIndexToImportIndex.Num(); ++MaterialIndex)
				{
					tinygltf::Material* Material;
					int32 ImportIndex = LocalRawMesh.MaterialIndexToImportIndex[MaterialIndex];
					if (OldSortedMaterials.IsValidIndex(ImportIndex))
					{
						Material = OldSortedMaterials[ImportIndex];
					}
					SortedMaterials.Add(Material);
				}
			}

			for (int32 TriIndex = 0; TriIndex < LocalRawMesh.FaceMaterialIndices.Num(); ++TriIndex)
			{
				MaxMaterialIndex = FMath::Max<int32>(MaxMaterialIndex,LocalRawMesh.FaceMaterialIndices[TriIndex]);
			}

			for( int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; i++ )
			{
				if( LocalRawMesh.WedgeTexCoords[i].Num() == 0 )
				{
					FirstOpenUVChannel = i;
					break;
				}
			}

			SrcModel.RawMeshBulkData->SaveRawMesh(LocalRawMesh);
		}

		// Setup per-section info and the materials array.
		if (LODIndex == 0)
		{
			StaticMesh->Materials.Empty();
		}
		
		// Build a new map of sections with the unique material set
		FMeshSectionInfoMap NewMap;
		int32 NumMaterials = FMath::Min(SortedMaterials.Num(),MaxMaterialIndex+1);
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, MaterialIndex);

			int32 Index = StaticMesh->Materials.Add(ToUMaterial(SortedMaterials[MaterialIndex]));

			Info.MaterialIndex = Index;
			NewMap.Set( LODIndex, MaterialIndex, Info);
		}

		// Copy the final section map into the static mesh
		StaticMesh->SectionInfoMap.Clear();
		StaticMesh->SectionInfoMap.CopyFrom(NewMap);

		FRawMesh LocalRawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(LocalRawMesh);

		// Setup default LOD settings based on the selected LOD group.
		if (ExistingMesh == NULL && LODIndex == 0)
		{
			ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(CurrentPlatform);
			const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(ImportOptions.StaticMeshLODGroup);
			int32 NumLODs = LODGroup.GetDefaultNumLODs();
			while (StaticMesh->SourceModels.Num() < NumLODs)
			{
				new (StaticMesh->SourceModels) FStaticMeshSourceModel();
			}
			for (int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
			{
				StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
			}
			StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
		}

		// @todo This overrides restored values currently but we need to be able to import over the existing settings if the user chose to do so.
		SrcModel.BuildSettings.bRemoveDegenerates = ImportOptions.bRemoveDegenerates;
		SrcModel.BuildSettings.bBuildAdjacencyBuffer = ImportOptions.bBuildAdjacencyBuffer;
		SrcModel.BuildSettings.bRecomputeNormals = LocalRawMesh.WedgeTangentZ.Num() == 0;
		SrcModel.BuildSettings.bRecomputeTangents = LocalRawMesh.WedgeTangentX.Num() == 0 || LocalRawMesh.WedgeTangentY.Num() == 0;
		SrcModel.BuildSettings.bUseMikkTSpace = false;
		if( ImportOptions.bGenerateLightmapUVs )
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.DstLightmapIndex = FirstOpenUVChannel;
			StaticMesh->LightMapCoordinateIndex = FirstOpenUVChannel;
		}
		else
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
		}

		TArray<FText> BuildErrors;
		StaticMesh->LODGroup = ImportOptions.StaticMeshLODGroup;
		StaticMesh->Build(false, &BuildErrors);

		for( FText& Error : BuildErrors )
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, Error), FFbxErrors::StaticMesh_BuildError );
		}
		
		// this is damage control. After build, we'd like to absolutely sure that 
		// all index is pointing correctly and they're all used. Otherwise we remove them
		FMeshSectionInfoMap OldSectionInfoMap = StaticMesh->SectionInfoMap;
		StaticMesh->SectionInfoMap.Clear();
		// fix up section data
		for (int32 LODResourceIndex = 0; LODResourceIndex < StaticMesh->RenderData->LODResources.Num(); ++LODResourceIndex)
		{
			FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODResourceIndex];
			int32 NumSections = LOD.Sections.Num();
			for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				FMeshSectionInfo Info = OldSectionInfoMap.Get(LODResourceIndex, SectionIndex);
				if (StaticMesh->Materials.IsValidIndex(Info.MaterialIndex))
				{
					StaticMesh->SectionInfoMap.Set(LODResourceIndex, SectionIndex, Info);
				}
			}
		}
	}
	else
	{
		StaticMesh = NULL;
	}

	return StaticMesh;
}

// Reverse the winding order for triangle indices
template <typename T>
void GLTFMeshBuilder::ReverseTriDirection(TArray<T>& OutArray)
{
	for (int i = 0; i < OutArray.Num() - 2; i += 3)
	{
		T Temp = OutArray[i];
		OutArray[i] = OutArray[i + 2];
		OutArray[i + 2] = Temp;
	}
}

bool GLTFMeshBuilder::BuildStaticMeshFromGeometry(tinygltf::Mesh* Mesh, UStaticMesh* StaticMesh, int LODIndex, FRawMesh& RawMesh)
{
	check(StaticMesh->SourceModels.IsValidIndex(LODIndex));

	auto ImportOptions = FGLTFLoaderModule::ImportOptions;

	tinygltf::Node* Node = GetMeshParentNode(Mesh);
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];

	tinygltf::Primitive* BaseLayer = &Mesh->primitives[0];
	if (BaseLayer == NULL)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(FText::FromString(FString("Error_NoGeometryInMesh", "There is no geometry information in mesh '{0}'")), FText::FromString(ToFString(Mesh->name)))), FFbxErrors::Generic_Mesh_NoGeometry);
		return false;
	}

	//
	// create materials
	//

	TArray<tinygltf::Material*> FoundMaterials;
	for (auto Prim : Mesh->primitives)
	{
		tinygltf::Material* CurrentMaterial = &Scene->materials[Prim.material];
		FoundMaterials.AddUnique(CurrentMaterial);
	}

	TArray<UMaterialInterface*> Materials;
	if (ImportOptions.bImportMaterials)
	{
		Materials.Add(UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));
	}
	else if (ImportOptions.bImportTextures)
	{
		ImportTexturesFromNode(Node);
	}

	// Used later to offset the material indices on the raw triangle data
	int32 MaterialIndexOffset = MeshMaterials.Num();

	for (int32 MaterialIndex = 0; MaterialIndex < FoundMaterials.Num(); MaterialIndex++)
	{
		MaterialPair NewMaterialPair;// = new(MeshMaterials)FFbxMaterial;
		tinygltf::Material* GLTFMaterial = FoundMaterials[MaterialIndex];
		NewMaterialPair.GLTFMaterial = GLTFMaterial;
		if (ImportOptions.bImportMaterials)
		{
			NewMaterialPair.Material = Materials[MaterialIndex];
		}
		else
		{
			FString MaterialFullName = ObjectTools::SanitizeObjectName(ToFString(GLTFMaterial->name));
			FString BasePackageName = PackageTools::SanitizePackageName(FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName()) / MaterialFullName);
			UMaterialInterface* UnrealMaterialInterface = FindObject<UMaterialInterface>(NULL, *(BasePackageName + TEXT(".") + MaterialFullName));
			if (UnrealMaterialInterface == NULL)
			{
				UnrealMaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			NewMaterialPair.Material = UnrealMaterialInterface;
		}
	}

	if (FoundMaterials.Num() == 0)
	{
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);
		MaterialPair NewMaterial;
		NewMaterial.Material = DefaultMaterial;
		NewMaterial.GLTFMaterial = NULL;
		FoundMaterials.AddDefaulted(1);
	}

	// Smoothing is already included in the format
	bool bSmoothingAvailable = true;

	// TODO: Collisions
	//
	// build collision
	//
	bool bImportedCollision = false;

	bool bEnableCollision = bImportedCollision || (/*GBuildStaticMeshCollision*/true && LODIndex == 0 && ImportOptions.bRemoveDegenerates);
	for (int32 SectionIndex = MaterialIndexOffset; SectionIndex<MaterialIndexOffset + FoundMaterials.Num(); SectionIndex++)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
		Info.bEnableCollision = bEnableCollision;
		StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	}

	//
	// build un-mesh triangles
	//

	// Construct the matrices for the conversion from right handed to left handed system
	FMatrix TotalMatrix;
	FMatrix TotalMatrixForNormal;
	TotalMatrix = GetNodeTransform(GetMeshParentNode(Mesh));
	FTransform ImportTransform(ImportOptions.ImportRotation.Quaternion(), ImportOptions.ImportTranslation, FVector(ImportOptions.ImportUniformScale));
	FMatrix ImportMatrix = ImportTransform.ToMatrixWithScale();
	TotalMatrix = TotalMatrix * ImportMatrix;

	if (ImportOptions.bCorrectUpDirection)
	{
		FTransform Temp(FRotator(0.0f, 0.0f, -90.0f));
		TotalMatrix = TotalMatrix * Temp.ToMatrixWithScale();
	}

	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.GetTransposed();	

	// Whether an odd number of axes have negative scale
	bool OddNegativeScale = (TotalMatrix.M[0][0] * TotalMatrix.M[1][1] * TotalMatrix.M[2][2]) < 0;

	// Copy the actual data!
	// Vertex Positions
	TArray<FVector> NewVertexPositions;
	if (!ConvertAttrib(NewVertexPositions, Mesh, std::string("POSITION"), false))
	{
		AddTokenizedErrorMessage(
			FTokenizedMessage::Create(
			EMessageSeverity::Error,
			FText::FromString(FString("Could not obtain position data."))),
			FFbxErrors::Generic_Mesh_LOD_InvalidIndex);
		return false;
	}
	for (auto& Vertex : NewVertexPositions)
	{
		FVector4 Vert4(Vertex);
		Vertex = TotalMatrix.TransformFVector4(Vert4);
	}
	int32 VertexCount = NewVertexPositions.Num();

	// Triangle indices
	TArray<int32> NewWedgeIndices;
	ConvertAttrib(NewWedgeIndices, Mesh, std::string("__WedgeIndices"));
	int32 WedgeCount = NewWedgeIndices.Num();
	int32 TriangleCount = WedgeCount / 3;
	if (TriangleCount == 0)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(FText::FromString(FString("Error_NoTrianglesFoundInMesh", "No triangles were found on mesh '{0}'")), FText::FromString(ToFString(Mesh->name)))), FFbxErrors::StaticMesh_NoTriangles);
		return false;
	}

	// Normals
	TArray<FVector> NewWedgeTangentX, NewWedgeTangentY, NewWedgeTangentZ;
	bool HasNormals = ConvertAttrib(NewWedgeTangentZ, Mesh, "NORMAL");
	ConvertAttrib(NewWedgeTangentY, Mesh, "TANGENT");
	ConvertAttrib(NewWedgeTangentX, Mesh, "BINORMAL");
	if (!HasNormals)
	{
		AddTokenizedErrorMessage(
			FTokenizedMessage::Create(
			EMessageSeverity::Warning,
			FText::FromString(FString("Could not obtain data for normals; they will be recalculated but the model will lack smoothing data."))),
			FFbxErrors::Generic_Mesh_LOD_InvalidIndex);
	}
	for (auto& Normal : NewWedgeTangentZ)
	{
		Normal = TotalMatrixForNormal.TransformVector(Normal);
	}

	// UVs
	TArray<FVector2D> NewWedgeTexCoords[MAX_MESH_TEXTURE_COORDS];
	bool bHasUVs = false;
	for (int i = 0; i < MAX_MESH_TEXTURE_COORDS; ++i)
	{
		bHasUVs |= ConvertAttrib(NewWedgeTexCoords[i], Mesh, std::string("TEXCOORD_") + std::to_string(i));
	}
	if (!bHasUVs)
	{
		AddTokenizedErrorMessage(
			FTokenizedMessage::Create(
			EMessageSeverity::Warning,
			FText::FromString(FString("Could not obtain UV data."))),
			FFbxErrors::Generic_Mesh_LOD_InvalidIndex);
	}

	TArray<FColor> NewWedgeColors;
	ConvertAttrib(NewWedgeColors, Mesh, std::string("COLOR")); // No need to check for errors

	// Reverse the triangle winding order since glTF uses the opposite to Unreal
	// Except if the model has negative scale on an odd number of axes, which will effectively do it for us
	if (!OddNegativeScale)
	{
		ReverseTriDirection(NewWedgeIndices);
		ReverseTriDirection(NewWedgeColors);
		ReverseTriDirection(NewWedgeTangentX);
		ReverseTriDirection(NewWedgeTangentY);
		ReverseTriDirection(NewWedgeTangentZ);
		for (int i = 0; i < MAX_MESH_TEXTURE_COORDS; ++i)
		{
			ReverseTriDirection(NewWedgeTexCoords[i]);
		}
	}

	TArray<int32> NewFaceMaterialIndices;
	GetMaterialIndices(NewFaceMaterialIndices, (*Mesh));

	TArray<int32> NewFaceSmoothingMasks; // Don't need to do anything with this since smoothing information is included implicitly in glTF

	// Force attribute arrays to the correct size, otherwise it complains
	NewFaceMaterialIndices.SetNumZeroed(TriangleCount);
	NewFaceSmoothingMasks.SetNumZeroed(TriangleCount);
	NewWedgeColors.SetNumZeroed(WedgeCount);
	NewWedgeTangentX.SetNumZeroed(WedgeCount);
	NewWedgeTangentY.SetNumZeroed(WedgeCount);
	NewWedgeTangentZ.SetNumZeroed(WedgeCount);
	for (int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; ++i)
	{
		NewWedgeTexCoords[i].SetNumZeroed(WedgeCount);
	}

	// Add the new data to the raw mesh
	RawMesh.VertexPositions.Append(NewVertexPositions);
	RawMesh.WedgeIndices.Append(NewWedgeIndices);
	RawMesh.FaceMaterialIndices.Append(NewFaceMaterialIndices);
	RawMesh.FaceSmoothingMasks.Append(NewFaceSmoothingMasks);
	RawMesh.WedgeColors.Append(NewWedgeColors);
	RawMesh.WedgeTangentX.Append(NewWedgeTangentX);
	RawMesh.WedgeTangentY.Append(NewWedgeTangentY);
	RawMesh.WedgeTangentZ.Append(NewWedgeTangentZ);
	for (int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; ++i)
	{
		RawMesh.WedgeTexCoords[i].Append(NewWedgeTexCoords[i]);
	}

	return true;
}

UMaterialInterface* GLTFMeshBuilder::ToUMaterial(tinygltf::Material* Material) 
{ 
	return UMaterial::GetDefaultMaterial(MD_Surface); 
}

FString GLTFMeshBuilder::GetError()
{
	return Error;
}

FString GLTFMeshBuilder::ToFString(std::string InString)
{
	return FString(InString.c_str());
}

std::string GLTFMeshBuilder::ToStdString(FString InString)
{
	auto CharArray = InString.GetCharArray();
	
	std::wstring WideString(&CharArray[0]);
	std::wstring_convert< std::codecvt_utf8<wchar_t> > Convert;
	return Convert.to_bytes(WideString);
}

template <typename T>
bool GLTFMeshBuilder::GetBufferData(TArray<T> &OutArray, tinygltf::Accessor* Accessor, bool Append)
{
	if (Accessor->type != TINYGLTF_TYPE_SCALAR)
	{
		return false;
	}

	if (!Accessor)
	{
		return false;
	}

	tinygltf::BufferView* BufferView = &Scene->bufferViews[Accessor->bufferView];

	if (!BufferView)
	{
		return false;
	}

	tinygltf::Buffer* Buffer = &Scene->buffers[BufferView->buffer];

	if (!Buffer)
	{
		return false;
	}

	if (!Append)
	{
		OutArray.Empty();
	}

	size_t Stride;
	if (Accessor->byteStride != 0)
	{
		Stride = Accessor->byteStride;
	}
	else
	{
		Stride = TypeSize(Accessor->componentType);
	}

	unsigned char* Start = &Buffer->data[Accessor->byteOffset + BufferView->byteOffset];

	switch (Accessor->componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>			::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>	::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>			::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_INT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_INT>			::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>			::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:			BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>			::Type, T>(OutArray, Start, Accessor->count, Stride);	break;
		default:										BufferCopy<T,														T>(OutArray, Start, Accessor->count, Stride);	break;
	}

	return true;
}

// This specialization is almost the same, but checks for the VEC2 GLTF type and uses the overloaded BufferCopy function for FVector2D
template <>
bool GLTFMeshBuilder::GetBufferData(TArray<FVector2D> &OutArray, tinygltf::Accessor* Accessor, bool Append)
{
	if (Accessor->type != TINYGLTF_TYPE_VEC2)
	{
		return false;
	}

	if (!Accessor)
	{
		return false;
	}

	tinygltf::BufferView* BufferView = &Scene->bufferViews[Accessor->bufferView];

	if (!BufferView)
	{
		return false;
	}

	tinygltf::Buffer* Buffer = &Scene->buffers[BufferView->buffer];

	if (!Buffer)
	{
		return false;
	}

	if (!Append)
	{
		OutArray.Empty();
	}

	size_t Stride;
	if (Accessor->byteStride != 0)
	{
		Stride = Accessor->byteStride;
	}
	else
	{
		Stride = TypeSize(Accessor->componentType);
	}

	unsigned char* Start = &Buffer->data[Accessor->byteOffset + BufferView->byteOffset];

	switch (Accessor->componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_INT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_INT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:			BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		default:										return false;
	}

	return true;
}

// This specialization is almost the same, but checks for the VEC3 GLTF type and uses the overloaded BufferCopy function for FVector
template <>
bool GLTFMeshBuilder::GetBufferData(TArray<FVector> &OutArray, tinygltf::Accessor* Accessor, bool Append)
{
	if (Accessor->type != TINYGLTF_TYPE_VEC3)
	{
		return false;
	}

	if (!Accessor)
	{
		return false;
	}

	tinygltf::BufferView* BufferView = &Scene->bufferViews[Accessor->bufferView];

	if (!BufferView)
	{
		return false;
	}

	tinygltf::Buffer* Buffer = &Scene->buffers[BufferView->buffer];

	if (!Buffer)
	{
		return false;
	}

	if (!Append)
	{
		OutArray.Empty();
	}

	size_t Stride;
	if (Accessor->byteStride != 0)
	{
		Stride = Accessor->byteStride;
	}
	else
	{
		Stride = TypeSize(Accessor->componentType);
	}

	unsigned char* Start = &Buffer->data[Accessor->byteOffset + BufferView->byteOffset];

	switch (Accessor->componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_INT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_INT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:			BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		default:										return false;
	}

	return true;
}

// This specialization is almost the same, but checks for the VEC4 GLTF type and uses the overloaded BufferCopy function for FVector4
template <>
bool GLTFMeshBuilder::GetBufferData(TArray<FVector4> &OutArray, tinygltf::Accessor* Accessor, bool Append)
{
	if (Accessor->type != TINYGLTF_TYPE_VEC4)
	{
		return false;
	}

	if (!Accessor)
	{
		return false;
	}

	tinygltf::BufferView* BufferView = &Scene->bufferViews[Accessor->bufferView];

	if (!BufferView)
	{
		return false;
	}

	tinygltf::Buffer* Buffer = &Scene->buffers[BufferView->buffer];

	if (!Buffer)
	{
		return false;
	}

	if (!Append)
	{
		OutArray.Empty();
	}

	size_t Stride;
	if (Accessor->byteStride != 0)
	{
		Stride = Accessor->byteStride;
	}
	else
	{
		Stride = TypeSize(Accessor->componentType);
	}

	unsigned char* Start = &Buffer->data[Accessor->byteOffset + BufferView->byteOffset];

	switch (Accessor->componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_INT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_INT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:			BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>			::Type>(OutArray, Start, Accessor->count, Stride);	break;
		default:										return false;
	}

	return true;
}

// This specialization is almost the same, but checks for a VEC3 or VEC4 GLTF type and uses the overloaded BufferCopy function for FColor
template <>
bool GLTFMeshBuilder::GetBufferData(TArray<FColor> &OutArray, tinygltf::Accessor* Accessor, bool Append)
{
	if (Accessor->type != TINYGLTF_TYPE_VEC3 && Accessor->type != TINYGLTF_TYPE_VEC4)
	{
		return false;
	}

	if (!Accessor)
	{
		return false;
	}

	tinygltf::BufferView* BufferView = &Scene->bufferViews[Accessor->bufferView];

	if (!BufferView)
	{
		return false;
	}

	tinygltf::Buffer* Buffer = &Scene->buffers[BufferView->buffer];

	if (!Buffer)
	{
		return false;
	}

	if (!Append)
	{
		OutArray.Empty();
	}

	size_t Stride;
	if (Accessor->byteStride != 0)
	{
		Stride = Accessor->byteStride;
	}
	else
	{
		Stride = TypeSize(Accessor->componentType);
	}

	unsigned char* Start = &Buffer->data[Accessor->byteOffset + BufferView->byteOffset];

	switch (Accessor->componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>			::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE>	::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>			::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT>	::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_INT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_INT>			::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:		BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT>	::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:				BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>			::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:			BufferCopy<GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>			::Type>(OutArray, Accessor->type, Start, Accessor->count, Stride);	break;
		default:										return false;
	}

	return true;
}

bool GLTFMeshBuilder::HasAttribute(tinygltf::Mesh* Mesh, std::string AttribName) const
{
	for (auto Prim : Mesh->primitives)
	{
		for (auto Entry : Prim.attributes)
		{
			if (Entry.first == AttribName)
			{
				return true;
			}
		}
	}
	return false;
}

template <typename T> int32 GLTFMeshBuilder::FindInStdVector(const std::vector<T> &InVector, const T &InElement) const
{
	for (int32 i = 0; i < InVector.size(); ++i)
	{
		if (InVector[i] == InElement)
		{
			return i;
		}
	}

	return -1;
}

FMatrix GLTFMeshBuilder::GetNodeTransform(tinygltf::Node* Node)
{
	if (Node->matrix.size() == 16)
	{
		FMatrix Ret;
		for (int32 i = 0; i < 4; ++i)
		{
			for (int32 j = 0; j < 4; ++j)
			{
				// Reverse order since glTF is column major and FMatrix is row major
				Ret.M[j][i] = Node->matrix[(4 * i) + j];
			}
		}
		return Ret;
	}
	else if (Node->rotation.size() == 4 && Node->scale.size() == 3 && Node->translation.size() == 3)
	{
		FQuat Rotation((float)Node->rotation[0], (float)Node->rotation[1], (float)Node->rotation[2], (float)Node->rotation[3]);
		FVector Scale((float)Node->scale[0], (float)Node->scale[1], (float)Node->scale[2]);
		FVector Translation((float)Node->translation[0], (float)Node->translation[1], (float)Node->translation[2]);
		return FTransform(Rotation, Translation, Scale).ToMatrixWithScale();
	}
	else
	{
		return FMatrix::Identity;
	}
}

size_t GLTFMeshBuilder::TypeSize(int Type) const
{
	switch (Type)
	{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return sizeof(GLTFType<TINYGLTF_COMPONENT_TYPE_BYTE>::Type);

		case TINYGLTF_COMPONENT_TYPE_SHORT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return sizeof(GLTFType<TINYGLTF_COMPONENT_TYPE_SHORT>::Type);

		case TINYGLTF_COMPONENT_TYPE_INT:
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return sizeof(GLTFType<TINYGLTF_COMPONENT_TYPE_INT>::Type);

		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return sizeof(GLTFType<TINYGLTF_COMPONENT_TYPE_FLOAT>::Type);

		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
			return sizeof(GLTFType<TINYGLTF_COMPONENT_TYPE_DOUBLE>::Type);

		default:
			return -1;
	}
}

int32 GLTFMeshBuilder::GetNumWedges(tinygltf::Primitive* Prim) const
{
	switch (Prim->mode)
	{
		case TINYGLTF_MODE_TRIANGLES:
			return Prim->indices.size();

		case TINYGLTF_MODE_TRIANGLE_STRIP:
		case TINYGLTF_MODE_TRIANGLE_FAN:
			return (Prim->indices.size() - 2) * 3;

		default:
			return 0;
	}
}

void GLTFMeshBuilder::GetMaterialIndices(TArray<int32>& OutArray, tinygltf::Mesh& Mesh)
{
	for (auto Prim : Mesh.primitives)
	{
		int32 Index = MeshMaterials.Find(ToFString(Prim.material));
		for (int i = 0; i < GetNumWedges(&Prim) / 3; ++i)
		{
			OutArray.Add(Index);
		}
	}
}

template <typename SrcType, typename DstType> void GLTFMeshBuilder::BufferCopy(TArray<DstType>& OutArray, unsigned char* Data, int32 Count, size_t Stride)
{
	for (int32 i = 0; i < Count; ++i)
	{
		// At this point, assume we can cast directly to the destination type
		OutArray.Add((DstType)BufferValue<SrcType>(Data));
		Data += Stride;
	}
}

template <typename SrcType> void GLTFMeshBuilder::BufferCopy(TArray<FVector2D> &OutArray, unsigned char* Data, int32 Count, size_t Stride)
{
	for (int32 i = 0; i < Count; ++i)
	{
		// At this point, assume we can cast directly to the destination type
		OutArray.Add(FVector2D(BufferValue<SrcType>(Data), BufferValue<SrcType>(Data + sizeof(SrcType))));
		Data += Stride;
	}
}

template <typename SrcType> void GLTFMeshBuilder::BufferCopy(TArray<FVector> &OutArray, unsigned char* Data, int32 Count, size_t Stride)
{
	for (int32 i = 0; i < Count; ++i)
	{
		// At this point, assume we can cast directly to the destination type
		OutArray.Add(FVector(BufferValue<SrcType>(Data), BufferValue<SrcType>(Data + sizeof(SrcType)), BufferValue<SrcType>(Data + 2 * sizeof(SrcType))));
		Data += Stride;
	}
}

template <typename SrcType> void GLTFMeshBuilder::BufferCopy(TArray<FVector4> &OutArray, unsigned char* Data, int32 Count, size_t Stride)
{
	for (int32 i = 0; i < Count; ++i)
	{
		// At this point, assume we can cast directly to the destination type
		OutArray.Add(FVector4(BufferValue<SrcType>(Data), BufferValue<SrcType>(Data + sizeof(SrcType)), BufferValue<SrcType>(Data + 2 * sizeof(SrcType)), BufferValue<SrcType>(Data + 3 * sizeof(SrcType))));
		Data += Stride;
	}
}

template <typename SrcType> void GLTFMeshBuilder::BufferCopy(TArray<FColor> &OutArray, int InType, unsigned char* Data, int32 Count, size_t Stride)
{
	switch (InType)
	{
		case TINYGLTF_TYPE_VEC3:
			for (int32 i = 0; i < Count; ++i)
			{
				// At this point, assume we can cast directly to the destination type
				OutArray.Add(FColor(BufferValue<SrcType>(Data), BufferValue<SrcType>(Data + sizeof(SrcType)), BufferValue<SrcType>(Data + 2 * sizeof(SrcType))));
				Data += Stride;
			}
			break;
		case TINYGLTF_TYPE_VEC4:
			for (int32 i = 0; i < Count; ++i)
			{
				// At this point, assume we can cast directly to the destination type
				OutArray.Add(FColor(BufferValue<SrcType>(Data), BufferValue<SrcType>(Data + sizeof(SrcType)), BufferValue<SrcType>(Data + 2 * sizeof(SrcType)), BufferValue<SrcType>(Data + 3 * sizeof(SrcType))));
				Data += Stride;
			}
			break;
		default: return;
	}
}

void GLTFMeshBuilder::AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName ErrorName)
{
	UE_LOG(LogTemp, Warning, TEXT("%s"), *(Error->ToText().ToString()));
}