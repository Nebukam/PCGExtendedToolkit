// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleNearestSurface.generated.h"

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestSurfaceSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestSurface, "Sample Nearest Surface", "Find the closest point on the nearest collidable surface.");
#endif

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
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
	FName Location = FName("NearestLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteLookAt = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteLookAt"))
	FName LookAt = FName("NearestLookAt");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FName Normal = FName("NearestNormal");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("NearestDistance");


	/** Maximum distance to check for closest surface.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	double MaxDistance = 1000;

	/** Maximum distance to check for closest surface.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics", meta=(EditCondition="CollisionType==EPCGExCollisionFilterType::Channel", EditConditionHides, Bitmask, BitmaskEnum="ECollisionChannel"))
	TEnumAsByte<ECollisionChannel> CollisionChannel = static_cast<ECollisionChannel>(ECC_WorldDynamic);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics", meta=(EditCondition="CollisionType==EPCGExCollisionFilterType::ObjectType", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.EObjectTypeQuery"))
	int32 CollisionObjectType = static_cast<EObjectTypeQuery>(EObjectTypeQuery::ObjectTypeQuery1);

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	bool bIgnoreSelf = true;

	/** StepSize can't get smaller than this.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	double MinStepSize = 1;

	/** Maximum number of attempts per point. Each attempt increases probing radius by (MaxDistance/NumMaxAttempts)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	int32 NumMaxAttempts = 256;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestSurfaceContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestSurfaceElement;

public:
	double RangeMax = 1000;
	int32 NumMaxAttempts = 100;
	double AttemptStepSize = 0;
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	int32 CollisionObjectType;
	bool bIgnoreSelf = true;

	PCGEX_OUT_ATTRIBUTE(Success, bool)
	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(LookAt, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)

	int64 NumSweepComplete = 0;

	void WrapSweepTask(const FPointTask* Task, bool bSuccess);
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestSurfaceElement : public FPCGExPointsProcessorElementBase
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

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FSweepSphereTask : public FPointTask
{
public:

	double RangeMax = 1000;
	
	FSweepSphereTask(FPCGContext* InContext, UPCGExPointIO* InPointData, PCGExMT::FTaskInfos InInfos) :
		FPointTask(InContext, InPointData, InInfos)
	{
	}

	virtual void ExecuteTask(FPCGContext* InContext) override
	{
		FPCGExSampleNearestSurfaceContext* Context = static_cast<FPCGExSampleNearestSurfaceContext*>(InContext);
		FPCGPoint InPoint = PointData->In->GetPoint(Infos.Index);
		FVector Origin = InPoint.Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		if (Context->bIgnoreSelf) { CollisionParams.AddIgnoredActor(InContext->SourceComponent->GetOwner()); }

		FCollisionShape CollisionShape = FCollisionShape::MakeSphere(RangeMax);

		if (!Context->Points) { return; }

		FVector HitLocation;
		bool bSuccess = false;
		TArray<FOverlapResult> OutOverlaps;

		auto ProcessOverlapResults = [&]()
		{
			float MinDist = MAX_FLT;
			for (const FOverlapResult& Overlap : OutOverlaps)
			{
				if (!Overlap.bBlockingHit) { continue; }
				FVector OutClosestLocation;
				const float Distance = Overlap.Component->GetClosestPointOnCollision(Origin, OutClosestLocation);
				if (Distance < 0) { continue; }
				if (Distance == 0)
				{
					// Fallback for complex collisions?
					continue;
				}
				if (Distance < MinDist)
				{
					MinDist = Distance;
					HitLocation = OutClosestLocation;
					bSuccess = true;
				}
			}

			if (bSuccess)
			{
				const FVector Direction = (HitLocation - Origin).GetSafeNormal();
				PCGEX_SET_OUT_ATTRIBUTE(Location, Infos.Key, HitLocation)
				PCGEX_SET_OUT_ATTRIBUTE(Normal, Infos.Key, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
				PCGEX_SET_OUT_ATTRIBUTE(LookAt, Infos.Key, Direction)
				PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, MinDist)
			}
		};


		if (Context->CollisionType == EPCGExCollisionFilterType::Channel)
		{
			if (Context->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Context->CollisionChannel, CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
		}
		else
		{
			if (FCollisionObjectQueryParams ObjectQueryParams = FCollisionObjectQueryParams(Context->CollisionObjectType);
				Context->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, ObjectQueryParams, CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
		}

		PCGEX_SET_OUT_ATTRIBUTE(Success, Infos.Key, bSuccess)
		Context->WrapSweepTask(this, bSuccess);
	}

};
