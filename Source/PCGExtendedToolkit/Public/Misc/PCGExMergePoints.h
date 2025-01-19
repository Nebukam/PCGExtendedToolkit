// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExPointIOMerger.h"


#include "PCGExMergePoints.generated.h"

// Hidden for now because buggy, concurrent writing occurs and I don't know why; need to look into it
UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMergePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergePoints, "Merge Points", "An alternative to the native Merge Points node with additional controls.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;

	//~End UPCGExPointsProcessorSettings

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/** If enabled, will convert tags into attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTagToAttributes = false;

	/** Tags that will be converted to attributes. Simple tags will be converted to boolean values, other supported formats are int32, double, FString, and FVector 2-3-4. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTagToAttributes"))
	FPCGExNameFiltersDetails TagsToAttributes = FPCGExNameFiltersDetails(false);
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergePointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMergePointsElement;
	FPCGExCarryOverDetails CarryOverDetails;
	FPCGExNameFiltersDetails TagsToAttributes;
	TSharedPtr<PCGExData::FFacade> CompositeDataFacade;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergePointsElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExMergePoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExMergePointsContext, UPCGExMergePointsSettings>
	{
		double NumPoints = 0;

	public:
		PCGExMT::FScope OutScope;
		TSharedPtr<TSet<FName>> ConvertedTags;
		TArray<FName> ConvertedTagsList;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool IsTrivial() const override { return false; }
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
	};

	class FBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
		TSharedPtr<FPCGExPointIOMerger> Merger;
		TSharedPtr<TSet<FName>> ConvertedTags;
		TSet<FName> IgnoredAttributes;

	public:
		explicit FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection);
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& PointsProcessor) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual void Write() override;

	protected:
		void StartMerge();
	};
}
