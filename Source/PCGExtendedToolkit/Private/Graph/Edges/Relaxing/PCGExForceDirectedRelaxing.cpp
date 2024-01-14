// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExForceDirectedRelaxing.h"

#include "Graph/PCGExCluster.h"

void UPCGExForceDirectedRelaxing::ProcessVertex(const PCGExCluster::FNode& Vertex)
{
	const FVector Position = (*ReadBuffer)[Vertex.PointIndex];
	FVector Force = FVector::Zero();

	for (const int32 VtxIndex : Vertex.AdjacentNodes)
	{
		const PCGExCluster::FNode& OtherVtx = CurrentCluster->Nodes[VtxIndex];
		const FVector OtherPosition = (*ReadBuffer)[OtherVtx.PointIndex];
		CalculateAttractiveForce(Force, Position, OtherPosition);
		CalculateRepulsiveForce(Force, Position, OtherPosition);
	}

	(*WriteBuffer)[Vertex.PointIndex] = Position + Force;
}

void UPCGExForceDirectedRelaxing::CalculateAttractiveForce(FVector& Force, const FVector& A, const FVector& B) const
{
	// Calculate the displacement vector between the nodes
	FVector Displacement = B - A;

	const double Distance = FMath::Max(Displacement.Length(), 1e-5);
	Displacement /= Distance;

	// Calculate the force magnitude using Hooke's law
	const double ForceMagnitude = SpringConstant * Distance;
	Force += Displacement * ForceMagnitude;
}

void UPCGExForceDirectedRelaxing::CalculateRepulsiveForce(FVector& Force, const FVector& A, const FVector& B) const
{
	// Calculate the displacement vector between the nodes
	FVector Displacement = B - A;

	const double Distance = FMath::Max(Displacement.Length(), 1e-5);
	Displacement /= Distance;

	// Calculate the force magnitude using Coulomb's law
	const double ForceMagnitude = ElectrostaticConstant / (Distance * Distance);
	Force -= Displacement * ForceMagnitude;
}
