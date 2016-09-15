/// @file GLTFLoaderCommands.h by Robert Poncelet

#pragma once

#include "SlateBasics.h"
#include "GLTFLoaderStyle.h"

/// Boilerplate class provided by Unreal.
class FGLTFLoaderCommands : public TCommands<FGLTFLoaderCommands>
{
public:

	FGLTFLoaderCommands()
		: TCommands<FGLTFLoaderCommands>(TEXT("GLTFLoader"), NSLOCTEXT("Contexts", "GLTFLoader", "GLTFLoader Plugin"), NAME_None, FGLTFLoaderStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
	TSharedPtr< FUICommandInfo > OpenImportWindow;
};