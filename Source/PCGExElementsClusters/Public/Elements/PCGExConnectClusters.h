// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExClustersProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"

#include "PCGExConnectClusters.generated.h"

class FPCGExPointIOMerger;

UENUM()
enum class EPCGExBridgeClusterMethod : uint8
{
	Delaunay3D = 0 UMETA(DisplayName = "Delaunay 3D", ToolTip="Uses Delaunay 3D graph to find connections."),
	Delaunay2D = 1 UMETA(DisplayName = "Delaunay 2D", ToolTip="Uses Delaunay 2D graph to find connections."),
	LeastEdges = 2 UMETA(DisplayName = "Least Edges", ToolTip="Ensure all clusters are connected using the least possible number of bridges."),
	MostEdges  = 3 UMETA(DisplayName = "Most Edges", ToolTip="Each cluster will have a bridge to every other cluster"),
	Filters    = 4 UMETA(DisplayName = "Node Filters", ToolTip="Isolate nodes in each cluster as generators & connectable and connect by proximity.", Hidden),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/connect-clusters"))
class UPCGExConnectClustersSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConnectClusters, "Cluster : Connect", "Connects isolated edge clusters by their closest vertices, if they share the same vtx group.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(ClusterOp); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings


	/** Method used to find & insert bridges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Connect Method"))
	EPCGExBridgeClusterMethod BridgeMethod = EPCGExBridgeClusterMethod::Delaunay3D;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="BridgeMethod == EPCGExBridgeClusterMethod::Delaunay2D", EditConditionHides))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagVtxConnector = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bFlagVtxConnector"))
	FName VtxConnectorFlagName = "NumBridges";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagEdgeConnector = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bFlagEdgeConnector"))
	FName EdgeConnectorFlagName = "IsBridge";

	/** If enabled, won't throw a warning if no bridge could be created. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietNoBridgeWarning = false;

private:
	friend class FPCGExConnectClustersElement;
};

struct FPCGExConnectClustersContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExConnectClustersElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
	FPCGExCarryOverDetails CarryOverDetails;

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> GeneratorsFiltersFactories;
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> ConnectablesFiltersFactories;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExConnectClustersElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ConnectClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExConnectClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExConnectClustersContext, UPCGExConnectClustersSettings>
	{
	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	protected:
		const FPCGMetadataAttribute<int64>* InVtxEndpointAtt = nullptr;

		FPCGMetadataAttribute<int64>* EdgeEndpointsAtt = nullptr;
		FPCGMetadataAttribute<int64>* OutVtxEndpointAtt = nullptr;

		FPCGMetadataAttribute<int32>* VtxConnectorFlagAttribute = nullptr;
		FPCGMetadataAttribute<bool>* EdgeConnectorFlagAttribute = nullptr;

	public:
		TSharedPtr<PCGExData::FFacade> CompoundedEdgesDataFacade;
		TSharedPtr<FPCGExPointIOMerger> Merger;
		TSet<uint64> Bridges;
		TArray<uint64> BridgesList;
		TArray<int32> NewEdges;

		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
		void CreateBridge(int32 EdgeIndex, int32 FromClusterIndex, int32 ToClusterIndex);
	};
}
