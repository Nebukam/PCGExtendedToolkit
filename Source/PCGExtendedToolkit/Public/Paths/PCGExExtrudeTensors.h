// Copyright 2025 Timothé Lapetite and contributors
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
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"
#include "Transform/Tensors/PCGExTensorHandler.h"

#include "PCGExExtrudeTensors.generated.h"

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

	/** Whether to give a new seed to the points. If disabled, they will inherit the original one. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bRefreshSeed = true;

	/** Whether the node should attempt to close loops based on angle and proximity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Closing Loops", meta=(PCG_NotOverridable))
	bool bDetectClosedLoops = false;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Length (Attr)", EditCondition="bUseMaxLength && MaxLengthInput!=EPCGExInputValueType::Constant", EditConditionHides))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count (Attr)", EditCondition="bUseMaxPointsCount && MaxPointsCountInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxPointsCountAttribute = FName("MaxPointsCount");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count", EditCondition="bUseMaxPointsCount && MaxPointsCountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxPointsCount = 100;


	/** Whether to limit path length or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, ClampMin=0.001))
	double FuseDistance = 0.01;

	/** How to deal with points that are stopped */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	EPCGExTensorStopConditionHandling StopConditionHandling = EPCGExTensorStopConditionHandling::Exclude;

	/** Whether to stop sampling when extrusion is stopped. While path will be cut, there's a chance that the head of the search comes back into non-stopping conditions, which would start a new extrusion. With this option disabled, new paths won't be permitted to exist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	bool bAllowChildExtrusions = false;

	/** If enabled, seeds that start stopped won't be extruded at all. Otherwise, they are transformed until they eventually reach a point that's outside stopping conditions and start an extrusion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bIgnoreStoppedSeeds = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails AttributesToPathTags;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfChildExtrusion = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfChildExtrusion"))
	FString IsChildExtrusionTag = TEXT("ChildExtrusion");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsStoppedByFilters = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsStoppedByFilters"))
	FString IsStoppedByFiltersTag = TEXT("StoppedByFilters");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsFollowUp = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsFollowUp"))
	FString IsFollowUpTag = TEXT("IsFollowUp");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfClosedLoop = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfClosedLoop"))
	FString IsClosedLoopTag = TEXT("ClosedLoop");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfOpenPath = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfOpenPath"))
	FString IsOpenPathTag = TEXT("OpenPath");

	/** Tensor sampling settings. Note that these are applied on the flattened sample, e.g after & on top of individual tensors' mutations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Tensor Sampling Settings"))
	FPCGExTensorHandlerDetails TensorHandlerDetails;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietMissingTensorError = false;

private:
	friend class FPCGExExtrudeTensorsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExExtrudeTensorsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExExtrudeTensorsElement;

	TArray<TObjectPtr<const UPCGExTensorFactoryData>> TensorFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> StopFilterFactories;

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
	enum class EExtrusionFlags : uint32
	{
		None           = 0,
		Bounded        = 1 << 0,
		ClosedLoop     = 1 << 1,
		AllowsChildren = 1 << 2,
		CollisionCheck = 1 << 3,
	};

	constexpr bool Supports(const EExtrusionFlags Flags, EExtrusionFlags Flag) { return (static_cast<uint32>(Flags) & static_cast<uint32>(Flag)) != 0; }

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
		bool bHitStopFilters = false;

		FPCGPoint Origin;

	public:
		bool bIsChildExtrusion = false;
		bool bIsFollowUp = false;

		virtual ~FExtrusion() = default;
		FProcessor* Processor = nullptr;
		const FPCGExExtrudeTensorsContext* Context = nullptr;
		const UPCGExExtrudeTensorsSettings* Settings = nullptr;
		TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;
		TSharedPtr<PCGExPointFilter::FManager> StopFilters;

		FTransform Head = FTransform::Identity;

		int32 SeedIndex = -1;
		int32 RemainingIterations = 0;
		double MaxLength = MAX_dbl;
		int32 MaxPointCount = MAX_int32;

		PCGExPaths::FPathMetrics Metrics;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TSharedPtr<PCGEx::FIndexedItemOctree> EdgeOctree;
		const TArray<TSharedPtr<FExtrusion>>* Extrusions = nullptr;

		FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations);

		void SetHead(const FTransform& InHead);

		virtual bool Advance() = 0;
		void Complete();

	protected:
		bool OnAdvanced(const bool bStop);
		bool Extrude(const PCGExTensor::FTensorSample& Sample, const FPCGPoint& InPoint);
		void StartNewExtrusion();
		void Insert(const FPCGPoint& InPoint) const;
	};

	template <EExtrusionFlags InternalFlags>
	class TExtrusion : public FExtrusion
	{
	public:
		TExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations):
			FExtrusion(InSeedIndex, InFacade, InMaxIterations)
		{
		}

		virtual bool Advance() override
		{
			if (bIsStopped) { return false; }

			const FVector PreviousHeadLocation = Head.GetLocation();
			bool bSuccess = false;
			const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(Head, bSuccess);

			if (!bSuccess) { return OnAdvanced(true); }

			// Apply sample to head

			if (Settings->bTransformRotation)
			{
				if (Settings->Rotation == EPCGExTensorTransformMode::Absolute)
				{
					Head.SetRotation(Sample.Rotation);
				}
				else if (Settings->Rotation == EPCGExTensorTransformMode::Relative)
				{
					Head.SetRotation(Head.GetRotation() * Sample.Rotation);
				}
				else if (Settings->Rotation == EPCGExTensorTransformMode::Align)
				{
					Head.SetRotation(PCGExMath::MakeDirection(Settings->AlignAxis, Sample.DirectionAndSize.GetSafeNormal() * -1, Head.GetRotation().GetUpVector()));
				}
			}

			const FVector HeadLocation = PreviousHeadLocation + Sample.DirectionAndSize;
			Head.SetLocation(HeadLocation);

			if constexpr (Supports(InternalFlags, EExtrusionFlags::ClosedLoop))
			{
				if (const FVector Tail = Origin.Transform.GetLocation();
					FVector::DistSquared(Metrics.Last, Tail) <= Context->ClosedLoopSquaredDistance &&
					FVector::DotProduct(Sample.DirectionAndSize.GetSafeNormal(), (Tail - PreviousHeadLocation).GetSafeNormal()) > Context->ClosedLoopSearchDot)
				{
					bIsClosedLoop = true;
					return OnAdvanced(true);
				}
			}

			FPCGPoint HeadPoint = ExtrudedPoints.Last();
			HeadPoint.Transform = Head;

			if constexpr (Supports(InternalFlags, EExtrusionFlags::Bounded))
			{
				if (StopFilters->Test(HeadPoint))
				{
					if (bIsExtruding && !bIsComplete)
					{
						bHitStopFilters = true;
						if (Settings->StopConditionHandling == EPCGExTensorStopConditionHandling::Include) { Insert(HeadPoint); }
						//else if (Settings->OutOfBoundHandling == EPCGExOutOfBoundPathPointHandling::Exclude){}
						Complete();

						if constexpr (!Supports(InternalFlags, EExtrusionFlags::AllowsChildren))
						{
							return OnAdvanced(true);
						}
					}

					return OnAdvanced(false);
				}

				if (bIsComplete)
				{
					if constexpr (Supports(InternalFlags, EExtrusionFlags::AllowsChildren))
					{
						StartNewExtrusion();
					}
					return OnAdvanced(true);
				}

				if (!bIsExtruding)
				{
					// Start writing path
					bIsExtruding = true;
					Metrics = PCGExPaths::FPathMetrics(PreviousHeadLocation);
					ExtrudedPoints[0].Transform = Head;
					return OnAdvanced(false);
				}
			}

			return OnAdvanced(!Extrude(Sample, HeadPoint));
		}
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExExtrudeTensorsContext, UPCGExExtrudeTensorsSettings>
	{
		FRWLock NewExtrusionLock;
		int32 RemainingIterations = 0;

		TSharedPtr<PCGExData::TBuffer<int32>> PerPointIterations;
		TSharedPtr<PCGExData::TBuffer<int32>> PerPointMaxPoints;
		TSharedPtr<PCGExData::TBuffer<double>> PerPointMaxLength;

		TSharedPtr<PCGExPointFilter::FManager> StopFilters;
		TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

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

		void InitExtrusionFromSeed(const int32 InSeedIndex);
		TSharedPtr<FExtrusion> InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion);

		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		bool UpdateExtrusionQueue();

		virtual void CompleteWork() override;

	protected:
		EExtrusionFlags ComputeFlags() const
		{
			uint32 Flags = 0;

			if (Settings->bAllowChildExtrusions) { Flags |= static_cast<uint32>(EExtrusionFlags::AllowsChildren); }
			if (Settings->bDetectClosedLoops) { Flags |= static_cast<uint32>(EExtrusionFlags::ClosedLoop); }
			if (StopFilters) { Flags |= static_cast<uint32>(EExtrusionFlags::Bounded); }

			return static_cast<EExtrusionFlags>(Flags);
		}

		TSharedPtr<FExtrusion> CreateExtrusionTemplate(const int32 InSeedIndex, const int32 InMaxIterations);
	};
}
