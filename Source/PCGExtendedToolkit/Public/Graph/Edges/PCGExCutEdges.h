// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Paths/PCGExPaths.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "PCGExCutEdges.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cut Edges Mode"))
enum class EPCGExCutEdgesMode : uint8
{
	Cut = 0 UMETA(DisplayName = "Remove Edges", ToolTip="Remove edges cut by the paths"),
	Crossings   = 1 UMETA(DisplayName = "Crossings", ToolTip="Add crossings nodes where the edges intersect with the paths"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cut Edges Node Handling Mode"))
enum class EPCGExCutEdgesNodeHandlingMode : uint8
{
	Ignore = 0 UMETA(DisplayName = "Ignore", ToolTip="Nodes are ignored"),
	Remove = 1 UMETA(DisplayName = "Remove", ToolTip="Remove nodes that are collinear to paths"),
};

namespace PCGExCutEdges
{
	const FName SourceNodeFilters = FName("NodeFilters");
	const FName SourceEdgeFilters = FName("EdgeFilters");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCutEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CutEdges, "Cluster : Cut Edges", "Refine clusters using edge/paths intersections.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCutEdgesMode Mode = EPCGExCutEdgesMode::Cut;

	/** Graph & Edges output properties */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!Mode==", EditConditionHides))
	EPCGExCutEdgesNodeHandlingMode NodeHandling = EPCGExCutEdgesNodeHandlingMode::Ignore;

	/** Closed loop handling.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPathClosedLoopDetails ClosedLoop;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode==EPCGExCutEdgesMode::Crossing"))
	FPCGExPathEdgeIntersectionDetails IntersectionDetails = FPCGExPathEdgeIntersectionDetails(false);

	/** Blending applied on intersecting points along the path prev and next point. This is different from inheriting from external properties. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, EditCondition="Mode==EPCGExCutEdgesMode::Crossing", ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable, EditCondition="Mode==EPCGExCutEdgesMode::Crossing"))
	bool bDoCrossBlending = false;

	/** If enabled, blend in properties & attributes from external sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable, EditCondition="bDoCrossBlending && Mode==EPCGExCutEdgesMode::Crossing"))
	FPCGExCarryOverDetails CrossingCarryOver;

	/** If enabled, blend in properties & attributes from external sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cross Blending", meta=(PCG_Overridable, EditCondition="bDoCrossBlending && Mode==EPCGExCutEdgesMode::Crossing"))
	FPCGExBlendingDetails CrossingBlending = FPCGExBlendingDetails(EPCGExDataBlendingType::Average, EPCGExDataBlendingType::None);

	FPCGExDistanceDetails CrossingBlendingDistance;

	
	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExCutEdgesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCutEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExCutEdgesElement;

	FPCGExPathClosedLoopDetails ClosedLoop;
	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	UPCGExSubPointsBlendOperation* Blending = nullptr;
	TSharedPtr<PCGEx::FAttributesInfos> PathsAttributesInfos;

	FPCGExBlendingDetails CrossingBlending;
	
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> EdgeFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> NodeFilterFactories;

	TArray<TSharedRef<PCGExData::FFacade>> PathFacades;
	TArray<TSharedRef<PCGExPaths::FPath>> Paths;
	TArray<TSharedPtr<PCGExData::FFacadePreloader>> PathsPreloaders;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCutEdgesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExCutEdges
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExCutEdgesContext, UPCGExCutEdgesSettings>
	{
	protected:
		TSharedPtr<PCGExClusterFilter::FManager> EdgeFilterManager;
		TSharedPtr<PCGExClusterFilter::FManager> NodeFilterManager;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;
		TArray<bool> EdgeFilterCache;

	public:
		
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		
		virtual void PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch<FProcessor>(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)
			bRequiresGraphBuilder = Settings->Mode != EPCGExCutEdgesMode::Crossings; // If crossing, just insert new data like ConnectCluster does
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;

		
	};

}
