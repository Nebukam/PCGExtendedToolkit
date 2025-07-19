// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExScopedContainers.h"


#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExGraph.h"
#include "PCGExConnectPoints.generated.h"

class UPCGExProbeFactoryData;
class FPCGExProbeOperation;

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/connect-points"))
class UPCGExConnectPointsSettings : public UPCGExPointsProcessorSettings
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
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
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
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(EPCGExMinimalAxis::X);
};

struct FPCGExConnectPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExConnectPointsElement;

	TArray<TObjectPtr<const UPCGExProbeFactoryData>> ProbeFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> GeneratorsFiltersFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> ConnectablesFiltersFactories;

	FVector CWCoincidenceTolerance = FVector::OneVector;
};

class FPCGExConnectPointsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ConnectPoints)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExConnectPoints
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExConnectPointsContext, UPCGExConnectPointsSettings>
	{
		TSharedPtr<PCGExPointFilter::FManager> GeneratorsFilter;
		TSharedPtr<PCGExPointFilter::FManager> ConnectableFilter;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TArray<TSharedPtr<FPCGExProbeOperation>> SearchProbes;
		TArray<TSharedPtr<FPCGExProbeOperation>> DirectProbes;
		TArray<TSharedPtr<FPCGExProbeOperation>> ChainProbeOperations;
		TArray<TSharedPtr<FPCGExProbeOperation>> SharedProbeOperations;

		bool bUseVariableRadius = false;
		int32 NumChainedOps = 0;
		double SharedSearchRadius = 0;

		TArray<int8> CanGenerate;
		TArray<int8> AcceptConnections;
		TUniquePtr<PCGEx::FIndexedItemOctree> Octree;

		TArray<FTransform> WorkingTransforms;

		TSharedPtr<PCGExMT::TScopedSet<uint64>> ScopedEdges;

		FPCGExGeo2DProjectionDetails ProjectionDetails;

		bool bPreventCoincidence = false;
		bool bUseProjection = false;
		FVector CWCoincidenceTolerance = FVector::OneVector;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void OnPreparationComplete();
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
		virtual void Output() override;

		virtual void Cleanup() override;
	};
}
