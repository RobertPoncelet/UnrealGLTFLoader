// Some copyright should be here...

using UnrealBuildTool;

public class GLTFLoader : ModuleRules
{
	public GLTFLoader(TargetInfo Target)
	{
		
		PublicIncludePaths.AddRange(
			new string[] {
				"GLTFLoader/Public"
				
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"GLTFLoader/Private",
				
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine",
                "UnrealEd",
                "RawMesh",
				"RenderCore", // For FPackedNormal
				"MaterialUtilities",
                "MeshUtilities",
                "AssetTools"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"CoreUObject", 
                "Engine", 
                "Slate", 
                "SlateCore",
                "RawMesh",
				"RenderCore", // For FPackedNormal
				"MaterialUtilities",
                "MeshUtilities",
                "AssetTools",
				"PropertyEditor",
                "EditorStyle",
                "EditorWidgets"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
