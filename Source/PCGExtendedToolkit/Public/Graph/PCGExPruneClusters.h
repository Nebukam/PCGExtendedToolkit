// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExPruneClusters.generated.h"


namespace PCGExGeo
{
	class FPointBoxCloud;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPruneClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneClusters, "Cluster : Prune", "Prune entire clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** What to do with the selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExPointFilterActionDetails FilterActions;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPruneClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPruneClustersSettings;
	friend class FPCGExPruneClustersElement;

	virtual ~FPCGExPruneClustersContext() override;

	PCGExGeo::FPointBoxCloud* BoxCloud = nullptr;
	TArray<bool> ClusterState;

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneClustersElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneClusterTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExPruneClusterTask(PCGExData::FPointIO* InPointIO,
	                       PCGExData::FPointIO* InEdgesIO) :
		FPCGExTask(InPointIO),
		EdgesIO(InEdgesIO)
	{
	}

	PCGExData::FPointIO* EdgesIO = nullptr;

	virtual bool ExecuteTask() override;
};
