// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPackClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

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

namespace PCGExPackClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExPackClustersContext, UPCGExPackClustersSettings>
	{
	protected:
		TSharedPtr<TArray<int32>> ReducedVtxIndex;
		TSharedPtr<PCGExData::FPointIO> PackedIO;
		TSharedPtr<PCGExData::FFacade> PackedIOFacade;
		TSharedPtr<PCGEx::FAttributesInfos> VtxAttributes;

		int32 VtxStartIndex = -1;
		int32 NumIndices = -1;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};

}
