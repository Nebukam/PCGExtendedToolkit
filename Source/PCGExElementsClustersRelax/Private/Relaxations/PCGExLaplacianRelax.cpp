// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relaxations/PCGExLaplacianRelax.h"

#pragma region UPCGExLaplacianRelax

void UPCGExLaplacianRelax::Step1(const PCGExClusters::FNode& Node)
{
	const FVector Position = (ReadBuffer->GetData() + Node.Index)->GetLocation();
	FVector Force = FVector::ZeroVector;

	for (const PCGExGraphs::FLink& Lk : Node.Links) { Force += (ReadBuffer->GetData() + Lk.Node)->GetLocation() - Position; }

	(*WriteBuffer)[Node.Index].SetLocation(Position + Force / static_cast<double>(Node.Links.Num()));
}

#pragma endregion
