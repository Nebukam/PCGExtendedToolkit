// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExElement.h"
#include "Core/PCGExSettings.h"
#include "PCGExCoreMacros.h"
#include "PCGExCoreSettingsCache.h"

#include "PCGExBitmask.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="TBD"))
class UPCGExBitmaskSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExBitmaskElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;

	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(Bitmask, "Bitmask", "A Simple bitmask attribute.", GetDisplayName());
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Constant); }
	FName GetDisplayName() const;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Operations executed on the flag if all filters pass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExBitmask Bitmask;

	/** Limit of characters for the title */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	int32 TitleCharLimit = 32;
};

class FPCGExBitmaskElement final : public IPCGExElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
