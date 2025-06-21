// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExVerletRelax.generated.h"

UENUM()
enum class EPCGExRelaxEdgeRestLength : uint8
{
	Fixed     = 0 UMETA(DisplayName = "Fixed", ToolTip="Aim for constant edge length while fitting"),
	Existing  = 1 UMETA(DisplayName = "Existing", ToolTip="Attempts to preserve existing edge length"),
	Attribute = 2 UMETA(DisplayName = "Attribute", ToolTip="Uses an attribute on the edges as target length"),
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Verlet (Gravity)", PCGExNodeLibraryDoc="clusters/relax-cluster/Gravity"))
class UPCGExVerletRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	UPCGExVerletRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override
	{
		Super::RegisterPrimaryBuffersDependencies(InContext, FacadePreloader);
		if (GravityInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, GravityAttribute); }
		if (FrictionInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, FrictionAttribute); }
	}

	/** Type of Gravity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType GravityInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Gravity (Attr)", EditCondition="GravityInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GravityAttribute;

	/** Constant Gravity value. Think of it as gravity vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Gravity", EditCondition="GravityInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Gravity = FVector(0, 0, -100);

	PCGEX_SETTING_VALUE_GET(Gravity, FVector, GravityInput, GravityAttribute, Gravity)

	/** Type of Friction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType FrictionInput = EPCGExInputValueType::Constant;

	/** Attribute to read friction value from. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction (Attr)", EditCondition="FrictionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector FrictionAttribute;

	/** Constant friction value. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction", EditCondition="FrictionInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double Friction = 0;

	PCGEX_SETTING_VALUE_GET(Friction, double, FrictionInput, FrictionAttribute, Friction)

	/** Type of Edge stiffness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeStiffnessInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge stiffness value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness (Attr)", EditCondition="EdgeStiffnessInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeStiffnessAttribute;

	/** Constant Edge stiffness value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness", EditCondition="EdgeStiffnessInput == EPCGExInputValueType::Constant", EditConditionHides))
	double EdgeStiffness = 0.1;

	PCGEX_SETTING_VALUE_GET(EdgeStiffness, double, EdgeStiffnessInput, EdgeStiffnessAttribute, EdgeStiffness)


	/** If this was a physic simulation, represent the time advance each iteration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TimeStep = 0.01;

	/** Under the hood updates are operated on a FIntVector3. The regular FVector value is multiplied by this factor, and later divided by it. Default value of 100 means .00 precision. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Floating Point Precision"), AdvancedDisplay)
	double Precision = 100;


	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

		GravityBuffer = GetValueSettingGravity();
		if (!GravityBuffer->Init(InContext, PrimaryDataFacade)) { return false; }

		FrictionBuffer = GetValueSettingFriction();
		if (!FrictionBuffer->Init(InContext, PrimaryDataFacade)) { return false; }

		StiffnessBuffer = GetValueSettingEdgeStiffness();
		if (!StiffnessBuffer->Init(InContext, SecondaryDataFacade)) { return false; }

		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
		Deltas.Init(FIntVector3(0), Cluster->Nodes->Num());

		OldPositions.SetNumUninitialized(Cluster->Nodes->Num());
		for (int i = 0; i < OldPositions.Num(); i++) { OldPositions[i] = Cluster->GetPos(i); }

		Cluster->ComputeEdgeLengths();
		EdgeLengths = Cluster->EdgeLengths;

		return true;
	}

	virtual int32 GetNumSteps() override { return 3; }

	virtual EPCGExClusterElement PrepareNextStep(const int32 InStep) override
	{
		// Step 1 : Apply Gravity Force on each node
		if (InStep == 0)
		{
			Super::PrepareNextStep(InStep);
			Deltas.Reset(Cluster->Nodes->Num());
			Deltas.Init(FIntVector3(0), Cluster->Nodes->Num());
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

	virtual void Step1(const PCGExCluster::FNode& Node) override
	{
		const double F = (1 - FrictionBuffer->Read(Node.PointIndex)) * 0.99;

		const FVector G = GravityBuffer->Read(Node.PointIndex);
		const FVector P = (*ReadBuffer)[Node.Index].GetLocation();
		const FVector V = (P - OldPositions[Node.Index]) * F;
		OldPositions[Node.Index] = P;
		(*WriteBuffer)[Node.Index].SetLocation(P + V + (G * TimeStep));
	}

	virtual void Step2(const PCGExGraph::FEdge& Edge) override
	{
		// Compute position corrections based on edges
		const PCGExCluster::FNode* NodeA = Cluster->GetEdgeStart(Edge);
		const PCGExCluster::FNode* NodeB = Cluster->GetEdgeEnd(Edge);

		const int32 A = NodeA->Index;
		const int32 B = NodeB->Index;

		const FVector PA = (*WriteBuffer)[A].GetLocation();
		const FVector PB = (*WriteBuffer)[B].GetLocation();

		const double RestLength = *(EdgeLengths->GetData() + Edge.Index);
		const double L = FVector::Dist(PA, PB);

		const double Stiffness = StiffnessBuffer->Read(Edge.Index);

		FVector Correction = (L > RestLength ? (PA - PB) : (PB - PA)).GetSafeNormal() * FMath::Abs(L - RestLength);

		AddDelta(A, Correction * -Stiffness);
		AddDelta(B, Correction * Stiffness);
	}

	virtual void Step3(const PCGExCluster::FNode& Node) override
	{
		// Update positions based on accumulated forces
		if (FrictionBuffer->Read(Node.Index) >= 1) { return; }
		(*WriteBuffer)[Node.Index].SetLocation((*WriteBuffer)[Node.Index].GetLocation() + GetDelta(Node.Index));
	}

protected:
	TSharedPtr<TArray<double>> EdgeLengths;
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> GravityBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> StiffnessBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> FrictionBuffer;

	TArray<FVector> OldPositions;
	TArray<FIntVector3> Deltas;

	TArray<int8> Hits;
	TArray<FVector> HitLocations;

	FVector GetDelta(const int32 Index)
	{
		const FIntVector3& P = Deltas[Index];
		return FVector(P.X, P.Y, P.Z) / Precision;
	}

	void AddDelta(const int32 Index, const FVector& Delta)
	{
		FPlatformAtomics::InterlockedAdd(&Deltas[Index].X, Delta.X * Precision);
		FPlatformAtomics::InterlockedAdd(&Deltas[Index].Y, Delta.Y * Precision);
		FPlatformAtomics::InterlockedAdd(&Deltas[Index].Z, Delta.Z * Precision);
	}

	void AddDelta(const int32 AddIndex, const int32 SubtractIndex, const FVector& Delta)
	{
		AddDelta(AddIndex, Delta);
		AddDelta(SubtractIndex, -Delta);
	}
};
