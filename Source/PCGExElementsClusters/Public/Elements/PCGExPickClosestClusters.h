// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Details/PCGExFilterDetails.h"

#include "Core/PCGExClustersProcessor.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "PCGExPickClosestClusters.generated.h"

UENUM()
enum class EPCGExClusterClosestPickMode : uint8
{
	OnlyBest = 0 UMETA(DisplayName = "Only Best", ToolTip="Allows duplicate picks for multiple targets"),
	NextBest = 1 UMETA(DisplayName = "Next Best", ToolTip="If a cluster was already the closest pick of another target, pick the nest best candidate."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/pick-closest-cluster"))
class UPCGExPickClosestClustersSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PickClosestClusters, "Cluster : Pick Closest", "Pick the clusters closest to input targets.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(ClusterOp); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Vtx;

	/** Whether to allow the same pick for multiple targets or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestPickMode PickMode = EPCGExClusterClosestPickMode::OnlyBest;

	/** Action type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFilterDataAction Action = EPCGExFilterDataAction::Keep;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TargetBoundsExpansion = 10;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bExpandSearchOutsideTargetBounds = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Action == EPCGExFilterDataAction::Tag"))
	FName KeepTag = NAME_None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Action == EPCGExFilterDataAction::Tag"))
	FName OmitTag = NAME_None;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeToTagDetails TargetAttributesToTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExForwardDetails TargetForwarding;

private:
	friend class FPCGExPickClosestClustersElement;
};

struct FPCGExPickClosestClustersContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExPickClosestClustersElement;

	TSharedPtr<PCGExData::FFacade> TargetDataFacade;

	FString KeepTag = TEXT("");
	FString OmitTag = TEXT("");

	FPCGExAttributeToTagDetails TargetAttributesToTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetForwardHandler;

	virtual void ClusterProcessing_InitialProcessingDone() override;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExPickClosestClustersElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PickClosestClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExPickClosestClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExPickClosestClustersContext, UPCGExPickClosestClustersSettings>
	{
		friend class FBatch;

	public:
		TArray<double> Distances;

		int32 Picker = -1;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		void Search();
		virtual void CompleteWork() override;
	};


	//


	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual void Output() override;
	};
}
