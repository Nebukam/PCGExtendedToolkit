// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetails.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPickClosestClusters.generated.h"

class FPCGExPointIOMerger;

namespace PCGExFilterCluster
{
	struct PCGEXTENDEDTOOLKIT_API FPicker
	{
		int32 VtxIndex = -1;
		int32 EdgesIndex = -1;
		double ClosestDistance = TNumericLimits<double>::Max();

		FVector Position;

		explicit FPicker(const FVector& InPosition)
			: Position(InPosition)
		{
		}
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExPickClosestClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PickClosestClusters, "Cluster : Pick Closest", "Pick the clusters closest to input targets.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Node;

	/** What to do with the selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExPointFilterActionDetails FilterActions;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExPickClosestClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPickClosestClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPickClosestClustersElement;

	virtual ~FPCGExPickClosestClustersContext() override;

	PCGExData::FPointIO* Targets = nullptr;
	TArray<PCGExFilterCluster::FPicker> Selectors;

	TMap<int32, TSet<int32>*> VtxEdgeMap;
	TSet<int32>* CurrentEdgeMap = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPickClosestClustersElement final : public FPCGExEdgesProcessorElement
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
