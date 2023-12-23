// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExEdgeRelaxingOperation.h"

void UPCGExEdgeRelaxingOperation::PrepareForPointIO(PCGExData::FPointIO& PointIO)
{
	CurrentPoints = &PointIO;
}

void UPCGExEdgeRelaxingOperation::PrepareForMesh(PCGExData::FPointIO& EdgesIO, PCGExMesh::FMesh* MeshIO)
{
	CurrentEdges = &EdgesIO;
	CurrentMesh = MeshIO;
}

void UPCGExEdgeRelaxingOperation::PrepareForIteration(const int Iteration, TArray<FVector>* PrimaryBuffer, TArray<FVector>* SecondaryBuffer)
{
	CurrentIteration = Iteration;
	if (CurrentIteration % 2 == 0)
	{
		ReadBuffer = PrimaryBuffer;
		WriteBuffer = SecondaryBuffer;
	}
	else
	{
		ReadBuffer = SecondaryBuffer;
		WriteBuffer = PrimaryBuffer;
	}
}

void UPCGExEdgeRelaxingOperation::ProcessVertex(const PCGExMesh::FVertex& Vertex)
{
}

void UPCGExEdgeRelaxingOperation::WriteActiveBuffer(PCGExData::FPointIO& PointIO, PCGEx::FLocalSingleFieldGetter& Influence)
{
	TArray<FPCGPoint>& MutablePoints = PointIO.GetOut()->GetMutablePoints();
	for (int i = 0; i < WriteBuffer->Num(); i++)
	{
		FPCGPoint& OutPoint = MutablePoints[i];
		OutPoint.Transform.SetLocation(
			FMath::Lerp(OutPoint.Transform.GetLocation(), (*WriteBuffer)[i], Influence.SafeGet(i, DefaultInfluence)));
	}
}

void UPCGExEdgeRelaxingOperation::Cleanup()
{
	CurrentPoints = nullptr;
	CurrentEdges = nullptr;
	CurrentMesh = nullptr;
	ReadBuffer = nullptr;
	WriteBuffer = nullptr;

	Super::Cleanup();
}
