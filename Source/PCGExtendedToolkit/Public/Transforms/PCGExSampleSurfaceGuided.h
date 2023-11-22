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
#include "PCGExSampleSurfaceGuided.generated.h"

namespace PCGExAsync
{
	class FTraceTask;
}


/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
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

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

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
	bool bWriteSurfaceLocation = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSurfaceLocation"))
	FName SurfaceLocation = FName("GuidedSurfaceLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteSurfaceNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSurfaceNormal"))
	FName SurfaceNormal = FName("GuidedSurfaceNormal");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("GuidedSurfaceDistance");

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

	double Size;
	bool bUseLocalSize = false;
	PCGEx::FLocalSingleComponentInput LocalSize;
	PCGEx::FLocalDirectionInput Direction;

	PCGEX_OUT_ATTRIBUTE(SurfaceLocation, FVector)
	PCGEX_OUT_ATTRIBUTE(SurfaceNormal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)

	int64 NumTraceComplete = 0;

	void WrapTraceTask(const PCGExAsync::FTraceTask* Task, bool bSuccess);
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

namespace PCGExAsync
{
	// Define the background task class
	class FTraceTask : public FPointTask
	{
	public:
		FTraceTask(FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, PCGExMT::FTaskInfos InInfos) :
			FPointTask(InContext, InPointData, InInfos)
		{
		}

		virtual void ExecuteTask(FPCGExPointsProcessorContext* InContext) override
		{
			FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);
			FPCGPoint InPoint = GetInPoint();
			FVector Origin = InPoint.Transform.GetLocation();

			FCollisionQueryParams CollisionParams;
			if (Context->bIgnoreSelf) { CollisionParams.AddIgnoredActor(InContext->SourceComponent->GetOwner()); }

			double Size = Context->bUseLocalSize ? Context->LocalSize.GetValue(InPoint) : Context->Size;

			if (!InContext->Points) { return; }

			if (Context->CollisionType == EPCGExCollisionFilterType::Channel)
			{
				if (InContext->World->LineTraceSingleByChannel(HitResult, Origin, Origin + (Context->Direction.GetValue(InPoint) * Size), Context->CollisionChannel, CollisionParams))
				{
					PCGEX_SET_OUT_ATTRIBUTE(SurfaceLocation, Infos.Key, HitResult.ImpactPoint)
					PCGEX_SET_OUT_ATTRIBUTE(SurfaceNormal, Infos.Key, HitResult.Normal)
					PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, FVector::Distance(HitResult.ImpactPoint, Origin))

					Context->WrapTraceTask(this, true);
				}
				else
				{
					Context->WrapTraceTask(this, false);
				}
			}
			else
			{
				FCollisionObjectQueryParams ObjectQueryParams = FCollisionObjectQueryParams(Context->CollisionObjectType);
				if (InContext->World->LineTraceSingleByObjectType(HitResult, Origin, Origin + (Context->Direction.GetValue(InPoint) * Size), ObjectQueryParams, CollisionParams))
				{
					PCGEX_SET_OUT_ATTRIBUTE(SurfaceLocation, Infos.Key, HitResult.ImpactPoint)
					PCGEX_SET_OUT_ATTRIBUTE(SurfaceNormal, Infos.Key, HitResult.Normal)
					PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, FVector::Distance(HitResult.ImpactPoint, Origin))

					Context->WrapTraceTask(this, true);
				}
				else
				{
					Context->WrapTraceTask(this, false);
				}
			}
		}

	public:
		FHitResult HitResult;
	};
}
