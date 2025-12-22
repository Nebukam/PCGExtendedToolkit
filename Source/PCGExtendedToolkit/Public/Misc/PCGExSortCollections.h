// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Core/PCGExPointsProcessor.h"
#include "Sorting/PCGExSortingCommon.h"
#include "PCGExSortCollections.generated.h"

namespace PCGExSorting
{
	class FSorter;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/reduce-data"))
class UPCGExSortCollectionsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SortCollections, "Sort Collections", "Sort collection using @Data domain attributes.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscWrite); }
#endif

	virtual bool HasDynamicPins() const override;

	virtual FName GetMainInputPin() const override { return PCGPinConstants::DefaultInputLabel; }
	virtual FName GetMainOutputPin() const override { return PCGPinConstants::DefaultOutputLabel; }

protected:
	virtual bool IsInputless() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Controls the order in which data will be sorted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
};

struct FPCGExSortCollectionsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSortCollectionsElement;
	TArray<FPCGTaggedData> Datas;
	TSharedPtr<PCGExSorting::FSorter> Sorter;
};

class FPCGExSortCollectionsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SortCollections)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
