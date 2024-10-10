// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExCollocationCount.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCollocationCountSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CollocationCount, "Collocation Count", "Write the number of time a point shares its location with another.",
		CollicationNumAttributeName);
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** The name of the attribute to write collocation to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName CollicationNumAttributeName = "NumCollocations";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLinearOccurences = false;

	/** The name of the attribute to write linear occurences to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteLinearOccurences"))
	FName LinearOccurencesAttributeName = "NumLinearOccurences";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.01))
	double Tolerance = 0.01;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCollocationCountContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExCollocationCountElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCollocationCountElement final : public FPCGExPointsProcessorElement
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

namespace PCGExCollocationCount
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExCollocationCountContext, UPCGExCollocationCountSettings>
	{
		double NumPoints = 0;
		double ToleranceConstant = 0;
		TSharedPtr<PCGExData::TBuffer<int32>> CollocationWriter;
		TSharedPtr<PCGExData::TBuffer<int32>> LinearOccurencesWriter;

		const UPCGPointData::PointOctree* Octree = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
