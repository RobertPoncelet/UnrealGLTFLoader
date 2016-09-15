/// @file GLTFLoader.h by Robert Poncelet

#pragma once

#include "ModuleManager.h"
#include "SlateBasics.h"
#include "GLTFImportOptions.h"

class FToolBarBuilder;
class FMenuBuilder;

///
/// \brief The top-level class in the plugin module. 
///
/// This class handles the UI and the corresponding actions to set off the importing 
/// process. Functions/members provided as boilerplate by Unreal's plugin creation 
/// wizard are marked with \"<B>(Boilerplate)</B>\".
///
class FGLTFLoaderModule : public IModuleInterface
{
public:
	///@name <B>(Boilerplate)</B> IModuleInterface implementation.
	///@{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	///@}
	
	/// <B>(Boilerplate)</B> This function will be bound to a TCommand to bring up the plugin window.
	void PluginButtonClicked();

	/// Opens the file browser for the user to select a file to import.
	void OpenImportWindow();

	/// Since this class doesn't instantiate the Factory directly, the import options are provided statically and publicly so that GLTFFactory can access them itself.
	static GLTFImportOptions ImportOptions;
	
private:

	/// Boilerplate
	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	/// Bound to the import button's OnClicked event which expects an FReply; OpenImportWindow() itself is used for FGLTFLoaderCommands which expects a void.
	FReply OpenImportWindowDelegateFunc() { OpenImportWindow(); return FReply::Handled(); }

	/// @name UI Setters
	///@{
	/// These functions are bound to the UI elements when they are created and called to update the options' data when the user modifies the values.
	void SetImportTX		(float Value);
	void SetImportTY		(float Value);
	void SetImportTZ		(float Value);
	void SetImportRPitch	(float Value);
	void SetImportRYaw		(float Value);
	void SetImportRRoll		(float Value);
	void SetImportScale		(float Value);
	void SetCorrectUp		(ECheckBoxState Value);
	///@}

	/// @name UI Getters
	///@{
	/// These functions are bound to the UI elements when they are created and called to validate the displayed UI values once the options' data is updated.
	TOptional<float> GetImportTX()		const;
	TOptional<float> GetImportTY()		const;
	TOptional<float> GetImportTZ()		const;
	TOptional<float> GetImportRPitch()	const;
	TOptional<float> GetImportRYaw()	const;
	TOptional<float> GetImportRRoll()	const;
	TOptional<float> GetImportScale()	const;
	ECheckBoxState	 GetCorrectUp()		const;
	///@}

	/// <B>(Boilerplate)</B> Brings up the main plugin window.
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:

	/// <B>(Boilerplate)</B> Used to link with FGLTFLoaderCommands.
	TSharedPtr<class FUICommandList> PluginCommands;
};