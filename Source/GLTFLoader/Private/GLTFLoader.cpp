// Some copyright should be here...

#include "GLTFLoaderPrivatePCH.h"

#include "SlateBasics.h"
#include "SlateExtras.h"

#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"

#include "GLTFLoaderStyle.h"
#include "GLTFLoaderCommands.h"
#include "GLTFFactory.h"

#include "LevelEditor.h"

static const FName GLTFLoaderTabName("GLTFLoader");

GLTFImportOptions FGLTFLoaderModule::ImportOptions = GLTFImportOptions::Default();

#define LOCTEXT_NAMESPACE "FGLTFLoaderModule"

void FGLTFLoaderModule::StartupModule()
{	
	FGLTFLoaderStyle::Initialize();
	FGLTFLoaderStyle::ReloadTextures();

	FGLTFLoaderCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FGLTFLoaderCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FGLTFLoaderModule::PluginButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FGLTFLoaderCommands::Get().OpenImportWindow,
		FExecuteAction::CreateRaw(this, &FGLTFLoaderModule::OpenImportWindow),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FGLTFLoaderModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FGLTFLoaderModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GLTFLoaderTabName, FOnSpawnTab::CreateRaw(this, &FGLTFLoaderModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FGLTFLoaderTabTitle", "GLTFLoader"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
		
	UE_LOG(LogInit, Warning, TEXT("GLTFLoader module started successfully."));
}

void FGLTFLoaderModule::ShutdownModule()
{	
	FGLTFLoaderStyle::Shutdown();

	FGLTFLoaderCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GLTFLoaderTabName);
}

TSharedRef<SDockTab> FGLTFLoaderModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TopText", "Import a glTF file"))
			]
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ImportTranslation", "Import Translation"))
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVectorInputBox)
					.X_Raw(this, &FGLTFLoaderModule::GetImportTX)
					.Y_Raw(this, &FGLTFLoaderModule::GetImportTY)
					.Z_Raw(this, &FGLTFLoaderModule::GetImportTZ)
					.bColorAxisLabels(true)
					.AllowResponsiveLayout(true)
					.OnXChanged_Raw(this, &FGLTFLoaderModule::SetImportTX)
					.OnYChanged_Raw(this, &FGLTFLoaderModule::SetImportTY)
					.OnZChanged_Raw(this, &FGLTFLoaderModule::SetImportTZ)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ImportRotation", "Import Rotation"))
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SRotatorInputBox)
					.Pitch_Raw(this, &FGLTFLoaderModule::GetImportRPitch)
					.Yaw_Raw(this, &FGLTFLoaderModule::GetImportRYaw)
					.Roll_Raw(this, &FGLTFLoaderModule::GetImportRRoll)
					.bColorAxisLabels(true)
					.AllowResponsiveLayout(true)
					.OnPitchChanged_Raw(this, &FGLTFLoaderModule::SetImportRPitch)
					.OnYawChanged_Raw(this, &FGLTFLoaderModule::SetImportRYaw)
					.OnRollChanged_Raw(this, &FGLTFLoaderModule::SetImportRRoll)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ImportScale", "Import Scale"))
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Raw(this, &FGLTFLoaderModule::GetImportScale)
					.OnValueChanged_Raw(this, &FGLTFLoaderModule::SetImportScale)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CorrectUp", "Correct Y up to Z up"))
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Raw(this, &FGLTFLoaderModule::GetCorrectUp)
					.OnCheckStateChanged_Raw(this, &FGLTFLoaderModule::SetCorrectUp)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(1.0f)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.OnClicked_Raw(this, &FGLTFLoaderModule::OpenImportWindowDelegateFunc)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImportWindow", "Import File"))
					]
				]
			]
		];

}

void FGLTFLoaderModule::OpenImportWindow()
{
	TArray<FString> Filenames;

	if (FDesktopPlatformModule::Get()->OpenFileDialog(nullptr,
		TEXT("Choose a GLTF file to import"),
		TEXT(""),
		TEXT(""),
		TEXT("GL Transmission Format files (*.gltf)|*.gltf"),
		EFileDialogFlags::None,
		Filenames))
	{
		for (FString File : Filenames)
		{
			UE_LOG(LogTemp, Log, TEXT("File: %s"), *File);
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

		AssetToolsModule.Get().ImportAssets(Filenames, FString("/Game/Content"));
	}
}

void FGLTFLoaderModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(GLTFLoaderTabName);
}

void FGLTFLoaderModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FGLTFLoaderCommands::Get().OpenPluginWindow);
}

void FGLTFLoaderModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FGLTFLoaderCommands::Get().OpenPluginWindow);
}

// Delegate setters
void FGLTFLoaderModule::SetImportTX(float Value)			{ ImportOptions.ImportTranslation.X = Value;	}
void FGLTFLoaderModule::SetImportTY(float Value)			{ ImportOptions.ImportTranslation.Y = Value;	}
void FGLTFLoaderModule::SetImportTZ(float Value)			{ ImportOptions.ImportTranslation.Z = Value;	}
void FGLTFLoaderModule::SetImportRPitch(float Value)		{ ImportOptions.ImportRotation.Pitch = Value;	}
void FGLTFLoaderModule::SetImportRYaw(float Value)			{ ImportOptions.ImportRotation.Yaw = Value;		}
void FGLTFLoaderModule::SetImportRRoll(float Value)			{ ImportOptions.ImportRotation.Roll = Value;	}
void FGLTFLoaderModule::SetImportScale(float Value)			{ ImportOptions.ImportUniformScale = Value;		}
void FGLTFLoaderModule::SetCorrectUp(ECheckBoxState Value)	{ ImportOptions.bCorrectUpDirection = (Value == ECheckBoxState::Checked); }

// Delegate getters
TOptional<float> FGLTFLoaderModule::GetImportTX() const		{ return ImportOptions.ImportTranslation.X;		}
TOptional<float> FGLTFLoaderModule::GetImportTY() const		{ return ImportOptions.ImportTranslation.Y;		}
TOptional<float> FGLTFLoaderModule::GetImportTZ() const		{ return ImportOptions.ImportTranslation.Z;		}
TOptional<float> FGLTFLoaderModule::GetImportRPitch() const	{ return ImportOptions.ImportRotation.Pitch;	}
TOptional<float> FGLTFLoaderModule::GetImportRYaw()	const	{ return ImportOptions.ImportRotation.Yaw;		}
TOptional<float> FGLTFLoaderModule::GetImportRRoll() const	{ return ImportOptions.ImportRotation.Roll;		}
TOptional<float> FGLTFLoaderModule::GetImportScale() const	{ return ImportOptions.ImportUniformScale;		}
ECheckBoxState	 FGLTFLoaderModule::GetCorrectUp() const	{ return ImportOptions.bCorrectUpDirection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGLTFLoaderModule, GLTFLoader)