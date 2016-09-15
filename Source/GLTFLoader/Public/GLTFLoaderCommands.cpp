// Some copyright should be here...

#include "GLTFLoaderPrivatePCH.h"
#include "GLTFLoaderCommands.h"

#define LOCTEXT_NAMESPACE "FGLTFLoaderModule"

void FGLTFLoaderCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "GLTFLoader", "Bring up GLTFLoader window", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(OpenImportWindow, "GLTF Import Window", "Choose a GLTF file to import", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
