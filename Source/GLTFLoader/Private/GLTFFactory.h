/// @file GLTFFactory.h by Robert Poncelet

#pragma once

#include "Factories/Factory.h"
#include "GLTFFactory.generated.h"

UCLASS()
/// The class instantiated by the AssetToolsModule for importing the chosen UAsset into the content browser. Adapted from UFbxFactory.
class UGLTFFactory : public UFactory
{
	GENERATED_BODY()

	UGLTFFactory(const FObjectInitializer& ObjectInitializer);

	/// @name UFactory Implementation
	///@{
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	///@}

	bool bShowOption;
	bool bDetectImportTypeOnImport;

	/** true if the import operation was canceled. */
	bool bOperationCanceled;
	
};
