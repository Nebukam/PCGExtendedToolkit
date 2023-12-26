// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExBuildDelaunayGraph.generated.h"

namespace PCGExMesh
{
	struct FDelaunayTriangulation;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildDelaunayGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph, "Graph : Delaunay", "Create a 3D delaunay triangulation for each input dataset.");
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
	/** Name of the attribute to ouput the unique Island identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandIDAttributeName = "IslandID";

	/** Name of the attribute to output the Island size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandSizeAttributeName = "IslandSize";
	
private:
	friend class FPCGExBuildDelaunayGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraphContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraphElement;

	virtual ~FPCGExBuildDelaunayGraphContext() override;

	int32 IslandUIndex = 0;
	
	FName IslandIDAttributeName;
	FName IslandSizeAttributeName;
	FName PointUIDAttributeName;

	mutable FRWLock NetworkLock;
	PCGExMesh::FDelaunayTriangulation* Delaunay = nullptr;
	PCGExGraph::FEdgeNetwork* EdgeNetwork = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;
	PCGExData::FPointIO* CurrentIslandIO = nullptr;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;

};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraphElement : public FPCGExPointsProcessorElementBase
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

class PCGEXTENDEDTOOLKIT_API FDelaunayInsertTask : public FPCGExNonAbandonableTask
{
public:
	FDelaunayInsertTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}
	
	virtual bool ExecuteTask() override;
};

