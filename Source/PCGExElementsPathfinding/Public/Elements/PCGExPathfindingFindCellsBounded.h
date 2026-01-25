// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/Artifacts/PCGExCellDetails.h"
#include "Containers/PCGExScopedContainers.h"

#include "Core/PCGExClustersProcessor.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "PCGExPathfindingFindAllCellsBounded.h"

#include "PCGExPathfindingFindCellsBounded.generated.h"

namespace PCGExClusters
{
	class FProjectedPointSet;
	class FCellConstraints;
	class FCellPathBuilder;
	class FCell;
}

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}

namespace PCGExFindContoursBounded
{
	class FProcessor;

	const FName SourceBoundsLabel = FName("Bounds");
	const FName OutputGoodSeedsLabel = TEXT("SeedGenSuccess");
	const FName OutputBadSeedsLabel = TEXT("SeedGenFailed");

	const FName OutputPathsInsideLabel = FName("Paths : Inside");
	const FName OutputPathsTouchingLabel = FName("Paths : Touching");
	const FName OutputPathsOutsideLabel = FName("Paths : Outside");

	const FName OutputBoundsInsideLabel = FName("Bounds : Inside");
	const FName OutputBoundsTouchingLabel = FName("Bounds : Touching");
	const FName OutputBoundsOutsideLabel = FName("Bounds : Outside");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="pathfinding/contours/find-contours-bounded"))
class UPCGExFindContoursBoundedSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindContoursBounded, "Pathfinding : Find Cells (Bounded)", "Finds closed cells around seed points and triages them by spatial bounds relationship (Inside/Touching/Outside).");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Pathfinding); }
#endif

protected:
	virtual bool HasDynamicPins() const override { return true; }
	virtual bool OutputPinsCanBeDeactivated() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** How to output triaged cells */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExCellTriageOutput OutputMode = EPCGExCellTriageOutput::Separate;

	/** Which cell categories to output (Inside/Touching/Outside) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, Bitmask, BitmaskEnum = "/Script/PCGExGraphs.EPCGExCellTriageFlags"))
	uint8 TriageFlags = static_cast<uint8>(PCGExCellTriage::DefaultFlags);

	FORCEINLINE bool OutputInside() const { return !!(TriageFlags & static_cast<uint8>(EPCGExCellTriageFlags::Inside)); }
	FORCEINLINE bool OutputTouching() const { return !!(TriageFlags & static_cast<uint8>(EPCGExCellTriageFlags::Touching)); }
	FORCEINLINE bool OutputOutside() const { return !!(TriageFlags & static_cast<uint8>(EPCGExCellTriageFlags::Outside)); }

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails Constraints = FPCGExCellConstraintsDetails(true);

	/** Cell output settings (output mode, attributes, OBB settings) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellArtifactsDetails Artifacts;

	/** Output a filtered set of points containing only seeds that generated a valid path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOutputFilteredSeeds = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOutputFilteredSeeds"))
	FPCGExCellSeedMutationDetails SeedMutations = FPCGExCellSeedMutationDetails(true);

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFindContoursBoundedElement;
};

struct FPCGExFindContoursBoundedContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExFindContoursBoundedElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExCellArtifactsDetails Artifacts;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;

	FBox BoundsFilter = FBox(ForceInit);

	// Separate output collections for each triage category
	TSharedPtr<PCGExData::FPointIOCollection> OutputPathsInside;
	TSharedPtr<PCGExData::FPointIOCollection> OutputPathsTouching;
	TSharedPtr<PCGExData::FPointIOCollection> OutputPathsOutside;

	TSharedPtr<PCGExData::FPointIOCollection> OutputCellBoundsInside;
	TSharedPtr<PCGExData::FPointIOCollection> OutputCellBoundsTouching;
	TSharedPtr<PCGExData::FPointIOCollection> OutputCellBoundsOutside;

	TSharedPtr<PCGExData::FPointIO> GoodSeeds;
	TSharedPtr<PCGExData::FPointIO> BadSeeds;

	TArray<int8> SeedQuality;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExFindContoursBoundedElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindContoursBounded)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExFindContoursBounded
{
	// Use shared triage result enum from PCGExCellDetails.h
	using ECellTriageResult = EPCGExCellTriageResult;

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindContoursBoundedContext, UPCGExFindContoursBoundedSettings>
	{
	protected:
		TSharedPtr<PCGExClusters::FProjectedPointSet> Seeds;
		TSharedPtr<PCGExClusters::FCellPathBuilder> CellProcessor;
		TArray<TSharedPtr<PCGExClusters::FCell>> EnumeratedCells;
		TArray<TSharedPtr<PCGExClusters::FCell>> AllCellsIncludingFailed;
		TSharedPtr<PCGExClusters::FCell> WrapperCell;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<PCGExClusters::FCell>>> ScopedValidCells;

		TArray<TSharedPtr<PCGExClusters::FCell>> CellsInside;
		TArray<TSharedPtr<PCGExClusters::FCell>> CellsTouching;
		TArray<TSharedPtr<PCGExClusters::FCell>> CellsOutside;

		TArray<TSharedPtr<PCGExData::FPointIO>> CellsIOInside;
		TArray<TSharedPtr<PCGExData::FPointIO>> CellsIOTouching;
		TArray<TSharedPtr<PCGExData::FPointIO>> CellsIOOutside;

		// For Combined mode tagging
		TArray<FString> CellTagsInside;
		TArray<FString> CellTagsTouching;
		TArray<FString> CellTagsOutside;

	public:
		TSharedPtr<PCGExClusters::FCellConstraints> CellsConstraints;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		void HandleWrapperOnlyCase(const int32 NumSeeds);

		virtual void Cleanup() override;

	protected:
		ECellTriageResult ClassifyCell(const TSharedPtr<PCGExClusters::FCell>& InCell) const;
	};
}
