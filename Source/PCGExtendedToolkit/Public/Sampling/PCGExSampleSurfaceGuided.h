// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExActorSelector.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleSurfaceGuided.generated.h"

#define PCGEX_FOREACH_FIELD_SURFACEGUIDED(MACRO)\
MACRO(Success, bool)\
MACRO(Location, FVector)\
MACRO(Normal, FVector)\
MACRO(Distance, double)

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
	PCGEX_NODE_INFOS(SampleSurfaceGuided, "Sample : Guided Trace", "Find the collision point on the nearest collidable surface in a given direction.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSampler; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** The direction to use for the trace */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGAttributePropertyInputSelector Direction;

	/** Trace max distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, CLampMin=0.001))
	double MaxDistance = 1000;

	/** Use a per-point maximum distance*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalMaxDistance = false;

	/** Attribute or property to read the local size from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalMaxDistance"))
	FPCGAttributePropertyInputSelector LocalMaxDistance;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sample location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** Name of the 'vector' attribute to write sampled Location to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteLocation"))
	FName LocationAttributeName = FName("TracedLocation");

	/** Write the sampled normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the 'vector' attribute to write sampled Normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteNormal"))
	FName NormalAttributeName = FName("TracedNormal");


	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("TracedDistance");

	/** Maximum distance to check for closest surface.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable))
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::Channel", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.ECollisionChannel"))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

	/** Collision object type to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::ObjectType", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.EObjectTypeQuery"))
	int32 CollisionObjectType = ObjectTypeQuery1;

	/** Collision profile to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::Profile", EditConditionHides))
	FName CollisionProfileName = NAME_None;

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable))
	bool bIgnoreSelf = true;

	/** Ignore a procedural selection of actors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bIgnoreActors = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable, EditCondition="bIgnoreActors"))
	FPCGExActorSelectorSettings IgnoredActorSelector;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSurfaceGuidedElement;

	virtual ~FPCGExSampleSurfaceGuidedContext() override;

	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	int32 CollisionObjectType;
	FName CollisionProfileName;

	bool bIgnoreSelf = true;
	TArray<AActor*> IgnoredActors;

	double MaxDistance;
	bool bUseLocalMaxDistance = false;
	PCGEx::FLocalSingleFieldGetter* MaxDistanceGetter = nullptr;
	PCGEx::FLocalVectorGetter* DirectionGetter = nullptr;

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DECL)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedElement : public FPCGExPointsProcessorElementBase
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

class PCGEXTENDEDTOOLKIT_API FTraceTask : public FPCGExPCGExCollisionTask
{
public:
	FTraceTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExPCGExCollisionTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
