// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExActorSelector.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

#include "PCGExSampleSurfaceGuided.generated.h"

#define PCGEX_FOREACH_FIELD_SURFACEGUIDED(MACRO)\
MACRO(Success, bool)\
MACRO(Location, FVector)\
MACRO(Normal, FVector)\
MACRO(Distance, double)\
MACRO(ActorReference, FString)\
MACRO(PhysMat, FString)

class UPCGExFilterFactoryBase;

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleSurfaceGuidedSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleSurfaceGuided, "Sample : Guided Trace", "Find the collision point on the nearest collidable surface in a given direction.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	virtual FName GetPointFilterLabel() const override;
	//~End UPCGExPointsProcessorSettings

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

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteActorReference = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, EditCondition="bWriteActorReference"))
	FName ActorReferenceAttributeName = FName("ActorReference");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePhysMat = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, EditCondition="bWritePhysMat"))
	FName PhysMatAttributeName = FName("PhysMat");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision", meta=(PCG_Overridable))
	bool bTraceComplex = false;

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

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSurfaceGuidedElement;

	virtual ~FPCGExSampleSurfaceGuidedContext() override;

	TArray<AActor*> IgnoredActors;

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DECL_TOGGLE)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleSurfaceGuidedElement final : public FPCGExPointsProcessorElement
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

namespace PCGExSampleSurfaceGuided
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExData::FCache<double>* MaxDistanceGetter = nullptr;
		PCGExData::FCache<FVector>* DirectionGetter = nullptr;

		FPCGExSampleSurfaceGuidedContext* LocalTypedContext = nullptr;
		const UPCGExSampleSurfaceGuidedSettings* LocalSettings = nullptr;

		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
