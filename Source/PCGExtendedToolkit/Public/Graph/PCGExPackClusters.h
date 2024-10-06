// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPackClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPackClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PackClusters, "Cluster : Pack", "Pack each cluster into an single point data object containing both vtx and edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

private:
	friend class FPCGExPackClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackClustersContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExPackClustersElement;
	friend class FPCGExCreateBridgeTask;

	TSharedPtr<PCGExData::FPointIOCollection> PackedClusters;
	FPCGExCarryOverDetails CarryOverDetails;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackClustersElement final : public FPCGExEdgesProcessorElement
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

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackClusterTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExPackClusterTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
	                      const TSharedPtr<PCGExData::FPointIO>& InInEdges,
	                      const TMap<uint32, int32>& InEndpointsLookup) :
		FPCGExTask(InPointIO),
		InEdges(InInEdges),
		EndpointsLookup(InEndpointsLookup)
	{
	}

	TSharedPtr<PCGExData::FPointIO> InEdges;
	TMap<uint32, int32> EndpointsLookup;

	virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
