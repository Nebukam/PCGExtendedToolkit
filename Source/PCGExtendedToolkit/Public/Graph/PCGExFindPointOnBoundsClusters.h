// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Misc/PCGExFindPointOnBounds.h"
#include "PCGExFindPointOnBoundsClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindPointOnBoundsClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBoundsClusters, "Cluster : Find point on Bounds", "Find the closest vtx or edge on each cluster' bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputPin() const override { return PCGEx::OutputPointsLabel; }
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Node;

	/** Data output mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointOnBoundsOutputMode OutputMode = EPCGExPointOnBoundsOutputMode::Merged;

	/** UVW position of the target within bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UVW = FVector(-1, -1, 0);

	/** Offset to apply to the closest point, away from the bounds center. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Offset = 1;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bQuietAttributeMismatchWarning = false;

private:
	friend class FPCGExFindPointOnBoundsClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsClustersContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFindPointOnBoundsClustersElement;

	FPCGExCarryOverDetails CarryOverDetails;

	TArray<int32> BestIndices;
	TSharedPtr<PCGExData::FPointIO> MergedOut;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOMergeSources;
	TSharedPtr<PCGEx::FAttributesInfos> MergedAttributesInfos;

	virtual void ClusterProcessing_InitialProcessingDone() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsClustersElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFindPointOnBoundsClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindPointOnBoundsClustersContext, UPCGExFindPointOnBoundsClustersSettings>
	{
		mutable FRWLock BestIndexLock;

		FVector BestPosition = FVector::ZeroVector;
		FVector SearchPosition = FVector::ZeroVector;
		int32 BestIndex = -1;
		double BestDistance = MAX_dbl;

	public:
		int32 Picker = -1;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void UpdateCandidate(const FVector& InPosition, const int32 InIndex);
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
