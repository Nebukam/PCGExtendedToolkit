// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExStagedTypeFilterDetails.h"
#include "Helpers/PCGExCollectionsHelpers.h"

#include "PCGExStagingTypeFilter.generated.h"

UENUM()
enum class EPCGExStagedTypeFilterMode : uint8
{
	Include = 0 UMETA(DisplayName = "Include", ToolTip="Keep points that match selected types"),
	Exclude = 1 UMETA(DisplayName = "Exclude", ToolTip="Remove points that match selected types"),
};


/**
 * Filters staged points by their collection entry type.
 * Useful when mixing different collection types through Asset Staging with per-point collections.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(Keywords = "filter type staged collection", PCGExNodeLibraryDoc="assets-management/staged-type-filter"))
class UPCGExStagedTypeFilterSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(StagedTypeFilter, "Staging : Type Filter", "Filters staged points by their collection entry type.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Filter); }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExStagedTypeFilterMode FilterMode = EPCGExStagedTypeFilterMode::Include;

	/** Type configuration - populated from collection type registry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStagedTypeFilterDetails TypeConfig;

	/** If enabled, output filtered-out points to a separate pin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOutputDiscarded = false;
};

struct FPCGExStagedTypeFilterContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExStagedTypeFilterElement;

	TSharedPtr<PCGExCollections::FPickUnpacker> CollectionUnpacker;

	TSharedPtr<PCGExData::FPointIOCollection> FilteredOutCollection;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExStagedTypeFilterElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(StagedTypeFilter)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExStagedTypeFilter
{
	const FName OutputFilteredOut = TEXT("Discarded");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExStagedTypeFilterContext, UPCGExStagedTypeFilterSettings>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashGetter;
		TArray<int8> Mask;
		int32 NumKept = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
