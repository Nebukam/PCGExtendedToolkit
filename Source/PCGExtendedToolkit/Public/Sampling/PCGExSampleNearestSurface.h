// Copyright 2024 Timothé Lapetite and contributors

// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGExDataForward.h"


#include "PCGExSampleNearestSurface.generated.h"

#if PCGEX_ENGINE_VERSION <= 503
#define PCGEX_FOREACH_FIELD_NEARESTSURFACE(MACRO)\
MACRO(Success, bool, false)\
MACRO(Location, FVector, FVector::ZeroVector)\
MACRO(LookAt, FVector, FVector::OneVector)\
MACRO(Normal, FVector, FVector::OneVector)\
MACRO(IsInside, bool, false)\
MACRO(Distance, double, 0)\
MACRO(ActorReference, FString, TEXT(""))\
MACRO(PhysMat, FString, TEXT(""))
#else
#define PCGEX_FOREACH_FIELD_NEARESTSURFACE(MACRO)\
MACRO(Success, bool, false)\
MACRO(Location, FVector, FVector::ZeroVector)\
MACRO(LookAt, FVector, FVector::OneVector)\
MACRO(Normal, FVector, FVector::OneVector)\
MACRO(IsInside, bool, false)\
MACRO(Distance, double, 0)\
MACRO(ActorReference, FSoftObjectPath, FSoftObjectPath())\
MACRO(PhysMat, FSoftObjectPath, FSoftObjectPath())
#endif

class UPCGExFilterFactoryBase;

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSampleNearestSurfaceSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestSurface, "Sample : Nearest Surface", "Find the closest point on the nearest collidable surface.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Surface source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExSurfaceSource SurfaceSource = EPCGExSurfaceSource::ActorReferences;

	/** Name of the attribute to read actor reference from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SurfaceSource==EPCGExSurfaceSource::ActorReferences", EditConditionHides))
	FName ActorReference = FName("ActorReference");

	/** Search max distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, CLampMin=0.001))
	double MaxDistance = 1000;

	/** Use a per-point maximum distance*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bUseLocalMaxDistance = false;

	/** Attribute or property to read the local max distance from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalMaxDistance"))
	FPCGAttributePropertyInputSelector LocalMaxDistance;

	/** Write whether the sampling was successful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Success", PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sample location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** Name of the 'vector' attribute to write sampled Location to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Location", PCG_Overridable, EditCondition="bWriteLocation"))
	FName LocationAttributeName = FName("NearestLocation");

	/** Write the sample "look at" direction from the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAt = false;

	/** Name of the 'vector' attribute to write sampled LookAt to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="LookAt", PCG_Overridable, EditCondition="bWriteLookAt"))
	FName LookAtAttributeName = FName("NearestLookAt");


	/** Write the sampled normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the 'vector' attribute to write sampled Normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Normal", PCG_Overridable, EditCondition="bWriteNormal"))
	FName NormalAttributeName = FName("NearestNormal");


	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("NearestDistance");

	/** Write the inside/outside status of the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsInside = false;

	/** Name of the 'bool' attribute to write sampled point inside or outside the collision.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="IsInside", PCG_Overridable, EditCondition="bWriteIsInside"))
	FName IsInsideAttributeName = FName("IsInside");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteActorReference = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="ActorReference", PCG_Overridable, EditCondition="bWriteActorReference"))
	FName ActorReferenceAttributeName = FName("ActorReference");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePhysMat = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="PhysMat", PCG_Overridable, EditCondition="bWritePhysMat"))
	FName PhysMatAttributeName = FName("PhysMat");

	/** Which actor reference points attributes to forward on points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="SurfaceSource==EPCGExSurfaceSource::ActorReferences", EditConditionHides))
	FPCGExForwardDetails AttributesForwarding;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasSuccesses"))
	FString HasSuccessesTag = TEXT("HasSuccesses");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasNoSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasNoSuccesses"))
	FString HasNoSuccessesTag = TEXT("HasNoSuccesses");

	//

	/** If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether. Only uncheck this if you want to ensure existing attribute values are preserved. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bProcessFilteredOutAsFails = true;

	/** If enabled, points that failed to sample anything will be pruned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bPruneFailedSamples = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestSurfaceContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestSurfaceElement;

	TSharedPtr<PCGExData::FFacade> ActorReferenceDataFacade;

	FPCGExCollisionDetails CollisionSettings;

	bool bUseInclude = false;
	TMap<AActor*, int32> IncludedActors;
	TArray<UPrimitiveComponent*> IncludedPrimitives;

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestSurfaceElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSampleNearestSurface
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSampleNearestSurfaceContext, UPCGExSampleNearestSurfaceSettings>
	{
		TArray<int8> SampleState;

		TSharedPtr<PCGExData::FDataForwardHandler> SurfacesForward;

		TSharedPtr<PCGExData::TBuffer<double>> MaxDistanceGetter;

		PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_DECL)

		int8 bAnySuccess = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
