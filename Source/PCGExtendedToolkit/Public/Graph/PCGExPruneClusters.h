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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph", Hidden)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPruneClustersSettings : public UPCGExEdgesProcessorSettings
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

	/** Bounds type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Epsilon value used to expand the box when testing if IsInside. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double InsideEpsilon = 1e-4;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPruneClustersSettings;
	friend class FPCGExPruneClustersElement;

	TSharedPtr<PCGExGeo::FPointBoxCloud> BoxCloud;
	TArray<bool> ClusterState;

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneClustersElement final : public FPCGExEdgesProcessorElement
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

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneClusterTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExPruneClusterTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
	                       const TSharedPtr<PCGExData::FPointIO>& InEdgesIO) :
		FPCGExTask(InPointIO),
		EdgesIO(InEdgesIO)
	{
	}

	const TSharedPtr<PCGExData::FPointIO> EdgesIO;

	virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
