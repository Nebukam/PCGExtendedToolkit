// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSampleNearestSurface.generated.h"

namespace PCGExAsync
{
	class FSweepSphereTask;
}


/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multithread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleDistanceFieldSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestSurface, "Sample Nearest Surface", "Find the closest point on the nearest collidable surface.");
#endif

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bWriteNearestSurfaceLocation = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNearestSurfaceLocation"))
	FName OutNearestSurfaceLocationName = FName("NearestSurfaceLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bWriteDirection = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDirection"))
	FName OutDirectionName = FName("DirectionToNearestSurface");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bWriteSurfaceNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteSurfaceNormal"))
	FName OutSurfaceNormalName = FName("NearestSurfaceNormal");

	
	/** Maximum distance to check for closest surface.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics")
	double MaxDistance = 1000;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics")
	TEnumAsByte<ECollisionChannel> CollisionChannel = static_cast<ECollisionChannel>(ECC_WorldDynamic);

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	bool bIgnoreSelf = true;

	/** StepSize can't get smaller than this*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	double MinStepSize = 1;

	/** Maximum number of attempts per point. Each attempt increases probing radius by (MaxDistance/NumMaxAttempts)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	int32 NumMaxAttempts = 256;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleDistanceFieldContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleDistanceFieldElement;

public:
	FName OutName = NAME_None;
	FPCGMetadataAttribute<FVector>* HitLocationAttribute;
	FPCGMetadataAttribute<FVector>* HitNormalAttribute;

	int32 NumMaxAttempts = 100;
	double AttemptStepSize = 0;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	bool bIgnoreSelf = true;

	int64 NumSweepComplete = 0;

	void ProcessSweepHit(const PCGExAsync::FSweepSphereTask* Task);
	void ProcessSweepMiss(const PCGExAsync::FSweepSphereTask* Task);
	void WrapSweepTask(const PCGExAsync::FSweepSphereTask* Task, bool bSuccess);
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleDistanceFieldElement : public FPCGExPointsProcessorElementBase
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
	class FSweepSphereTask : public FPointTask
	{
	public:
		FSweepSphereTask(FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, PCGExMT::FTaskInfos InInfos) :
			FPointTask(InContext, InPointData, InInfos)
		{
		}

		virtual void ExecuteTask() override
		{
			FPCGExSampleDistanceFieldContext* Ctx = static_cast<FPCGExSampleDistanceFieldContext*>(Context);
			FPCGPoint InPoint = GetInPoint();
			FVector Origin = InPoint.Transform.GetLocation();

			FCollisionQueryParams CollisionParams;
			if (Ctx->bIgnoreSelf) { CollisionParams.AddIgnoredActor(Context->SourceComponent->GetOwner()); }

			FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Ctx->AttemptStepSize * static_cast<float>(Infos.Attempt));
			FPCGExSampleDistanceFieldContext* InContext = static_cast<FPCGExSampleDistanceFieldContext*>(Context);

			if (Context->World->SweepSingleByChannel(HitResult, Origin, Origin + (FVector::UpVector * 0.001), FQuat::Identity, Ctx->CollisionChannel, CollisionShape, CollisionParams))
			{
				InContext->ProcessSweepHit(this);
			}
			else
			{
				InContext->ProcessSweepMiss(this);
			}
		}

	public:
		FHitResult HitResult;
	};
}
