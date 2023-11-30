// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleNearestPoint.generated.h"

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

		double GetRangeRatio(double Distance) const
		{
			return (Distance - SampledRangeMin) / SampledRangeWidth;
		}

		bool IsValid() { return UpdateCount > 0; }
	};
}

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling")
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling")
	double RangeMax = 300;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle))
	bool bUseLocalRangeMin = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMin", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMin;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle))
	bool bUseLocalRangeMax = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRangeMax", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRangeMax;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting")
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting")
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSuccess"))
	FName Success = FName("SuccessfullySampled");

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName=" └─ Source", EditCondition="bWriteNormal"))
	FPCGExInputDescriptorWithDirection NormalSource;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance"))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteAngle"))
	FName Angle = FName("WeightedAngle");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName=" └─ Axis", EditCondition="bWriteAngle"))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName=" └─ Range", EditCondition="bWriteAngle"))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestPointElement;

public:
	UPCGPointData* Targets = nullptr;
	UPCGPointData* TargetsCache = nullptr;
	UPCGPointData::PointOctree* Octree = nullptr;

	TMap<PCGMetadataEntryKey, int64> TargetIndices;
	mutable FRWLock IndicesLock;

	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;
	EPCGExWeightMethod WeightMethod = EPCGExWeightMethod::FullRange;

	double RangeMin = 0;
	double RangeMax = 1000;

	bool bLocalRangeMin = false;
	bool bLocalRangeMax = false;

	int64 NumTargets = 0;

	PCGEx::FLocalSingleFieldGetter RangeMinGetter;
	PCGEx::FLocalSingleFieldGetter RangeMaxGetter;

	PCGEx::FLocalDirectionGetter NormalInput;

	UCurveFloat* WeightCurve = nullptr;

	//TODO: Setup target local inputs

	PCGEX_OUT_ATTRIBUTE(Success, bool)
	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(LookAt, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)

	PCGEX_OUT_ATTRIBUTE(SignedDistance, double)
	EPCGExAxis SignAxis;

	PCGEX_OUT_ATTRIBUTE(Angle, double)
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
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
