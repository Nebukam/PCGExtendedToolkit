// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExBuildConvexHull.generated.h"

namespace PCGExGeo
{
	class TDelaunayTriangulation3;
}

namespace PCGExMesh
{
	struct FDelaunayTriangulation;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildConvexHullSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull, "Graph : Convex Hull 3D", "Create a 3D Convex Hull triangulation for each input dataset.");
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
	/** Only exports the convex hull (truncate points output to fit edges) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bHullOnly = true;

	/** Mark points & edges that lie on the hull */	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, EditConditionHides, EditCondition="!bHullOnly"))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="!bHullOnly && bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="!bHullOnly"))
	bool bMarkEdgeOnTouch = false;
	
private:
	friend class FPCGExBuildConvexHullElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHullContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHullElement;

	virtual ~FPCGExBuildConvexHullContext() override;

	int32 IslandUIndex = 0;
	
	PCGExGeo::TDelaunayTriangulation3* Delaunay = nullptr;
	
	mutable FRWLock NetworkLock;
	PCGExGraph::FEdgeNetwork* EdgeNetwork = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;

};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHullElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	void ExportCurrent(FPCGExBuildConvexHullContext* Context) const;
	void ExportCurrentHullOnly(FPCGExBuildConvexHullContext* Context) const;
};

class PCGEXTENDEDTOOLKIT_API FHullInsertTask : public FPCGExNonAbandonableTask
{
public:
	FHullInsertTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}
	
	virtual bool ExecuteTask() override;
};

