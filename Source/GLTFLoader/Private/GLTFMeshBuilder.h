/// @file GLTFMeshBuilder.h by Robert Poncelet

#pragma once

#include "UnrealString.h"
#include "TokenizedMessage.h"
#include "GLTFImportOptions.h"

#include <string>
#include <vector>

class UStaticMesh;
class UMaterialInterface;
struct FRawMesh;

/// Forward-declared TinyGLTF types since its header can only be #included in one source file.
/// This also means that we must use pointers to these types outside of GLTFMeshBuilder.cpp.
namespace tinygltf
{
	class TinyGLTFLoader;
	class Scene;
	class Node;
	struct ACCESSOR;
	typedef struct ACCESSOR Accessor;
	struct PRIMITIVE;
	typedef struct PRIMITIVE Primitive;
	struct MESH;
	typedef struct MESH Mesh;
	struct MATERIAL;
	typedef struct MATERIAL Material;
}

/// Works in conjunction with TinyGLTF and Unreal's Static Mesh build system to return a UStaticMesh to the factory. This class is adapted from FbxImporter.
class GLTFMeshBuilder
{
public:
	GLTFMeshBuilder(FString FilePath);
	~GLTFMeshBuilder();

	/// Returns whether we have a valid glTF scene loaded up. For a new MeshBuilder, this should always be queried before calling other functions.
	bool LoadedSuccessfully() { return LoadSuccess; }
	
	/// Returns the number of meshes owned by a given node.
	int32 GetMeshCount(FString NodeName);
	
	/// Returns the name of the scene's root node.
	FString GetRootNode();
	
	/// Obtains the mesh names of a node (and optionally its children); useful as an argument to <B>ImportStaticMeshAsSingle()</B>.
	TArray<FString> GetMeshNames(FString NodeName, bool GetChildren = true);
	
	/// Organises materials and builds the StaticMesh using a RawMesh filled with data using <B>BuildStaticMeshFromGeometry()</B>.
	/// This function mirrors that in FFbxImporter of the same name.
	/// @param InParent A pointer provided by the system importing the file (i.e. probably AssetImportModule) so we know where the saved package goes.
	/// @param MeshNameArray An array of strings used as keys to obtain the actual meshes from the glTF scene.
	/// @param InName The name of the package to be saved.
	/// @param Flags Metadata used for the creation of the new package.
	/// @param InStaticMesh A pointer to the StaticMesh to be built and have this new geometry added to it.
	UStaticMesh* ImportStaticMeshAsSingle(UObject* InParent, TArray<FString>& MeshNameArray, const FName InName, EObjectFlags Flags, UStaticMesh* InStaticMesh);
	
	/// Obtains the geometry data from the file and adds it to the RawMesh ready to be built for the StaticMesh.
	/// This function mirrors that in FFbxImporter of the same name.
	/// @param Mesh The glTF mesh to grab the data from.
	/// @param StaticMesh The asset the mesh will be built for. This will eventually be the object serialised and saved in the import process.
	/// @param MeshMaterials An array of materials to convert to those used by the engine and send to the build process.
	/// @param LODIndex Level of detail for this mesh - currently unused i.e. always 0.
	/// @param RawMesh The intermediate container for the data between the external format (glTF in our case) and the built StaticMesh.
	bool BuildStaticMeshFromGeometry(tinygltf::Mesh* Mesh, UStaticMesh* StaticMesh, int LODIndex, FRawMesh& RawMesh);
	
	/// Material/texture system does nothing currently.
	UMaterialInterface* ToUMaterial(tinygltf::Material* Material);
	void CreateUnrealMaterial(tinygltf::Material* Material, TArray<UMaterialInterface*>& OutMaterials) { return; };
	int32 CreateNodeMaterials(tinygltf::Node* Node, TArray<UMaterialInterface*>& OutMaterials) { return 0; };
	void ImportTexturesFromNode(tinygltf::Node* Node) { return; }
	
	/// Logs an error message.
	void AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName ErrorName);
	/// Returns the error message left by TinyGLTFLoader, if any.
	FString GetError();

