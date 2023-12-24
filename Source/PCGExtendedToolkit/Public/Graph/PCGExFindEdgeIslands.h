// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExFindEdgeIslands.generated.h"

namespace PCGExGraph
{
	struct FEdgeNetwork;
	struct FEdgeCrossingsHandler;
}

UENUM(BlueprintType)
enum class EPCGExRoamingResolveMethod : uint8
{
	Overlap UMETA(DisplayName = "Overlap", ToolTip="Roaming nodes with unidirectional connections will create their own overlapping Islands."),
	Merge UMETA(DisplayName = "Merge", ToolTip="Roaming Islands will be merged into existing ones; thus creating less Islands yet not canon ones."),
	Cutoff UMETA(DisplayName = "Cutoff", ToolTip="Roaming Islands discovery will be cut off where they would otherwise overlap."),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgeIslandsSettings : public UPCGExGraphProcessorSettings, public IPCGExDebug
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgeIslands, "Graph : Find Edge Islands", "Create partitions from interconnected points. Each Island is the result of all input graphs combined.");
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Edge types to crawl to create a Island */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Removes roaming points from the output, and keeps only points that are part of an island. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

	/** Don't output Islands if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallIslands = true;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallIslands", ClampMin=2))
	int32 MinIslandSize = 3;

	/** Don't output Islands if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigIslands = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigIslands", ClampMin=2))
	int32 MaxIslandSize = 500;

	/** If two edges are close enough, create a "crossing" point. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFindCrossings = false;

	/** Distance at which segments are considered crossing. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFindCrossings", ClampMin=0.001))
	double CrossingTolerance = 10;

	/** Edges will inherit point attributes -- NOT IMPLEMENTED*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInheritAttributes = false;

	/** Name of the attribute to ouput the unique Island identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandIDAttributeName = "IslandID";

	/** Name of the attribute to output the Island size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandSizeAttributeName = "IslandSize";

	/** Cleanup graph socket data from output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDeleteGraphData = true;

	//~Begin IPCGExDebug interface
public:
#if WITH_EDITOR
	virtual bool IsDebugEnabled() const override { return bEnabled && bDebug; }
#endif
	//~End IPCGExDebug interface

#if WITH_EDITORONLY_DATA
	/** Edge debug display settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug", meta=(DisplayPriority=999999))
	FPCGExDebugEdgeSettings DebugEdgeSettings;
#endif

private:
	friend class FPCGExFindEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeIslandsContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExFindEdgeIslandsElement;

	virtual ~FPCGExFindEdgeIslandsContext() override;

	EPCGExEdgeType CrawlEdgeTypes;
	bool bPruneIsolatedPoints;
	bool bInheritAttributes;

	int32 MinIslandSize;
	int32 MaxIslandSize;

	bool bFindCrossings;
	double CrossingTolerance;

	int32 IslandUIndex = 0;

	TMap<int32, int32> IndexRemap;

	FName IslandIDAttributeName;
	FName IslandSizeAttributeName;
	FName PointUIDAttributeName;

	mutable FRWLock NetworkLock;
	PCGExGraph::FEdgeNetwork* EdgeNetwork = nullptr;
	PCGExGraph::FEdgeCrossingsHandler* EdgeCrossings = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;

	FPCGMetadataAttribute<int64>* InCachedIndex;

#if WITH_EDITOR
	PCGExGraph::FDebugEdgeData* DebugEdgeData = nullptr;
#endif

	virtual void Done() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeIslandsElement : public FPCGExGraphProcessorElement
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
