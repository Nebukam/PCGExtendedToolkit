// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExSanitizeClusters.generated.h"


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExSanitizeClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SanitizeClusters, "Graph : Sanitize Clusters", "Ensure the input set of vertex and edges outputs clean, interconnected clusters. May create new clusters, but does not creates nor deletes points/edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Graph & Edges output properties. Note that pruning isolated points is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExSanitizeClustersSettings;
	friend class FPCGExSanitizeClustersElement;

	virtual ~FPCGExSanitizeClustersContext() override;

	TArray<PCGExGraph::FGraphBuilder*> Builders;
	TArray<TMap<int64, int32>> EndpointsLookups;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersElement final : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClusterTask final : public FPCGExNonAbandonableTask
{
public:
	FPCGExSanitizeClusterTask(PCGExData::FPointIO* InPointIO,
	                          PCGExData::FPointIOTaggedEntries* InTaggedEdges) :
		FPCGExNonAbandonableTask(InPointIO),
		TaggedEdges(InTaggedEdges)
	{
	}

	PCGExData::FPointIOTaggedEntries* TaggedEdges = nullptr;

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSanitizeInsertTask final : public FPCGExNonAbandonableTask
{
public:
	FPCGExSanitizeInsertTask(PCGExData::FPointIO* InPointIO,
	                         PCGExData::FPointIO* InEdgeIO) :
		FPCGExNonAbandonableTask(InPointIO),
		EdgeIO(InEdgeIO)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;

	virtual bool ExecuteTask() override;
};
