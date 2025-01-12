// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Paths/PCGExPaths.h"
#include "Sampling/PCGExSampling.h"
#include "Tensors/PCGExTensor.h"


#include "PCGExTensorsTransform.generated.h"

#define PCGEX_FOREACH_FIELD_TRTENSOR(MACRO)\
MACRO(EffectorsPings, int32, 0)\
MACRO(UpdateCount, int32, 0)\
MACRO(TraveledDistance, double, 0)\
MACRO(GracefullyStopped, bool, false)\
MACRO(MaxIterationsReached, bool, false)

UENUM()
enum class EPCGExTensorTransformMode : uint8
{
	Absolute = 0 UMETA(DisplayName = "Absolute", ToolTip="Absolute, ignores source transform."),
	Relative = 1 UMETA(DisplayName = "Relative", ToolTip="Relative to source transform."),
	Align    = 2 UMETA(DisplayName = "Align", ToolTip="Align rotation with movement direction."),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorsTransformSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorsTransform, "Tensors Transform", "Transform input points using tensors.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformPosition = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformRotation = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTransformRotation"))
	EPCGExTensorTransformMode Rotation = EPCGExTensorTransformMode::Align;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTransformRotation && Rotation == EPCGExTensorTransformMode::Align"))
	EPCGExAxis AlignAxis = EPCGExAxis::Forward;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 Iterations = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEffectorsPings = false;

	/** Name of the 'int32' attribute to write the total number of effectors that affected the transform, all iterations combined. This is hardly a measure of anything, but it's an interesting value nonetheless */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Effectors Pings", PCG_Overridable, EditCondition="bWriteEffectorsPings"))
	FName EffectorsPingsAttributeName = FName("EffectorsPings");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteUpdateCount = false;

	/** Name of the 'int32' attribute to write the number of iterations that affected the point before it stopped. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Update Count", PCG_Overridable, EditCondition="bWriteUpdateCount"))
	FName UpdateCountAttributeName = FName("UpdateCount");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTraveledDistance = false;

	/** Name of the 'double' attribute to write the approximative distance travelled by this point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Traveled Distance", PCG_Overridable, EditCondition="bWriteTraveledDistance"))
	FName TraveledDistanceAttributeName = FName("TraveledDistance");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteGracefullyStopped = false;

	/** Name of the 'bool' attribute to tag the point with if transform stopped before the maximum number of iterations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Gracefully Stopped", PCG_Overridable, EditCondition="bWriteGracefullyStopped"))
	FName GracefullyStoppedAttributeName = FName("GracefullyStopped");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteMaxIterationsReached = false;

	/** Name of the 'bool' attribute to tag the point with if it has reached the max number of iterations set. Faster alternative to comparing multiple attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Max Iterations Reached", PCG_Overridable, EditCondition="bWriteMaxIterationsReached"))
	FName MaxIterationsReachedAttributeName = FName("MaxIterationsReached");

private:
	friend class FPCGExTensorsTransformElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorsTransformContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExTensorsTransformElement;
	TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

	PCGEX_FOREACH_FIELD_TRTENSOR(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorsTransformElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExTensorsTransform
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExTensorsTransformContext, UPCGExTensorsTransformSettings>
	{
		bool bIteratedOnce = false;
		int32 RemainingIterations = 0;
		TArray<PCGExPaths::FPathMetrics> Metrics;
		TArray<int32> Pings;

		PCGEX_FOREACH_FIELD_TRTENSOR(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
