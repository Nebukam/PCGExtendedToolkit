// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Transform/PCGExTensorsTransform.h"
#include "Transform/PCGExTransform.h"
#include "Transform/Tensors/PCGExTensor.h"

#include "PCGExExtrudeTensors.generated.h"

UENUM()
enum class EPCGExOutOfBoundPathPointHandling : uint8
{
	Exclude        = 0 UMETA(DisplayName = "Exclude", Tooltip="Ignore the out-of-bound sample and don't add it to the path."),
	Include        = 1 UMETA(DisplayName = "Include", Tooltip="Include the out-of-bound sample to the path."),
	IncludeAndSnap = 2 UMETA(Hidden, DisplayName = "Snap", Tooltip="Include the out-of-bound sample and snap it"),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExExtrudeTensorsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ExtrudeTensors, "Path : Extrude Tensors", "Extrude input points into paths along tensors.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainInputPin() const override;
	virtual FName GetMainOutputPin() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformRotation = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTransformRotation"))
	EPCGExTensorTransformMode Rotation = EPCGExTensorTransformMode::Align;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTransformRotation && Rotation == EPCGExTensorTransformMode::Align"))
	EPCGExAxis AlignAxis = EPCGExAxis::Forward;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bUsePerPointMaxIterations = false;

	/** Per-point Max Iterations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Per-point Iterations", EditCondition="bUsePerPointMaxIterations"))
	FName IterationsAttribute = FName("Iterations");

	/** Max Iterations. If using per-point max, this will act as a clamping mechanism. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Iterations", ClampMin=1))
	int32 Iterations = 1;

	/** Whether to adjust max iteration based on max value found on points. Use at your own risks! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Use Max from Points", ClampMin=1, EditCondition="bUsePerPointMaxIterations", HideEditConditionToggle))
	bool bUseMaxFromPoints = false;

	/** Whether the node should attempt to close loops based on angle and proximity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Closing Loops", meta=(PCG_NotOverridable))
	bool bSearchForClosedLoops = false;

	/** Range at which the first point must be located to check angle */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Closing Loops", meta=(PCG_Overridable, DisplayName=" └─ Search Distance", EditCondition="bCloseLoops"))
	double ClosedLoopSearchDistance = 100;

	/** Angle at which the loop will be closed, if within range */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Closing Loops", meta=(PCG_Overridable, DisplayName=" └─ Search Angle", EditCondition="bCloseLoops", Units="Degrees", ClampMin=0, ClampMax=90))
	double ClosedLoopSearchAngle = 11.25;
	
	/** Whether to limit the length of the generated path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bUseMaxLength = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable, EditCondition="bUseMaxLength", EditConditionHides))
	EPCGExInputValueType MaxLengthInput = EPCGExInputValueType::Constant;

	/** Max length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Length", EditCondition="bUseMaxLength && MaxLengthInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxLengthAttribute = FName("MaxLength");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Length", EditCondition="bUseMaxLength && MaxLengthInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxLength = 100;


	/** Whether to limit the number of points in a generated path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bUseMaxPointsCount = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable, EditCondition="bUseMaxPointsCount", EditConditionHides))
	EPCGExInputValueType MaxPointsCountInput = EPCGExInputValueType::Constant;

	/** Max length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count", EditCondition="bUseMaxPointsCount && MaxPointsCountInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxPointsCountAttribute = FName("MaxPointsCount");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count", EditCondition="bUseMaxPointsCount && MaxPointsCountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxPointsCount = 100;


	/** Whether to limit path length or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, ClampMin=0.001))
	double FuseDistance = 0.01;

	/** How to deal with out-of-bound samples */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	EPCGExOutOfBoundPathPointHandling OutOfBoundHandling = EPCGExOutOfBoundPathPointHandling::Exclude;

	/** Whether to stop sampling when going out of bounds. While path will be cut, there's a chance that the head of the search comes back into the bounds, which would start a new extrusion. With this option disabled, new paths won't be permitted to exist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	bool bAllowNewExtrusions = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails AttributesToPathTags;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfClosedLoop = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(PCG_Overridable, EditCondition="bTagIfClosedLoop"))
	FString IsClosedLoopTag = TEXT("ClosedLoop");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfOpenPath = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(PCG_Overridable, EditCondition="bTagIfOpenPath"))
	FString IsOpenPathTag = TEXT("OpenPath");

private:
	friend class FPCGExExtrudeTensorsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExExtrudeTensorsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExExtrudeTensorsElement;
	TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;
	FBox Limits = FBox(ForceInit);
	double ClosedLoopSquaredDistance = 0;
	double ClosedLoopSearchDot = 0;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExExtrudeTensorsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExExtrudeTensors
{
	class FProcessor;

	class FExtrusion : public TSharedFromThis<FExtrusion>
	{
	protected:
		TArray<FPCGPoint>& ExtrudedPoints;
		double DistToLastSum = 0;

		bool bIsExtruding = false;
		bool bIsComplete = false;
		bool bIsStopped = false;
		bool bIsClosedLoop = false;

		FPCGPoint Origin;

	public:
		FProcessor* Processor = nullptr;
		const FPCGExExtrudeTensorsContext* Context = nullptr;
		const UPCGExExtrudeTensorsSettings* Settings = nullptr;

		FVector Head = FVector::ZeroVector;

		int32 SeedIndex = -1;
		int32 RemainingIterations = 0;
		double MaxLength = MAX_dbl;
		int32 MaxPointCount = MAX_int32;

		PCGExPaths::FPathMetrics Metrics;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TSharedPtr<PCGEx::FIndexedItemOctree> EdgeOctree;
		const TArray<TSharedPtr<FExtrusion>>* Extrusions = nullptr;

		FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations);

		void SetOriginPosition(const FVector& InPosition);

		bool Advance();
		bool Extrude(const PCGExTensor::FTensorSample& Sample);
		void Insert(const PCGExTensor::FTensorSample& Sample, const FVector& InPosition) const;
		void Complete();
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExExtrudeTensorsContext, UPCGExExtrudeTensorsSettings>
	{
		FRWLock NewExtrusionLock;
		int32 RemainingIterations = 0;

		TSharedPtr<PCGExData::TBuffer<int32>> PerPointIterations;
		TSharedPtr<PCGExData::TBuffer<int32>> PerPointMaxPoints;
		TSharedPtr<PCGExData::TBuffer<double>> PerPointMaxLength;

		FPCGExAttributeToTagDetails AttributesToPathTags;
		TArray<TSharedPtr<FExtrusion>> ExtrusionQueue;
		TArray<TSharedPtr<FExtrusion>> NewExtrusions;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		void PrepareExtrusion(const TSharedPtr<FExtrusion>& InExtrusion);
		void InitExtrusionFromSeed(const int32 InSeedIndex);
		void InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion);

		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		bool UpdateExtrusionQueue();

		void CompleteWork() override;
	};
}
