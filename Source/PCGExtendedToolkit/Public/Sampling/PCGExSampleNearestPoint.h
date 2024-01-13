// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleNearestPoint.generated.h"

#define PCGEX_FOREACH_FIELD_NEARESTPOINT(MACRO)\
MACRO(Success, bool)\
MACRO(Location, FVector)\
MACRO(LookAt, FVector)\
MACRO(Normal, FVector)\
MACRO(Distance, double)\
MACRO(SignedDistance, double)\
MACRO(Angle, double)

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
		double SampledRangeMin = TNumericLimits<double>::Max();
		double SampledRangeMax = 0;
		double SampledRangeWidth = 0;
		int32 UpdateCount = 0;

		FTargetInfos Closest;
		FTargetInfos Farthest;

		void UpdateCompound(const FTargetInfos& Infos)
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

		double GetRangeRatio(const double Distance) const
		{
			return (Distance - SampledRangeMin) / SampledRangeWidth;
		}

		bool IsValid() const { return UpdateCount > 0; }
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestPointSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNearestPointSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestPoint, "Sample : Nearest Point", "Find the closest point on the nearest collidable surface.");
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=1))
	double RangeMax = 300;

	/** Use a per-point minimum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMin = false;

	/** Attribute or property to read the minimum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMin", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMin;

	/** Use a per-point maximum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMax = false;

	/** Attribute or property to read the maximum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMax", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMax;

	/** Weight method used for blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("SuccessfullySampled");

	/** Write the sample location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** Name of the 'vector' attribute to write sampled Location to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteLocation"))
	FName LocationAttributeName = FName("WeightedLocation");

	/** Write the sample "look at" direction from the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAt = false;

	/** Name of the 'vector' attribute to write sampled LookAt to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteLookAt"))
	FName LookAtAttributeName = FName("WeightedLookAt");


	/** Write the sampled normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the 'vector' attribute to write sampled Normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteNormal"))
	FName NormalAttributeName = FName("WeightedNormal");

	/** The attribute or property on the targets that is to be considered their "Normal".*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Source", EditCondition="bWriteNormal"))
	FPCGExInputDescriptorWithDirection NormalSource;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance"))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Write the sampled angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteAngle"))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle"))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestPointElement;

	virtual ~FPCGExSampleNearestPointContext() override;

	PCGExData::FPointIO* Targets = nullptr;

	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	double RangeMin = 0;
	double RangeMax = 1000;

	bool bUseLocalRangeMin = false;
	bool bUseLocalRangeMax = false;

	PCGEx::FLocalSingleFieldGetter RangeMinGetter;
	PCGEx::FLocalSingleFieldGetter RangeMaxGetter;
	PCGEx::FLocalDirectionGetter NormalGetter;

	TObjectPtr<UCurveFloat> WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_DECL)

	EPCGExAxis SignAxis;
	EPCGExAxis AngleAxis;
	EPCGExAngleRange AngleRange;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FSamplePointTask : public FPCGExNonAbandonableTask
{
public:
	FSamplePointTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
