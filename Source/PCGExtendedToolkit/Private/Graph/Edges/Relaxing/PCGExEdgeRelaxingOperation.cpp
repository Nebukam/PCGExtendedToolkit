// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExEdgeRelaxingOperation.h"

void UPCGExEdgeRelaxingOperation::PrepareForPointIO(PCGExData::FPointIO& PointIO)
{
	CurrentPoints = &PointIO;
}

void UPCGExEdgeRelaxingOperation::PrepareForCluster(PCGExData::FPointIO& EdgesIO, PCGExCluster::FCluster* InCluster)
{
	CurrentEdges = &EdgesIO;
	CurrentCluster = InCluster;
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

void UPCGExEdgeRelaxingOperation::ProcessVertex(const PCGExCluster::FNode& Vertex)
{
}

void UPCGExEdgeRelaxingOperation::ApplyInfluence(const PCGEx::FLocalSingleFieldGetter& Influence, TArray<FVector>* OverrideBuffer) const
{
	if (OverrideBuffer) { for (int i = 0; i < WriteBuffer->Num(); i++) { (*WriteBuffer)[i] = FMath::Lerp((*OverrideBuffer)[i], (*WriteBuffer)[i], Influence.SafeGet(i, DefaultInfluence)); } }
	else { for (int i = 0; i < WriteBuffer->Num(); i++) { (*WriteBuffer)[i] = FMath::Lerp((*ReadBuffer)[i], (*WriteBuffer)[i], Influence.SafeGet(i, DefaultInfluence)); } }
}

void UPCGExEdgeRelaxingOperation::WriteActiveBuffer(PCGExData::FPointIO& PointIO)
{
	TArray<FPCGPoint>& MutablePoints = PointIO.GetOut()->GetMutablePoints();
	for (int i = 0; i < WriteBuffer->Num(); i++)
	{
		FPCGPoint& OutPoint = MutablePoints[i];
		OutPoint.Transform.SetLocation((*WriteBuffer)[i]);
	}
}

void UPCGExEdgeRelaxingOperation::Cleanup()
{
	CurrentPoints = nullptr;
	CurrentEdges = nullptr;
	CurrentCluster = nullptr;
	ReadBuffer = nullptr;
	WriteBuffer = nullptr;

	Super::Cleanup();
}
