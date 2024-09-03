// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"
#include "AssetSelectors/PCGExMeshCollection.h"

#include "Tangents/PCGExTangentsOperation.h"
#include "Components/SplineMeshComponent.h"

#include "PCGExPathSplineMesh.generated.h"

#define PCGEX_FOREACH_SPLINE_PARAM(MACRO) \
MACRO(StartScale, FVector) \
MACRO(StartOffset, FVector) \
MACRO(StartRoll, double) \
MACRO(EndScale, FVector) \
MACRO(EndOffset, FVector) \
MACRO(EndRoll, FVector)

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplineParamsMapping
{
	GENERATED_BODY()

	FPCGExSplineParamsMapping()
	{
	}

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bLocalParam = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathSplineMeshSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSplineMesh, "Path : Spline Mesh", "Create spline mesh components from paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollectionSource CollectionSource = EPCGExCollectionSource::Asset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::Asset", EditConditionHides))
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::AttributeSet", EditConditionHides))
	FPCGExAssetAttributeSetDetails AttributeSetDetails;

	/** Distribution details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAssetDistributionDetails DistributionSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta = (PCG_Overridable))
	//bool bPerSegmentTargetActor = false;

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta=(PCG_Overridable, EditCondition="bPerSegmentTargetActor", EditConditionHides))
	//FName TargetActorAttributeName;

	/** Whether to read tangents from attributes or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta = (PCG_Overridable))
	bool bTangentsFromAttributes = false;

	/** Arrive tangent attribute (will be broadcast to FVector under the hood) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta=(PCG_Overridable, EditCondition="bTangentsFromAttributes", EditConditionHides))
	FPCGAttributePropertyInputSelector Arrive;

	/** Leave tangent attribute (will be broadcast to FVector under the hood) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta=(PCG_Overridable, EditCondition="bTangentsFromAttributes", EditConditionHides))
	FPCGAttributePropertyInputSelector Leave;

	/** In-place tangent solver */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Tangents", Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault, EditCondition="!bTangentsFromAttributes", EditConditionHides))
	TObjectPtr<UPCGExTangentsOperation> Tangents;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSplineMeshContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathSplineMeshElement;

	virtual ~FPCGExPathSplineMeshContext() override;
	virtual void RegisterAssetDependencies() override;

	UPCGExTangentsOperation* Tangents = nullptr;
	TSet<AActor*> NotifyActors;

	TObjectPtr<UPCGExAssetCollection> MainCollection;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSplineMeshElement final : public FPCGExPathProcessorElement
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

namespace PCGExPathSplineMesh
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExPathSplineMeshContext* LocalTypedContext = nullptr;
		const UPCGExPathSplineMeshSettings* LocalSettings = nullptr;

		bool bClosedPath = false;

		int32 LastIndex = 0;

		PCGExAssetCollection::FDistributionHelper* Helper = nullptr;

		PCGExData::FCache<FVector>* ArriveReader = nullptr;
		PCGExData::FCache<FVector>* LeaveReader = nullptr;

		UPCGExTangentsOperation* Tangents = nullptr;

		TArray<PCGExPaths::FSplineMeshSegment> Segments;
		//TArray<USplineMeshComponent*> SplineMeshComponents;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;

		virtual void Output() override;
	};
}
