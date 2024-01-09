// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExBuildConvexHull2D.generated.h"

namespace PCGExGeo
{
	class TConvexHull2;
	class TDelaunayTriangulation2;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildConvexHull2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull2D, "Graph : Convex Hull 2D", "Create a 2D Convex Hull triangulation for each input dataset.");
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
	/** Removes points that are not on the hull from the Vtx output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPrunePoints = true;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, EditCondition="!bPrunePoints"))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bPrunePoints && bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

private:
	friend class FPCGExBuildConvexHull2DElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHull2DContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHull2DElement;

	virtual ~FPCGExBuildConvexHull2DContext() override;

	int32 ClusterUIndex = 0;

	PCGExGeo::TConvexHull2* ConvexHull = nullptr;
	TSet<int32> HullIndices;
	TMap<int32, int32> IndicesRemap;

	mutable FRWLock NetworkLock;
	PCGExGraph::FEdgeNetwork* EdgeNetwork = nullptr;
	PCGExData::FPointIOGroup* ClustersIO;
	PCGExData::FPointIOGroup* PathsIO;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHull2DElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	void WriteEdges(FPCGExBuildConvexHull2DContext* Context) const;
};

class PCGEXTENDEDTOOLKIT_API FHull2DInsertTask : public FPCGExNonAbandonableTask
{
public:
	FHull2DInsertTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
