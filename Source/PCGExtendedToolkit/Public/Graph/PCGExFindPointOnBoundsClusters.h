﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Misc/PCGExFindPointOnBounds.h"
#include "PCGExFindPointOnBoundsClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/find-point-on-bounds"))
class UPCGExFindPointOnBoundsClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBoundsClusters, "Cluster : Find point on Bounds", "Find the closest vtx or edge on each cluster' bounds.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorClusterOp); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputPin() const override { return PCGPinConstants::DefaultOutputLabel; }
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Vtx;

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

struct FPCGExFindPointOnBoundsClustersContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFindPointOnBoundsClustersElement;

	FPCGExCarryOverDetails CarryOverDetails;

	TArray<int32> BestIndices;
	TSharedPtr<PCGExData::FPointIO> MergedOut;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOMergeSources;
	TSharedPtr<PCGEx::FAttributesInfos> MergedAttributesInfos;

	virtual void ClusterProcessing_InitialProcessingDone() override;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExFindPointOnBoundsClustersElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindPointOnBoundsClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExFindPointOnBoundsClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindPointOnBoundsClustersContext, UPCGExFindPointOnBoundsClustersSettings>
	{
		mutable FRWLock BestIndexLock;

		double BestDistance = MAX_dbl;
		FVector BestPosition = FVector::ZeroVector;
		FVector SearchPosition = FVector::ZeroVector;
		int32 BestIndex = -1;

	public:
		int32 Picker = -1;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void UpdateCandidate(const FVector& InPosition, const int32 InIndex);
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
