// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExGraph.h"
#include "PCGExEdgeRelaxingOperation.generated.h"

namespace PCGExMesh
{
	struct FVertex;
}

namespace PCGExMesh
{
	struct FMesh;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRelaxingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForPointIO(PCGExData::FPointIO& PointIO);
	virtual void PrepareForMesh(PCGExData::FPointIO& EdgesIO, PCGExMesh::FMesh*);
	virtual void PrepareForIteration(int Iteration, TArray<FVector>* PrimaryBuffer, TArray<FVector>* SecondaryBuffer);
	virtual void ProcessVertex(const PCGExMesh::FVertex& Vertex);

	virtual void Write(PCGExData::FPointIO& PointIO, PCGEx::FLocalSingleFieldGetter& Influence);

	double DefaultInfluence = 1;

	virtual void Cleanup() override;
	
protected:
	int32 CurrentIteration = 0;
	PCGExData::FPointIO* CurrentPoints = nullptr;
	PCGExData::FPointIO* CurrentEdges = nullptr;
	PCGExMesh::FMesh* CurrentMesh = nullptr;
	TArray<FVector>* ReadBuffer = nullptr;
	TArray<FVector>* WriteBuffer = nullptr;
};
