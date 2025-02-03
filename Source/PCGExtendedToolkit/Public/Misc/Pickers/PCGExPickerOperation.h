// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExOperation.h"
#include "PCGExPicker.h"
#include "Paths/PCGExPaths.h"
#include "PCGExPickerOperation.generated.h"

class UPCGExPickerFactoryData;
/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	TObjectPtr<const UPCGExPickerFactoryData> Factory = nullptr;
	FPCGExPickerConfigBase BaseConfig;

	virtual bool Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory);
	virtual void AddPicks(const TSharedRef<PCGExData::FFacade>& InDataFacade, TSet<int32>& OutPicks) const;

	virtual void Cleanup() override
	{
		Super::Cleanup();
		Factory = nullptr;
	}
};

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerPointOperation : public UPCGExPickerOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory) override;
};
