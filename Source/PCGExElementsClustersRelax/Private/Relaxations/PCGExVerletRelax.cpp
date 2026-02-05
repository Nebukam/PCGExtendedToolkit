// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relaxations/PCGExVerletRelax.h"

#pragma region UPCGExVerletRelax

void UPCGExVerletRelax::RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterPrimaryBuffersDependencies(InContext, FacadePreloader);
	if (GravityInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, GravityAttribute); }
	if (FrictionInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, FrictionAttribute); }
}

bool UPCGExVerletRelax::PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster)
{
	if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

	GravityBuffer = GetValueSettingGravity();
	if (!GravityBuffer->Init(PrimaryDataFacade)) { return false; }

	FrictionBuffer = GetValueSettingFriction();
	if (!FrictionBuffer->Init(PrimaryDataFacade)) { return false; }

	ScalingBuffer = GetValueSettingEdgeScaling();
	if (!ScalingBuffer->Init(SecondaryDataFacade)) { return false; }

	StiffnessBuffer = GetValueSettingEdgeStiffness();
	if (!StiffnessBuffer->Init(SecondaryDataFacade)) { return false; }

	Deltas.Init(FInt64Vector3(0), Cluster->Nodes->Num());

	Cluster->ComputeEdgeLengths();
	EdgeLengths = Cluster->EdgeLengths;

	return true;
}

EPCGExClusterElement UPCGExVerletRelax::PrepareNextStep(const int32 InStep)
{
	// Step 1 : Apply Gravity Force on each node
	if (InStep == 0)
	{
		Super::PrepareNextStep(InStep);
		Deltas.Reset(Cluster->Nodes->Num());
		Deltas.Init(FInt64Vector3(0), Cluster->Nodes->Num());
		return EPCGExClusterElement::Vtx;
	}

	if (InStep == 1)
	{
		// Step 2 : Apply Edge Spring Forces
		return EPCGExClusterElement::Edge;
	}

	// Step 3 : Update positions based on accumulated forces
	return EPCGExClusterElement::Vtx;
}

void UPCGExVerletRelax::Step1(const PCGExClusters::FNode& Node)
{
	const double F = (1 - FrictionBuffer->Read(Node.PointIndex)) * DampingScale;

	const FVector G = GravityBuffer->Read(Node.PointIndex);
	const FVector P = (*ReadBuffer)[Node.Index].GetLocation();

	// Write buffer is the old position at this point
	const FVector V = (P - (*WriteBuffer)[Node.Index].GetLocation()) * F;

	// Compute predicted position INCLUDING gravity, so springs can properly counteract it
	(*WriteBuffer)[Node.Index].SetLocation(P + V + G * (TimeStep * TimeStep));
}

void UPCGExVerletRelax::Step2(const PCGExGraphs::FEdge& Edge)
{
	// Compute position corrections based on edges
	const PCGExClusters::FNode* NodeA = Cluster->GetEdgeStart(Edge);
	const PCGExClusters::FNode* NodeB = Cluster->GetEdgeEnd(Edge);

	const int32 A = NodeA->Index;
	const int32 B = NodeB->Index;

	const FVector PA = (*WriteBuffer)[A].GetLocation();
	const FVector PB = (*WriteBuffer)[B].GetLocation();

	const double RestLength = *(EdgeLengths->GetData() + Edge.Index) * ScalingBuffer->Read(Edge.PointIndex);
	const double L = FVector::Dist(PA, PB);

	const double Stiffness = (StiffnessBuffer->Read(Edge.PointIndex)) * 0.32;

	FVector Correction = (L > RestLength ? (PA - PB) : (PB - PA)).GetSafeNormal() * FMath::Abs(L - RestLength);

	AddDelta(A, Correction * -Stiffness);
	AddDelta(B, Correction * Stiffness);
}

void UPCGExVerletRelax::Step3(const PCGExClusters::FNode& Node)
{
	// Update positions based on accumulated forces
	if (FrictionBuffer->Read(Node.PointIndex) >= 1) { return; }
	(*WriteBuffer)[Node.Index].SetLocation((*WriteBuffer)[Node.Index].GetLocation() + GetDelta(Node.Index));
}

#pragma endregion
