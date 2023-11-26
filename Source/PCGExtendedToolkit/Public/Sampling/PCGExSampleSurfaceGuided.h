// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleSurfaceGuided.generated.h"

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleSurfaceGuidedSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleSurfaceGuided, "Sample Surface Guided", "Find the collision point on the nearest collidable surface in a given direction.");
#endif

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithDirection Direction;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double Size = 1000;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bUseLocalSize = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bUseLocalSize", ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithSingleField LocalSize;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** If the trace fails, use the end of the trace as a hit, but will still be marked as fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bProjectFailToSize = false;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSuccess"))
	FName Success = FName("SuccessfullySampled");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteLocation"))
	FName Location = FName("GuidedLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FName Normal = FName("GuidedNormal");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("GuidedDistance");

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
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSurfaceGuidedElement;

public:
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	int32 CollisionObjectType;
	bool bIgnoreSelf = true;

	bool bProjectFailToSize;
	double Size;
	bool bUseLocalSize = false;
	PCGEx::FLocalSingleComponentInput LocalSize;
	PCGEx::FLocalDirectionInput Direction;

	PCGEX_OUT_ATTRIBUTE(Success, bool)
	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)

	int64 NumTraceComplete = 0;

	void WrapTraceTask(const FPointTask* Task, bool bSuccess)
	{
		FWriteScopeLock ScopeLock(ContextLock);
		NumTraceComplete++;
	}
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedElement : public FPCGExPointsProcessorElementBase
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
class PCGEXTENDEDTOOLKIT_API FTraceTask : public FPointTask
{
public:
	FTraceTask(FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, PCGExMT::FTaskInfos InInfos) :
		FPointTask(InContext, InPointData, InInfos)
	{
	}

	virtual void ExecuteTask(FPCGContext* InContext) override
	{
		FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);
		FPCGPoint InPoint = PointData->In->GetPoint(Infos.Index);
		FVector Origin = InPoint.Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		if (Context->bIgnoreSelf) { CollisionParams.AddIgnoredActor(InContext->SourceComponent->GetOwner()); }

		double Size = Context->bUseLocalSize ? Context->LocalSize.GetValue(InPoint) : Context->Size;
		FVector Trace = Context->Direction.GetValue(InPoint) * Size;
		FVector End = Origin + Trace;
		
		if (!Context->Points) { return; }

		bool bSuccess = false;
		auto ProcessTraceResult = [&]()
		{
			PCGEX_SET_OUT_ATTRIBUTE(Location, Infos.Key, HitResult.ImpactPoint)
			PCGEX_SET_OUT_ATTRIBUTE(Normal, Infos.Key, HitResult.Normal)
			PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, FVector::Distance(HitResult.ImpactPoint, Origin))
		};

		if (Context->CollisionType == EPCGExCollisionFilterType::Channel)
		{
			if (Context->World->LineTraceSingleByChannel(HitResult, Origin, End, Context->CollisionChannel, CollisionParams))
			{
				ProcessTraceResult();
			}
		}
		else
		{
			FCollisionObjectQueryParams ObjectQueryParams = FCollisionObjectQueryParams(Context->CollisionObjectType);
			if (Context->World->LineTraceSingleByObjectType(HitResult, Origin, End, ObjectQueryParams, CollisionParams))
			{
				ProcessTraceResult();
			}
		}

		if(Context->bProjectFailToSize)
		{
			PCGEX_SET_OUT_ATTRIBUTE(Location, Infos.Key, End)
			PCGEX_SET_OUT_ATTRIBUTE(Normal, Infos.Key, Trace.GetSafeNormal()*-1)
			PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, Size)
		}
		
		PCGEX_SET_OUT_ATTRIBUTE(Success, Infos.Key, bSuccess)
		Context->WrapTraceTask(this, bSuccess);
	}

public:
	FHitResult HitResult;
};
