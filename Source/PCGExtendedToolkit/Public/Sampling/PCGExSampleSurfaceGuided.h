// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "PCGExTexParamFactoryProvider.h"
#include "Data/PCGExDataForward.h"


#include "PCGExSampleSurfaceGuided.generated.h"

#if PCGEX_ENGINE_VERSION <= 503
#define PCGEX_FOREACH_FIELD_SURFACEGUIDED(MACRO)\
MACRO(Success, bool, false)\
MACRO(Location, FVector, FVector::ZeroVector)\
MACRO(LookAt, FVector, FVector::OneVector)\
MACRO(Normal, FVector, FVector::OneVector)\
MACRO(Distance, double, 0)\
MACRO(IsInside, bool, false)\
MACRO(UVCoords, FVector2D, FVector2D::ZeroVector)\
MACRO(FaceIndex, int32, -1)\
MACRO(ActorReference, FString, TEXT(""))\
MACRO(HitComponentReference, FString, TEXT(""))\
MACRO(PhysMat, FString, TEXT(""))\
MACRO(RenderMat, FString, TEXT(""))
#else
#define PCGEX_FOREACH_FIELD_SURFACEGUIDED(MACRO)\
MACRO(Success, bool, false)\
MACRO(Location, FVector, FVector::ZeroVector)\
MACRO(LookAt, FVector, FVector::OneVector)\
MACRO(Normal, FVector, FVector::OneVector)\
MACRO(Distance, double, 0)\
MACRO(IsInside, bool, false)\
MACRO(UVCoords, FVector2D, FVector2D::ZeroVector)\
MACRO(FaceIndex, int32, -1)\
MACRO(ActorReference, FSoftObjectPath, FSoftObjectPath())\
MACRO(HitComponentReference, FSoftObjectPath, FSoftObjectPath())\
MACRO(PhysMat, FSoftObjectPath, FSoftObjectPath())\
MACRO(RenderMat, FSoftObjectPath, FSoftObjectPath())
#endif

UENUM()
enum class EPCGExTraceSampleDistanceInput : uint8
{
	DirectionLength = 0 UMETA(DisplayName = "Direction Length", ToolTip="..."),
	Constant        = 1 UMETA(DisplayName = "Constant", ToolTip="Constant"),
	Attribute       = 2 UMETA(DisplayName = "Attribute", ToolTip="Attribute"),
};

