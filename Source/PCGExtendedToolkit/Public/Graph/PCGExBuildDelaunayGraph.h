// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExBuildDelaunayGraph.generated.h"

namespace PCGExGeo
{
	class TConvexHull3;
	class TDelaunayTriangulation3;
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
	PCGEX_NODE_INFOS(BuildDelaunayGraph, "Graph : Delaunay 3D", "Create a 3D delaunay tetrahedralization for each input dataset.");
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
	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

private:
	friend class FPCGExBuildDelaunayGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraphContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraphElement;

	virtual ~FPCGExBuildDelaunayGraphContext() override;

	int32 ClusterUIndex = 0;

	PCGExGeo::TDelaunayTriangulation3* Delaunay = nullptr;
	PCGExGeo::TConvexHull3* ConvexHull = nullptr;
	TSet<int32> HullIndices;

	PCGExGraph::FEdgeNetworkBuilder* NetworkBuilder = nullptr;
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
	void WriteEdges(FPCGExBuildDelaunayGraphContext* Context) const;
};