private:
	// Templated data copy functions, from highest to lowest level:

	/// @name Level 4: ConvertAttrib
	///@{
	/// Fills a TArray with a particular vertex attribute. Set InAttribName to "__WedgeIndex" to use the "indices" accessor for each primitive, or "__MaterialIndices" to obtain the material index.
	/// @param OutArray The array to fill with data from the imported file.
	/// @param Mesh The glTF from which to convert the specified attribute.
	/// @param AttribName The name of the attribute to convert.
	/// @param UseWedgeIndices Whether to copy data for each triangle corner ("wedge") or each vertex.
	/// @param AutoSetArraySize Whether to resize the array to the number of elements in this attribute (usually false since we may be adding to data from another mesh).
	template <typename T> bool ConvertAttrib(TArray<T> &OutArray, tinygltf::Mesh* Mesh, std::string AttribName, bool UseWedgeIndices = true, bool AutoSetArraySize = false);
	///@}

	/// @name Level 3: GetBufferData
	///@{
	/// Fills a TArray with typed data; works at the glTF Accessor level and figures out which arguments to send to <B>BufferCopy()</B>.
	/// @param OutArray The array to fill with data from the imported file.
	/// @param Accessor A pointer to a glTF Accessor containing the type data for the geometry attribute.
	/// @param Append Whether to add to the array or overwrite the elements currently in it.
	template <typename T>	bool GetBufferData				(TArray<T>			&OutArray, tinygltf::Accessor* Accessor, bool Append = true);
	template <>				bool GetBufferData<FVector2D>	(TArray<FVector2D>	&OutArray, tinygltf::Accessor* Accessor, bool Append	   );
	template <>				bool GetBufferData<FVector>		(TArray<FVector>	&OutArray, tinygltf::Accessor* Accessor, bool Append	   );
	template <>				bool GetBufferData<FVector4>	(TArray<FVector4>	&OutArray, tinygltf::Accessor* Accessor, bool Append	   );
	///@}

	/// @name Level 2: BufferCopy
	///@{
	/// Handles filling the TArray at the data type level.
	/// @param OutArray The array to fill with data from the imported file.
	/// @param Data A pointer to the raw data to use as the argument to <B>BufferValue()</B>.
	/// @param Count The number of elements to add to the array i.e. the number of calls to <B>BufferValue()</B>.
	/// @param Stride The number of bytes between the first byte of each element - usually the size of one element.
	template <typename SrcType, typename DstType>	void BufferCopy(TArray<DstType>		&OutArray, unsigned char* Data, int32 Count, size_t Stride);
	template <typename SrcType>						void BufferCopy(TArray<FVector2D>	&OutArray, unsigned char* Data, int32 Count, size_t Stride);
	template <typename SrcType>						void BufferCopy(TArray<FVector>		&OutArray, unsigned char* Data, int32 Count, size_t Stride);
	template <typename SrcType>						void BufferCopy(TArray<FVector4>	&OutArray, unsigned char* Data, int32 Count, size_t Stride);
	template <typename SrcType>						void BufferCopy(TArray<FColor>		&OutArray, int Type, unsigned char* Data, int32 Count, size_t Stride);
	///@}

	/// @name Level 1: BufferValue
	///@{
	/// Obtains a single value from the geometry data buffer, accounting for endianness.
	/// Adapted from http://stackoverflow.com/questions/13001183/how-to-read-little-endian-integers-from-file-in-c
	/// @param Data A pointer to the raw data to cast to the desired type.
	/// @return The typed data value.
	template <typename T> T BufferValue(void* Data);
	///@}

	/// Separate function to obtain material indices since it is not stored as a buffer. Should be called after MeshMaterials has been filled in.
	void GetMaterialIndices(TArray<int32>& OutArray, tinygltf::Mesh& Mesh);
	
	// Miscellaneous helper functions

	/// Reverses the order of every group of 3 elements.
	template<typename T> void ReverseTriDirection(TArray<T>& OutArray);
	/// Whether a mesh's geometry has a specified attribute.
	bool HasAttribute(tinygltf::Mesh* Mesh, std::string AttribName) const;
	/// Similar to TArray's Find() function; returns the array index if the specified object was found, -1 otherwise.
	template <typename T> int32 FindInStdVector(const std::vector<T> &InVector, const T &InElement) const;
	/// Returns the transform of a node relative to its parent.
	FMatrix GetNodeTransform(tinygltf::Node* Node);
	/// Returns the size of the C++ data type given the corresponding glTF type.
	size_t TypeSize(int Type) const;
	/// Returns the number of triangle corners given a glTF primitive, taking into account its draw mode.
	int32 GetNumWedges(tinygltf::Primitive* Prim) const;
	/// Returns the owning node of a given mesh.
	tinygltf::Node* GetMeshParentNode(tinygltf::Mesh* InMesh);

	/// @name String Conversion
	///@{
	/// Helper functions for converting between Unreal's and STL's strings.
	static FString ToFString(std::string InString);
	static std::string ToStdString(FString InString);
	///@}

	/// 
	TWeakObjectPtr<UObject> Parent;
	tinygltf::TinyGLTFLoader* Loader;
	tinygltf::Scene* Scene;
	TArray<FString> MeshMaterials;
	bool LoadSuccess;
	FString Error;
};

