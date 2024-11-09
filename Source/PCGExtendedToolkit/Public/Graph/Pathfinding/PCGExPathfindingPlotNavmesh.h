// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathfinding.h"
#include "PCGExPathfindingNavmesh.h"
#include "PCGExPointsProcessor.h"
#include "AI/Navigation/NavigationTypes.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#include "PCGExPathfindingPlotNavmesh.generated.h"

namespace PCGExPathfinding
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FPlotPoint
	{
		int32 PlotIndex;
		FVector Position;
		PCGMetadataEntryKey MetadataEntryKey = -1;

		FPlotPoint(const int32 InPlotIndex, const FVector& InPosition, const PCGMetadataEntryKey InMetadataEntryKey)
			: PlotIndex(InPlotIndex), Position(InPosition), MetadataEntryKey(InMetadataEntryKey)
		{
		}
	};
}

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathfindingPlotNavmeshSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PCGExPathfindingPlotNavmesh, "Pathfinding : Plot Navmesh", "Extract a single paths from navmesh, going through each seed points in order.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;

	virtual FName GetMainInputLabel() const override { return PCGExGraph::SourcePlotsLabel; }
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputPathsLabel; }
	//~End UPCGExPointsProcessorSettings


	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddSeedToPath = true;

	/** Add goal point at the end of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddGoalToPath = true;

	/** Insert plot points inside the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddPlotPointsToPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bClosedLoop = false;

	/** Whether the pathfinding requires a naviguable end location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bRequireNavigableEndLocation = true;

	/** Fuse sub points by distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001))
	double FuseDistance = 10;

	/** Controls how path points blend from seed to goal. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	/** Pathfinding mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathfindingNavmeshMode PathfindingMode = EPCGExPathfindingNavmeshMode::Regular;

	/** Nav agent to be used by the nav system. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FNavAgentProperties NavAgentProperties;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitCompletePathOnFailedPlot = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingPlotNavmeshContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPathfindingPlotNavmeshElement;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;
	bool bAddPlotPointsToPath = true;

	FNavAgentProperties NavAgentProperties;

	bool bRequireNavigableEndLocation = true;
	EPCGExPathfindingNavmeshMode PathfindingMode;
	double FuseDistance = 10;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingPlotNavmeshElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPlotNavmeshTask final : public PCGExMT::FPCGExTask
{
public:
	explicit FPCGExPlotNavmeshTask(
		const TSharedPtr<PCGExData::FPointIO>& InPointIO) :
		FPCGExTask(InPointIO)
	{
	}

	virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