class UPCGExFilterFactoryData;

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSampleSurfaceGuidedSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleSurfaceGuidedSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleSurfaceGuided, "Sample : Line Trace", "Find the collision point on the nearest collidable surface in a given direction.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Surface source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExSurfaceSource SurfaceSource = EPCGExSurfaceSource::ActorReferences;

	/** Name of the attribute to read actor reference from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SurfaceSource==EPCGExSurfaceSource::ActorReferences", EditConditionHides))
	FName ActorReference = FName("ActorReference");

	/** The origin of the trace */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGAttributePropertyInputSelector Origin;

	/** The direction to use for the trace */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGAttributePropertyInputSelector Direction;

	/** This UV Channel will be selected when retrieving UV Coordinates from a raycast query. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTraceSampleDistanceInput DistanceInput = EPCGExTraceSampleDistanceInput::Constant;

	/** Trace max distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DistanceInput==EPCGExTraceSampleDistanceInput::Constant", EditConditionHides, CLampMin=0.001))
	double MaxDistance = 1000;

	/** Attribute or property to read the local size from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DistanceInput==EPCGExTraceSampleDistanceInput::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalMaxDistance;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Success", PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sample location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** Name of the 'vector' attribute to write sampled Location to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Location", PCG_Overridable, EditCondition="bWriteLocation"))
	FName LocationAttributeName = FName("TracedLocation");

	/** Write the sample "look at" direction from the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAt = false;

	/** Name of the 'vector' attribute to write sampled LookAt to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="LookAt", PCG_Overridable, EditCondition="bWriteLookAt"))
	FName LookAtAttributeName = FName("TracedLookAt");

	/** Write the sampled normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the 'vector' attribute to write sampled Normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Normal", PCG_Overridable, EditCondition="bWriteNormal"))
	FName NormalAttributeName = FName("TracedNormal");

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("TracedDistance");

	/** Write the inside/outside status of the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsInside = false;

	/** Name of the 'bool' attribute to write sampled point inside or outside the collision.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="IsInside", PCG_Overridable, EditCondition="bWriteIsInside"))
	FName IsInsideAttributeName = FName("IsInside");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteUVCoords = false;

	/** Create an attribute for UV Coordinates of the surface hit. Note: Will only work in complex traces and must have 'Project Settings->Physics->Support UV From Hit Results' set to true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="UV Coords", PCG_Overridable, EditCondition="bWriteUVCoords"))
	FName UVCoordsAttributeName = FName("UVCoords");

	/** This UV Channel will be selected when retrieving UV Coordinates from a raycast query. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName=" └─ UV Channel", EditCondition = "bWriteUVCoords", EditConditionHides, HideEditConditionToggle))
	int32 UVChannel = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteFaceIndex = false;

	/** Create an attribute for index of the hit face. Note: Will only work in complex traces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Face Index", PCG_Overridable, EditCondition="bWriteFaceIndex"))
	FName FaceIndexAttributeName = FName("FaceIndex");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteActorReference = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="ActorReference", PCG_Overridable, EditCondition="bWriteActorReference"))
	FName ActorReferenceAttributeName = FName("ActorReference");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteHitComponentReference = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="HitComponent", PCG_Overridable, EditCondition="bWriteHitComponentReference"))
	FName HitComponentReferenceAttributeName = FName("HitComponent");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePhysMat = false;

	/** Name of the 'string' attribute to write actor reference to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="PhysMat", PCG_Overridable, EditCondition="bWritePhysMat"))
	FName PhysMatAttributeName = FName("PhysMat");

	/** Write the actor reference hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteRenderMat = false;

	/** Create an attribute for the render material. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(DisplayName="RenderMat", PCG_Overridable, EditCondition="bWriteRenderMat"))
	FName RenderMatAttributeName = FName("RenderMat");

	/** The index of the render material when it is queried from the hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta = (PCG_Overridable, DisplayName=" └─ Material Index", EditCondition = "bWriteRenderMat", EditConditionHides, HideEditConditionToggle))
	int32 RenderMaterialIndex = 0;

	/** Whether to extract texture parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta = (PCG_Overridable, DisplayName=" └─ Texture Parameters", EditCondition = "bWriteRenderMat", EditConditionHides, HideEditConditionToggle))
	bool bExtractTextureParameters = false;

	/** Which actor reference points attributes to forward on points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output (Actor Data)", meta=(EditCondition="SurfaceSource==EPCGExSurfaceSource::ActorReferences", EditConditionHides))
	FPCGExForwardDetails AttributesForwarding;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasSuccesses"))
	FString HasSuccessesTag = TEXT("HasSuccesses");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasNoSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasNoSuccesses"))
	FString HasNoSuccessesTag = TEXT("HasNoSuccesses");

	//

	/** If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether. Only uncheck this if you want to ensure existing attribute values are preserved. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bProcessFilteredOutAsFails = true;

	/** If enabled, points that failed to sample anything will be pruned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bPruneFailedSamples = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietUVSettingsWarning = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleSurfaceGuidedContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSurfaceGuidedElement;

	TSharedPtr<PCGExData::FFacade> ActorReferenceDataFacade;

	bool bSupportsUVQuery = false;
	bool bUseInclude = false;
	bool bExtractTextureParams = false;

	TMap<AActor*, int32> IncludedActors;

	FPCGExCollisionDetails CollisionSettings;

	TArray<TObjectPtr<const UPCGExTexParamFactoryData>> TexParamsFactories;

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleSurfaceGuidedElement final : public FPCGExPointsProcessorElement
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

namespace PCGExSampleSurfaceGuided
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSampleSurfaceGuidedContext, UPCGExSampleSurfaceGuidedSettings>
	{
		TArray<int8> SampleState;

		TSharedPtr<PCGExData::FDataForwardHandler> SurfacesForward;

		TSharedPtr<PCGExData::TBuffer<double>> MaxDistanceGetter;
		TSharedPtr<PCGExData::TBuffer<FVector>> DirectionGetter;
		TSharedPtr<PCGExData::TBuffer<FVector>> OriginGetter;

		TSharedPtr<PCGExMT::TScopedValue<double>> MaxDistanceValue;

		TSharedPtr<PCGExTexture::FLookup> TexParamLookup;

		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DECL)

		int8 bAnySuccess = 0;
		UWorld* World = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
