// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExLocalAttributeHelpers.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSampleNearestPoint.generated.h"

UENUM(BlueprintType)
enum class EPCGExSampleMethod : uint8
{
	TargetsWithinRange UMETA(DisplayName = "All Targets Within Range", ToolTip="TBD"),
	AllTargets UMETA(DisplayName = "All Targets", ToolTip="TBD"),
	ClosestTarget UMETA(DisplayName = "Closest Target", ToolTip="TBD"),
	FarthestTarget UMETA(DisplayName = "Farthest Target", ToolTip="TBD"),
	TargetsExtents UMETA(DisplayName = "Targets Extents", ToolTip="TBD"),
};

UENUM(BlueprintType)
enum class EPCGExWeightMethod : uint8
{
	FullRange UMETA(DisplayName = "Full Range", ToolTip="Weight is sampled using the normalized distance over the full min/max range."),
	EffectiveRange UMETA(DisplayName = "Effective Range", ToolTip="Weight is sampled using the normalized distance over the min/max of sampled points."),
};

namespace PCGExNearestPoint
{
	struct PCGEXTENDEDTOOLKIT_API FTargetInfos
	{
		FTargetInfos()
		{
		}

		FTargetInfos(const int32 InIndex, const double InDistance):
			Index(InIndex), Distance(InDistance)
		{
		}

		int32 Index = -1;
		double Distance = 0;
	};

	struct PCGEXTENDEDTOOLKIT_API FTargetsCompoundInfos
	{
		FTargetsCompoundInfos()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		double RangeMin = TNumericLimits<double>::Max();
		double RangeMax = 0;
		double RangeWidth = 0;

		FTargetInfos Closest;
		FTargetInfos Farthest;

		void UpdateCompound(const FTargetInfos& Infos)
		{
			if (Infos.Distance < RangeMin)
			{
				Closest = Infos;
				RangeMin = Infos.Distance;
			}

			if (Infos.Distance > RangeMax)
			{
				Farthest = Infos;
				RangeMax = Infos.Distance;
			}

			RangeWidth = RangeMax - RangeMin;
		}

		double GetRangeRatio(double Distance) const
		{
			return (Distance - RangeMin) / RangeWidth;
		}
	};
}

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestPointSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExSampleNearestPointSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestPoint, "Sample Nearest Point", "Find the closest point on the nearest collidable surface.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling")
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::TargetsWithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="SampleMethod==EPCGExSampleMethod::TargetsWithinRange"))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(ForceInlineRow, EditCondition="SampleMethod==EPCGExSampleMethod::TargetsWithinRange"))
	double RangeMax = 300;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle, EditCondition="SampleMethod==EPCGExSampleMethod::TargetsWithinRange"))
	bool bUseLocalRangeMin = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMin && SampleMethod==EPCGExSampleMethod::TargetsWithinRange", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMin;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle, EditCondition="SampleMethod==EPCGExSampleMethod::TargetsWithinRange"))
	bool bUseLocalRangeMax = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMax && SampleMethod==EPCGExSampleMethod::TargetsWithinRange", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMax;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting")
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting")
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteLocation"))
	FName Location = FName("WeightedLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDirection = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDirection"))
	FName Direction = FName("WeightedDirection");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FName Normal = FName("WeightedNormal");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FPCGExInputDescriptorWithDirection NormalSource;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("WeightedDistance");


	/** Maximum distance to check for closest surface. Input 0 to sample all target points.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	double MaxDistance = 1000;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestPointElement;

public:
	UPCGPointData* Targets = nullptr;
	UPCGPointData::PointOctree* Octree = nullptr;

	TMap<PCGMetadataEntryKey, int64> TargetIndices;
	mutable FRWLock IndicesLock;

	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::TargetsWithinRange;
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	double RangeMin = 0;
	double RangeMax = 1000;

	bool bLocalRangeMin = false;
	bool bLocalRangeMax = false;

	bool bUseOctree = false;
	int64 NumTargets = 0;

	PCGEx::FLocalSingleComponentInput RangeMinInput;
	PCGEx::FLocalSingleComponentInput RangeMaxInput;

	PCGEx::FLocalDirectionInput NormalInput;

	UCurveFloat* WeightCurve = nullptr;

	//TODO: Setup target local inputs

	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(Direction, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
