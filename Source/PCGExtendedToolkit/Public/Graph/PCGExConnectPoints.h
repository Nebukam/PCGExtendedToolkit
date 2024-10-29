// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExGraph.h"
#include "PCGExConnectPoints.generated.h"

class UPCGExProbeFactoryBase;
class UPCGExProbeOperation;

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConnectPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConnectPoints, "Cluster : Connect Points", "Connect points according to a set of probes");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPreventCoincidence = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPreventCoincidence", ClampMin=0.00001, ClampMax=1))
	double CoincidenceTolerance = 0.001;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bProjectPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bProjectPoints", DisplayName="Project Points"))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConnectPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExConnectPointsElement;

	TArray<TObjectPtr<const UPCGExProbeFactoryBase>> ProbeFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> GeneratorsFiltersFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> ConnectablesFiltersFactories;

	FVector CWCoincidenceTolerance = FVector::OneVector;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConnectPointsElement final : public FPCGExPointsProcessorElement
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

namespace PCGExConnectPoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExConnectPointsContext, UPCGExConnectPointsSettings>
	{
		TSharedPtr<PCGExPointFilter::FManager> GeneratorsFilter;
		TSharedPtr<PCGExPointFilter::FManager> ConnectableFilter;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TArray<UPCGExProbeOperation*> ProbeOperations;
		TArray<UPCGExProbeOperation*> DirectProbeOperations;
		TArray<UPCGExProbeOperation*> ChainProbeOperations;
		TArray<UPCGExProbeOperation*> SharedProbeOperations;
		bool bUseVariableRadius = false;
		int32 NumChainedOps = 0;
		double SharedSearchRadius = MIN_dbl;

		TArray<int8> CanGenerate;
		TUniquePtr<PCGEx::FIndexedItemOctree> Octree;

		const TArray<FPCGPoint>* InPoints = nullptr;
		TArray<FTransform> CachedTransforms;

		TArray<TSharedPtr<TSet<uint64>>> DistributedEdgesSet;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		bool bPreventCoincidence = false;
		bool bUseProjection = false;
		FVector CWCoincidenceTolerance = FVector::OneVector;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void OnPreparationComplete();
		virtual void PrepareLoopScopesForPoints(const TArray<uint64>& Loops) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
