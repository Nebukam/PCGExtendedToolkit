// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relaxations/PCGExForceDirectedRelax.h"

#pragma region UPCGExForceDirectedRelax

void UPCGExForceDirectedRelax::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExForceDirectedRelax* TypedOther = Cast<UPCGExForceDirectedRelax>(Other))
	{
		SpringConstant = TypedOther->SpringConstant;
		ElectrostaticConstant = TypedOther->ElectrostaticConstant;
	}
}

void UPCGExForceDirectedRelax::Step1(const PCGExClusters::FNode& Node)
{
	const FVector Position = (ReadBuffer->GetData() + Node.Index)->GetLocation();
	FVector Force = FVector::ZeroVector;

	// Attractive forces: only between connected nodes (edges act as springs)
	for (const PCGExGraphs::FLink& Lk : Node.Links)
	{
		const FVector OtherPosition = (ReadBuffer->GetData() + Lk.Node)->GetLocation();
		CalculateAttractiveForce(Force, Position, OtherPosition);
	}

	// Repulsive forces: between ALL node pairs (electrostatic repulsion)
	for (int32 OtherNodeIndex = 0; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
	{
		if (OtherNodeIndex == Node.Index) { continue; }
		const FVector OtherPosition = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();
		CalculateRepulsiveForce(Force, Position, OtherPosition);
	}

	(*WriteBuffer)[Node.Index].SetLocation(Position + Force);
}

void UPCGExForceDirectedRelax::CalculateAttractiveForce(FVector& Force, const FVector& A, const FVector& B) const
{
	// Calculate the displacement vector between the nodes
	FVector Displacement = B - A;

	const double Distance = FMath::Max(Displacement.Length(), 1e-5);
	Displacement /= Distance;

	// Calculate the force magnitude using Hooke's law
	const double ForceMagnitude = SpringConstant * Distance;
	Force += Displacement * ForceMagnitude;
}

void UPCGExForceDirectedRelax::CalculateRepulsiveForce(FVector& Force, const FVector& A, const FVector& B) const
{
	// Calculate the displacement vector between the nodes
	FVector Displacement = B - A;

	const double Distance = FMath::Max(Displacement.Length(), 1e-5);
	Displacement /= Distance;

	// Calculate the force magnitude using Coulomb's law
	const double ForceMagnitude = ElectrostaticConstant / (Distance * Distance);
	Force -= Displacement * ForceMagnitude;
}

#pragma endregion
