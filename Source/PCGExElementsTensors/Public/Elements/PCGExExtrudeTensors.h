// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExTensorsTransform.h"

#include "Core/PCGExPathProcessor.h"
#include "Core/PCGExPointFilter.h"
#include "Core/PCGExTensor.h"
#include "Data/PCGExPointElements.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "Details/PCGExSettingsMacros.h"
#include "Math/PCGExMathAxis.h"
#include "Paths/PCGExPathIntersectionDetails.h"
#include "Paths/PCGExPathOutputDetails.h"
#include "Paths/PCGExPathsCommon.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Sorting/PCGExSortingCommon.h"

#include "PCGExExtrudeTensors.generated.h"

struct FPCGExSortRuleConfig;

namespace PCGExSorting
{
	class FSorter;
}

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}

UENUM()
enum class EPCGExSelfIntersectionMode : uint8
{
	PathLength  = 0 UMETA(DisplayName = "Path Length", Tooltip="Sort extrusion by length, and resort to sorting rules in case of equality."),
	SortingOnly = 1 UMETA(DisplayName = "Sorting only", Tooltip="Only use sorting rules to sort paths."),
};

UENUM()
enum class EPCGExSelfIntersectionPriority : uint8
{
	Crossing = 0 UMETA(DisplayName = "Favor Crossing", Tooltip="Resolve crossing detection first, then merge."),
	Merge    = 1 UMETA(DisplayName = "Favor Merge", Tooltip="Resolve merge first, then crossing"),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="tensors/extrude-tensors"))
class UPCGExExtrudeTensorsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ExtrudeTensors, "Path : Extrude Tensors", "Extrude input points into paths along tensors.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Transform); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPathProcessorSettings
public:
	virtual FName GetMainInputPin() const override;
	virtual FName GetMainOutputPin() const override;
	//~End UPCGExPathProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Transforms", meta=(PCG_Overridable))
	bool bTransformRotation = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Transforms", meta=(PCG_Overridable, EditCondition="bTransformRotation"))
	EPCGExTensorTransformMode Rotation = EPCGExTensorTransformMode::Align;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Transforms", meta=(PCG_Overridable, EditCondition="bTransformRotation && Rotation == EPCGExTensorTransformMode::Align"))
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

	PCGEX_SETTING_VALUE_DECL(Iterations, int32)

	/** Whether to adjust max iteration based on max value found on points. Use at your own risks! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Use Max from Points", ClampMin=1, EditCondition="bUsePerPointMaxIterations", HideEditConditionToggle))
	bool bUseMaxFromPoints = false;

	/** Whether to limit the length of the generated path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bUseMaxLength = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable, EditCondition="bUseMaxLength", EditConditionHides))
	EPCGExInputValueType MaxLengthInput = EPCGExInputValueType::Constant;

	/** Max length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Length (Attr)", EditCondition="bUseMaxLength && MaxLengthInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxLengthAttribute = FName("MaxLength");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Length", EditCondition="bUseMaxLength && MaxLengthInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxLength = 100;

	PCGEX_SETTING_VALUE_DECL(MaxLength, double)

	/** Whether to limit the number of points in a generated path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bUseMaxPointsCount = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable, EditCondition="bUseMaxPointsCount", EditConditionHides))
	EPCGExInputValueType MaxPointsCountInput = EPCGExInputValueType::Constant;

	/** Max length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count (Attr)", EditCondition="bUseMaxPointsCount && MaxPointsCountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxPointsCountAttribute = FName("MaxPointsCount");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, DisplayName="Max Points Count", EditCondition="bUseMaxPointsCount && MaxPointsCountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxPointsCount = 100;

	PCGEX_SETTING_VALUE_DECL(MaxPointsCount, int32)

	/** Whether to limit path length or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable, ClampMin=0.001))
	double FuseDistance = DBL_COLLOCATION_TOLERANCE;

	/** How to deal with points that are stopped */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	EPCGExTensorStopConditionHandling StopConditionHandling = EPCGExTensorStopConditionHandling::Exclude;

	/** Whether to stop sampling when extrusion is stopped. While path will be cut, there's a chance that the head of the search comes back into non-stopping conditions, which would start a new extrusion. With this option disabled, new paths won't be permitted to exist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_Overridable))
	bool bAllowChildExtrusions = false;

	/** If enabled, seeds that start stopped won't be extruded at all. Otherwise, they are transformed until they eventually reach a point that's outside stopping conditions and start an extrusion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta=(PCG_NotOverridable))
	bool bIgnoreStoppedSeeds = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Ext)", meta=(PCG_Overridable))
	bool bDoExternalPathIntersections = false;

	/** If enabled, if the origin location of the extrusion is detected as an intersection, it is not considered an intersection. This allows to have seeds perfectly located on paths used for intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Ext)", meta=(PCG_Overridable, EditCondition="bDoExternalPathIntersections"))
	bool bIgnoreIntersectionOnOrigin = true;

	/** Intersection settings  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Ext)", meta=(PCG_Overridable, EditCondition="bDoExternalPathIntersections"))
	FPCGExPathIntersectionDetails ExternalPathIntersections;

	/** Whether to test for intersection between actively extruding paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable))
	bool bDoSelfPathIntersections = false;

	/** How to order intersection checks. Sorting is using seeds input attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, EditCondition="bDoSelfPathIntersections"))
	EPCGExSelfIntersectionMode SelfIntersectionMode = EPCGExSelfIntersectionMode::PathLength;

	/** Controls the order in which paths extrusion will be stopped when intersecting, if shortest/longest path fails. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta = (PCG_Overridable, EditCondition="bDoSelfPathIntersections"))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Descending;

	/** Intersection settings for extruding path intersections */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, EditCondition="bDoSelfPathIntersections"))
	FPCGExPathIntersectionDetails SelfPathIntersections;


	/** Whether to test for intersection between actively extruding paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, EditCondition="bDoSelfPathIntersections"))
	bool bMergeOnProximity = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, DisplayName=" ├─ Priority", EditCondition="bDoSelfPathIntersections && bMergeOnProximity", ClampMin = 0, ClampMax =1))
	EPCGExSelfIntersectionPriority SelfIntersectionPriority = EPCGExSelfIntersectionPriority::Crossing;

	/** Which end of the extruded segment should be favored. 0 = start, 1 = end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, DisplayName=" ├─ Balance", EditCondition="bDoSelfPathIntersections && bMergeOnProximity", ClampMin = 0, ClampMax =1))
	double ProximitySegmentBalance = 0.5;

	/** Whether to test for intersection between actively extruding paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)", meta=(PCG_Overridable, DisplayName=" └─ Settings", EditCondition="bDoSelfPathIntersections && bMergeOnProximity"))
	FPCGExPathIntersectionDetails MergeDetails = FPCGExPathIntersectionDetails(10, 20);


	/** Whether the node should attempt to close loops based on angle and proximity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)|Closing Loops", meta=(PCG_NotOverridable))
	bool bDetectClosedLoops = false;

	/** Range at which the first point must be located to check angle */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)|Closing Loops", meta=(PCG_Overridable, DisplayName=" ├─ Search Distance", EditCondition="bDetectClosedLoops"))
	double ClosedLoopSearchDistance = 100;

	/** Angle at which the loop will be closed, if within range */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Intersections (Self)|Closing Loops", meta=(PCG_Overridable, DisplayName=" └─ Search Angle", EditCondition="bDetectClosedLoops", Units="Degrees", ClampMin=0, ClampMax=90))
	double ClosedLoopSearchAngle = 11.25;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails AttributesToPathTags;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfChildExtrusion = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfChildExtrusion"))
	FString IsChildExtrusionTag = TEXT("Child");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsStoppedByFilters = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsStoppedByFilters"))
	FString IsStoppedByFiltersTag = TEXT("StoppedByFilters");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsStoppedByIntersection = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsStoppedByIntersection"))
	FString IsStoppedByIntersectionTag = TEXT("StoppedByPath");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsStoppedBySelfIntersection = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsStoppedBySelfIntersection"))
	FString IsStoppedBySelfIntersectionTag = TEXT("SelfCrossed");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfSelfMerged = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfSelfMerged"))
	FString IsSelfMergedTag = TEXT("SelfMerged");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagIfIsFollowUp = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagIfIsFollowUp"))
	FString IsFollowUpTag = TEXT("IsFollowUp");


	/** Tensor sampling settings. Note that these are applied on the flattened sample, e.g after & on top of individual tensors' mutations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Tensor Sampling Settings"))
	FPCGExTensorHandlerDetails TensorHandlerDetails;


	/** Whether to give a new seed to the points. If disabled, they will inherit the original one. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_NotOverridable))
	bool bRefreshSeed = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="Paths Output Settings"))
	FPCGExPathOutputDetails PathOutputDetails;

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;

private:
	friend class FPCGExExtrudeTensorsElement;
};

struct FPCGExExtrudeTensorsContext final : FPCGExPathProcessorContext
{
	friend class FPCGExExtrudeTensorsElement;

	TArray<TObjectPtr<const UPCGExTensorFactoryData>> TensorFactories;
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> StopFilterFactories;

	FPCGExPathIntersectionDetails ExternalPathIntersections;
	FPCGExPathIntersectionDetails SelfPathIntersections;
	FPCGExPathIntersectionDetails MergeDetails;

	double ClosedLoopSquaredDistance = 0;
	double ClosedLoopSearchDot = 0;

	TArray<TSharedPtr<PCGExData::FFacade>> PathsFacades;
	TArray<TSharedPtr<PCGExPaths::FPath>> ExternalPaths;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExExtrudeTensorsElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ExtrudeTensors)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
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
		TArray<FTransform> ExtrudedPoints;
		TArray<FBox> SegmentBounds;
		double DistToLastSum = 0;
		PCGExData::FConstPoint Origin;
		PCGExData::FProxyPoint ProxyHead;

	public:
		FBox Bounds = FBox(ForceInit);

		bool bIsExtruding = false;
		bool bIsComplete = false;
		bool bIsValidPath = false;
		bool bIsStopped = false;
		bool bIsClosedLoop = false;
		bool bHitStopFilters = false;
		bool bHitIntersection = false;
		bool bHitSelfIntersection = false;
		bool bIsSelfMerged = false;

		bool bIsProbe = false;
		bool bIsChildExtrusion = false;
		bool bIsFollowUp = false;
		bool bAdvancedOnly = false;

		virtual ~FExtrusion() = default;
		FProcessor* Processor = nullptr;
		const FPCGExExtrudeTensorsContext* Context = nullptr;
		const UPCGExExtrudeTensorsSettings* Settings = nullptr;
		TSharedPtr<TArray<TSharedPtr<PCGExPaths::FPath>>> SolidPaths;
		TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;
		TSharedPtr<PCGExPointFilter::FManager> StopFilters;

		FVector LastInsertion = FVector::ZeroVector;
		FVector ExtrusionDirection = FVector::ZeroVector;
		FTransform Head = FTransform::Identity;
		FTransform ActiveTransform = FTransform::Identity;

		int32 SeedIndex = -1;
		int32 RemainingIterations = 0;
		double MaxLength = MAX_dbl;
		int32 MaxPointCount = MAX_int32;

		PCGExPaths::FPathMetrics Metrics;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TSharedPtr<PCGExOctree::FItemOctree> EdgeOctree;
		const TArray<TSharedPtr<FExtrusion>>* Extrusions = nullptr;

		FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations);

		PCGExMath::FSegment GetHeadSegment() const;
		void SetHead(const FTransform& InHead);

		virtual bool Advance() = 0;
		void Complete();
		void CutOff(const FVector& InCutOff);
		void Shorten(const FVector& InCutOff);

		PCGExMath::FClosestPosition FindCrossing(const PCGExMath::FSegment& InSegment, bool& OutIsLastSegment, PCGExMath::FClosestPosition& OutClosestPosition, const int32 TruncateSearch = 0) const;
		bool TryMerge(const PCGExMath::FSegment& InSegment, const PCGExMath::FClosestPosition& InMerge);

		void Cleanup();

	protected:
		bool OnAdvanced(const bool bStop);
		virtual bool Extrude(const PCGExTensor::FTensorSample& Sample, FTransform& InPoint) = 0;
		void StartNewExtrusion();
		void Insert(const FTransform& InPoint);
	};

	template <EExtrusionFlags InternalFlags>
	class TExtrusion : public FExtrusion
	{
	public:
		TExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations)
			: FExtrusion(InSeedIndex, InFacade, InMaxIterations)
		{
		}

		virtual bool Advance() override
		{
			if (bIsStopped) { return false; }

			bAdvancedOnly = true;

			const FVector PreviousHeadLocation = Head.GetLocation();
			bool bSuccess = false;
			const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(SeedIndex, Head, bSuccess);

			if (!bSuccess) { return OnAdvanced(true); }

			ExtrusionDirection = Sample.DirectionAndSize.GetSafeNormal();

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
					Head.SetRotation(PCGExMath::MakeDirection(Settings->AlignAxis, ExtrusionDirection * -1, Head.GetRotation().GetUpVector()));
				}
			}

			const FVector HeadLocation = PreviousHeadLocation + Sample.DirectionAndSize;
			Head.SetLocation(HeadLocation);
			ActiveTransform = Head; // Copy head to mutable head

			if constexpr (Supports(InternalFlags, EExtrusionFlags::ClosedLoop))
			{
				if (const FVector Tail = Origin.GetLocation(); FVector::DistSquared(Metrics.Last, Tail) <= Context->ClosedLoopSquaredDistance && FVector::DotProduct(ExtrusionDirection, (Tail - PreviousHeadLocation).GetSafeNormal()) > Context->ClosedLoopSearchDot)
				{
					bIsClosedLoop = true;
					return OnAdvanced(true);
				}
			}

			//Head = ExtrudedPoints.Last();

			if constexpr (Supports(InternalFlags, EExtrusionFlags::Bounded))
			{
				ProxyHead.Transform = ActiveTransform;
				if (StopFilters->Test(ProxyHead))
				{
					if (bIsExtruding && !bIsComplete)
					{
						bHitStopFilters = true;
						if (Settings->StopConditionHandling == EPCGExTensorStopConditionHandling::Include) { Insert(Head); }

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
					if (bIsProbe)
					{
						SetHead(Head);
						return OnAdvanced(false);
					}
				}
			}

			return OnAdvanced(!Extrude(Sample, ActiveTransform));
		}

	protected:
		virtual bool Extrude(const PCGExTensor::FTensorSample& Sample, FTransform& InActiveTransform) override;
	};

	template <EExtrusionFlags InternalFlags>
	bool TExtrusion<InternalFlags>::Extrude(const PCGExTensor::FTensorSample& Sample, FTransform& InActiveTransform)
	{
		// return whether we can keep extruding or not
		bIsExtruding = true;

		double DistToLast = 0;
		const double Length = Metrics.Add(Metrics.Last + Sample.DirectionAndSize, DistToLast);
		DistToLastSum += DistToLast;

		if (DistToLastSum < Settings->FuseDistance) { return true; }
		DistToLastSum = 0;

		if (Length > MaxLength)
		{
			// Adjust position to match max length
			const FVector LastValidPos = ExtrudedPoints.Last().GetLocation();
			InActiveTransform.SetLocation(LastValidPos + ((Metrics.Last - LastValidPos).GetSafeNormal() * (Length - MaxLength)));
		}

		if constexpr (Supports(InternalFlags, EExtrusionFlags::CollisionCheck))
		{
			int32 PathIndex = -1;

			bIsExtruding = true;

			const PCGExMath::FSegment Segment(ExtrudedPoints.Last().GetLocation(), InActiveTransform.GetLocation());

			PCGExMath::FClosestPosition Intersection = PCGExPaths::Helpers::FindClosestIntersection(Context->ExternalPaths, Context->ExternalPathIntersections, Segment, PathIndex);

			// Path intersection
			if (Intersection)
			{
				bHitIntersection = true;

				if (FMath::IsNearlyZero(Intersection.DistSquared))
				{
					if (!Settings->bIgnoreIntersectionOnOrigin || (Settings->bIgnoreIntersectionOnOrigin && ExtrudedPoints.Num() > 1))
					{
						return OnAdvanced(true);
					}

					bHitIntersection = false;
				}
				else
				{
					InActiveTransform.SetLocation(Intersection);
					Insert(InActiveTransform);
					return OnAdvanced(true);
				}
			}

			PCGExMath::FClosestPosition Merge(Segment.Lerp(Settings->ProximitySegmentBalance));

			Intersection = PCGExPaths::Helpers::FindClosestIntersection(*SolidPaths.Get(), Context->ExternalPathIntersections, Segment, PathIndex, Merge);

			if (Settings->SelfIntersectionPriority == EPCGExSelfIntersectionPriority::Crossing)
			{
				// Self-intersect
				if (Intersection)
				{
					bHitIntersection = true;
					bHitSelfIntersection = true;

					InActiveTransform.SetLocation(Intersection);
					Insert(InActiveTransform);
					return OnAdvanced(true);
				}

				// Merge
				if (TryMerge(Segment, Merge))
				{
					InActiveTransform.SetLocation(Merge);
					Insert(InActiveTransform);
					return OnAdvanced(true);
				}
			}
			else
			{
				// Merge
				if (TryMerge(Segment, Merge))
				{
					InActiveTransform.SetLocation(Merge);
					Insert(InActiveTransform);
					return OnAdvanced(true);
				}

				// Self-intersect
				if (Intersection)
				{
					bHitIntersection = true;
					bHitSelfIntersection = true;

					InActiveTransform.SetLocation(Intersection);
					Insert(InActiveTransform);
					return OnAdvanced(true);
				}
			}
		}

		Insert(InActiveTransform);

		return !(Length >= MaxLength || ExtrudedPoints.Num() >= MaxPointCount);
	}

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExExtrudeTensorsContext, UPCGExExtrudeTensorsSettings>
	{
	protected:
		TSharedPtr<PCGExSorting::FSorter> Sorter;

		FRWLock NewExtrusionLock;
		int32 RemainingIterations = 0;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> PerPointIterations;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> MaxPointsCount;
		TSharedPtr<PCGExDetails::TSettingValue<double>> MaxLength;

		TSharedPtr<PCGExPointFilter::FManager> StopFilters;
		TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

		FPCGExAttributeToTagDetails AttributesToPathTags;
		TArray<TSharedPtr<FExtrusion>> ExtrusionQueue;
		TArray<TSharedPtr<FExtrusion>> NewExtrusions;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<FExtrusion>>> CompletedExtrusions;
		TSharedPtr<TArray<TSharedPtr<PCGExPaths::FPath>>> StaticPaths;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool IsTrivial() const override { return false; }

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		void InitExtrusionFromSeed(const int32 InSeedIndex);
		TSharedPtr<FExtrusion> InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion);

		void SortQueue();

		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
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
			if (!Context->ExternalPaths.IsEmpty() || Settings->bDoSelfPathIntersections) { Flags |= static_cast<uint32>(EExtrusionFlags::CollisionCheck); }

			return static_cast<EExtrusionFlags>(Flags);
		}

		TSharedPtr<FExtrusion> CreateExtrusionTemplate(const int32 InSeedIndex, const int32 InMaxIterations);
	};

	class FBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
	public:
		explicit FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection);
		virtual void Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		void OnPathsPrepared();
	};
}
