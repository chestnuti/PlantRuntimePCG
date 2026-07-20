#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "AEParameterBundleFactory.generated.h"

/* Imports and atomically reimports strict Published Parameter Bundle v1 JSON files. */
UCLASS()
class ADAPTIVEENVEDITOR_API UAEParameterBundleFactory final : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	/* Configures the factory for editor-only .aeparams.json imports. */
	UAEParameterBundleFactory();
	/* Returns whether the filename uses the exact published bundle suffix. */
	virtual bool FactoryCanImport(const FString& Filename) override;
	/* Parses, validates, and creates one bundle asset without partial mutation. */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	/* Returns the single source filename used to create one bundle asset. */
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	/* Updates the source filename associated with one bundle asset. */
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	/* Reimports through a temporary validated candidate before overwriting the asset. */
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
};
