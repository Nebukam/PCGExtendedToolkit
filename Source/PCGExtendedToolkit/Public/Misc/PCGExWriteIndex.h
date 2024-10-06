// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExWriteIndex.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWriteIndexSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		WriteIndex, "Write Index", "Write the current point index to an attribute.",
		OutputAttributeName);
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Whther to write the index as a normalized output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOutputNormalizedIndex = false;

	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OutputAttributeName = "CurrentIndex";

	/** Whether to write the collection index as an attribute (note this will be a default value attribute, so memory-friendly).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputCollectionIndex = false;

	/** The name of the attribute to write the collection index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputCollectionIndex"))
	FName CollectionIndexAttributeName = "CollectionIndex";
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteIndexContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteIndexElement final : public FPCGExPointsProcessorElement
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

namespace PCGExWriteIndex
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWriteIndexContext, UPCGExWriteIndexSettings>
	{
		double NumPoints = 0;
		TSharedPtr<PCGExData::TBuffer<int32>> IntWriter;
		TSharedPtr<PCGExData::TBuffer<double>> DoubleWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
