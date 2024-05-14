// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExFilterClusters.generated.h"

class FPCGExPointIOMerger;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cluster Filter Mode"))
enum class EPCGExClusterFilterMode : uint8
{
	Keep UMETA(DisplayName = "Keep", ToolTip="Keeps only selected clusters"),
	Omit UMETA(DisplayName = "Omit", ToolTip="Omit selected clusters from output"),
};

namespace PCGExFilterCluster
{
	struct PCGEXTENDEDTOOLKIT_API FSelector
	{
		int32 VtxIndex = -1;
		int32 EdgesIndex = -1;
		double ClosestDistance = TNumericLimits<double>::Max();

		FVector Position;

		explicit FSelector(const FVector& InPosition)
			: Position(InPosition)
		{
		}
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExFilterClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FilterClusters, "Graph : Filter Clusters", "Filter clusters based on proximity to targets.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Node;

	/** What to do with the selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterFilterMode FilterMode = EPCGExClusterFilterMode::Keep;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFilterClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFilterClustersContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFilterClustersElement;

	virtual ~FPCGExFilterClustersContext() override;

	PCGExData::FPointIOCollection* TargetsCollection = nullptr;
	PCGExData::FPointIO* Targets = nullptr;
	TArray<PCGExFilterCluster::FSelector> Selectors;

	TMap<int32, TSet<int32>*> VtxEdgeMap;
	TSet<int32>* CurrentEdgeMap = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFilterClustersElement : public FPCGExEdgesProcessorElement
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
