// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Data/PCGExPolyLineIO.h"

#include "PCGExSampleNearestPolyline.generated.h"

namespace PCGExPolyLine
{

	struct PCGEXTENDEDTOOLKIT_API FSampleInfos
	{
		FSampleInfos()
		{
		}

		FSampleInfos(const FTransform& InTransform, const double InDistance):
			Transform(InTransform), Distance(InDistance)
		{
		}

		FTransform Transform;
		double Distance = 0;
	};

	struct PCGEXTENDEDTOOLKIT_API FTargetsCompoundInfos
	{
		FTargetsCompoundInfos()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		double SampledRangeMin = DBL_MAX; 
		double SampledRangeMax = 0;
		double SampledRangeWidth = 0;
		int32 UpdateCount = 0;

		FSampleInfos Closest;
		FSampleInfos Farthest;

		void UpdateCompound(const FSampleInfos& Infos)
		{
			UpdateCount++;

			if (Infos.Distance < SampledRangeMin)
			{
				Closest = Infos;
				SampledRangeMin = Infos.Distance;
			}

			if (Infos.Distance > SampledRangeMax)
			{
				Farthest = Infos;
				SampledRangeMax = Infos.Distance;
			}

			SampledRangeWidth = SampledRangeMax - SampledRangeMin;
		}

		double GetRangeRatio(double Distance) const
		{
			return (Distance - SampledRangeMin) / SampledRangeWidth;
		}
	};
	
}

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestPolylineSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExSampleNearestPolylineSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestPolyline, "Sample Nearest Polyline", "Find the closest transform on nearest polylines.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling")
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="SampleMethod==EPCGExSampleMethod::WithinRange"))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(ForceInlineRow, EditCondition="SampleMethod==EPCGExSampleMethod::WithinRange"))
	double RangeMax = 300;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle, EditCondition="SampleMethod==EPCGExSampleMethod::WithinRange"))
	bool bUseLocalRangeMin = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMin && SampleMethod==EPCGExSampleMethod::WithinRange", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMin;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle, EditCondition="SampleMethod==EPCGExSampleMethod::WithinRange"))
	bool bUseLocalRangeMax = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMax && SampleMethod==EPCGExSampleMethod::WithinRange", EditConditionHides))
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
	bool bWriteLookAt = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteLookAt"))
	FName LookAt = FName("WeightedLookAt");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FName Normal = FName("WeightedNormal");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	EPCGExAxis NormalSource = EPCGExAxis::Forward;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("WeightedDistance");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSignedDistance"))
	FName SignedDistance = FName("WeightedSignedDistance");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSignedDistance"))
	EPCGExAxis SignedDistanceAxis = EPCGExAxis::Forward;

	/** Maximum distance to check for closest surface. Input 0 to sample all target points.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	double MaxDistance = 1000;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPolylineContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestPolylineElement;

public:
	UPCGExPolyLineIOGroup* Targets = nullptr;

	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	EPCGExAxis NormalSource;

	double RangeMin = 0;
	double RangeMax = 1000;

	bool bLocalRangeMin = false;
	bool bLocalRangeMax = false;

	bool bUseOctree = false;
	int64 NumTargets = 0;

	PCGEx::FLocalSingleComponentInput RangeMinInput;
	PCGEx::FLocalSingleComponentInput RangeMaxInput;

	UCurveFloat* WeightCurve = nullptr;

	//TODO: Setup target local inputs

	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(LookAt, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)
	PCGEX_OUT_ATTRIBUTE(SignedDistance, double)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPolylineElement : public FPCGExPointsProcessorElementBase
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
