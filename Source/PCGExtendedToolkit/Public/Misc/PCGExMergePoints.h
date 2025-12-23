// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Sorting/PCGExSortingDetails.h"


#include "PCGExMergePoints.generated.h"

class FPCGExPointIOMerger;
// Hidden for now because buggy, concurrent writing occurs and I don't know why; need to look into it
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/merge-points"))
class UPCGExMergePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergePoints, "Merge Points", "An alternative to the native Merge Points node with additional controls.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Misc); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	//~End UPCGExPointsProcessorSettings

	/** Sorting settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Collection Sorting"))
	FPCGExCollectionSortingDetails SortingDetails;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/** If enabled, will convert tags into attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTagToAttributes = false;

	/** Tags that will be converted to attributes. Simple tags will be converted to boolean values, other supported formats are int32, double, FString, and FVector 2-3-4. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTagToAttributes"))
	FPCGExNameFiltersDetails TagsToAttributes = FPCGExNameFiltersDetails(false);

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTagOverlapWarning = false;
};

struct FPCGExMergePointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMergePointsElement;
	FPCGExCollectionSortingDetails SortingDetails;
	FPCGExCarryOverDetails CarryOverDetails;
	FPCGExNameFiltersDetails TagsToAttributes;
	TSharedPtr<PCGExData::FFacade> CompositeDataFacade;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExMergePointsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(MergePoints)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExMergePoints
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExMergePointsContext, UPCGExMergePointsSettings>
	{
	protected:
		FRWLock SimpleTagsLock;
		TSet<FName> SimpleTags;

		double NumPoints = 0;

	public:
		PCGExMT::FScope OutScope;
		TSharedPtr<TSet<FName>> ConvertedTags;
		TArray<FName> ConvertedTagsList;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;
	};

	class FBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
		TSharedPtr<FPCGExPointIOMerger> Merger;
		TSharedPtr<TSet<FName>> ConvertedTags;
		TSet<FName> IgnoredAttributes;

	public:
		explicit FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection);
		virtual bool PrepareSingle(const TSharedRef<PCGExPointsMT::IProcessor>& InProcessor) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual void CompleteWork() override;

	protected:
		void StartMerge();
	};
}
