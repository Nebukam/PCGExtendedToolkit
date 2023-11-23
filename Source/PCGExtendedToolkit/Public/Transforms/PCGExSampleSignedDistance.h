// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExLocalAttributeHelpers.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGPolyLineData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSampleSignedDistance.generated.h"


class UPCGPolyLineData;

namespace PCGExSampleSignedDistance
{
}

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleSignedDistanceSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExSampleSignedDistanceSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleSignedDistance, "Signed Distance", "Find the shortest signed distance to the input.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Drive how precise the sampling is going to be on polylines. Low value can become VERY expensive. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double SplineStepSize = 10;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleSignedDistanceContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSignedDistanceElement;

public:
	TArray<UPCGPolyLineData*> TargetPolyLines;
	TArray<UPCGPointData*> TargetPoints;

	double SplineStepSize = 10;
	EPCGExAxis SignAxis;

	void FindClosestTransform(const FVector& Position, FTransform& OutTransform) const
	{
		FTransform ClosestTransform = FTransform::Identity;
		double BoundDistance = TNumericLimits<double>::Max();
		const UPCGPolyLineData* ClosestLine = nullptr;
		for (const UPCGPolyLineData* PolyLine : TargetPolyLines)
		{
			if (const double Dist = FVector::DistSquared(Position, PolyLine->GetBounds().GetCenter());
				!ClosestLine || Dist < BoundDistance)
			{
				BoundDistance = Dist;
				ClosestLine = PolyLine;
			}
		}

		BoundDistance = TNumericLimits<double>::Max();

		if (ClosestLine)
		{
			for (int i = 0; i < ClosestLine->GetNumSegments(); i++)
			{
				//TODO: Need to check what Length return, and if it is normalized or not.
				const double NumSamples = ClosestLine->GetSegmentLength(i) / SplineStepSize; // TODO: This can be cached.
				for (int d = 0; d <= NumSamples; d++)
				{
					// TODO :
					// search within spline could be optimized by caching a candid representation and
					// sampling start | end + 25%, 50%, 75%, and start sampling from there
					FTransform SampledTransform = ClosestLine->GetTransformAtDistance(i, d * SplineStepSize);
					if (const double Dist = FVector::DistSquared(Position, SampledTransform.GetLocation());
						Dist < BoundDistance)
					{
						BoundDistance = Dist;
						ClosestTransform = SampledTransform;
					}
				}
			}
		}

		for (const UPCGPointData* TargetPointsData : TargetPoints)
		{
			for (const TArray<FPCGPoint>& PointsList = TargetPointsData->GetPoints();
			     const FPCGPoint& Point : PointsList)
			{
				FTransform SampledTransform = Point.Transform;
				if (const double Dist = FVector::DistSquared(Position, SampledTransform.GetLocation());
					Dist < BoundDistance)
				{
					BoundDistance = Dist;
					ClosestTransform = SampledTransform;
				}
			}
		}

		OutTransform = ClosestTransform;
	}
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleSignedDistanceElement : public FPCGExPointsProcessorElementBase
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
