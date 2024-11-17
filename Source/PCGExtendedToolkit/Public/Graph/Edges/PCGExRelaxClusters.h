// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Relaxing/PCGExForceDirectedRelax.h"
#include "PCGExRelaxClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRelaxClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		RelaxClusters, "Cluster : Relax", "Relax point positions using edges connecting them.",
		(Relaxing ? FName(Relaxing.GetClass()->GetMetaData(TEXT("DisplayName"))) : FName("...")));
#endif

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 100;

	/** Influence Settings*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceDetails InfluenceDetails;

	/** Relaxing arithmetics */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExRelaxClusterOperation> Relaxing;

private:
	friend class FPCGExRelaxClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRelaxClustersContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExRelaxClustersElement;

	UPCGExRelaxClusterOperation* Relaxing = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRelaxClustersElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExRelaxClusters
{
	const FName SourceOverridesRelaxing = TEXT("Overrides : Relaxing");

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExRelaxClustersContext, UPCGExRelaxClustersSettings>
	{
		int32 Iterations = 10;

		TSharedPtr<PCGExData::TBuffer<double>> InfluenceCache;

		UPCGExRelaxClusterOperation* RelaxOperation = nullptr;

		TSharedPtr<TArray<FVector>> PrimaryBuffer;
		TSharedPtr<TArray<FVector>> SecondaryBuffer;

		FPCGExInfluenceDetails InfluenceDetails;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;
		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void StartRelaxIteration();
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void Write() override;
	};

	class FRelaxRangeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FRelaxRangeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                const TSharedPtr<FProcessor>& InProcessor):
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		TSharedPtr<FProcessor> Processor;
		uint64 Scope = 0;
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
