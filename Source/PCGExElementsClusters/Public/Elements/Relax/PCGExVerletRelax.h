// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Elements/Meta/NeighborSamplers/PCGExNeighborSampleAttribute.h"
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

	PCGEX_SETTING_VALUE_INLINE(Gravity, FVector, GravityInput, GravityAttribute, Gravity)

	/** Type of Friction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType FrictionInput = EPCGExInputValueType::Constant;

	/** Attribute to read friction value from. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction (Attr)", EditCondition="FrictionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector FrictionAttribute;

	/** Constant friction value. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction", EditCondition="FrictionInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double Friction = 0;

	PCGEX_SETTING_VALUE_INLINE(Friction, double, FrictionInput, FrictionAttribute, Friction)


	/** Type of Edge Scaling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeScalingInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge scaling value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Scaling (Attr)", EditCondition="EdgeScalingInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeScalingAttribute;

	/** Constant Edge scaling value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Scaling", EditCondition="EdgeScalingInput == EPCGExInputValueType::Constant", EditConditionHides))
	double EdgeScaling = 1;

	PCGEX_SETTING_VALUE_INLINE(EdgeScaling, double, EdgeScalingInput, EdgeScalingAttribute, EdgeScaling)


	/** Type of Edge stiffness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeStiffnessInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge stiffness value from. Note that this value is expected to be in the [0..1] range and will be divided by 3 internally. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness (Attr)", EditCondition="EdgeStiffnessInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeStiffnessAttribute;

	/** Constant Edge stiffness value. Note that this value is expected to be in the [0..1] range and will be divided by 3 internally.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness", EditCondition="EdgeStiffnessInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double EdgeStiffness = 0.5;

	PCGEX_SETTING_VALUE_INLINE(EdgeStiffness, double, EdgeStiffnessInput, EdgeStiffnessAttribute, EdgeStiffness)

	/** If this was a physic simulation, represent the time advance each iteration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TimeStep = 0.1;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override
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

		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
		Deltas.Init(FInt64Vector3(0), Cluster->Nodes->Num());

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

	virtual void Step1(const PCGExClusters::FNode& Node) override
	{
		const double F = (1 - FrictionBuffer->Read(Node.PointIndex)) * 0.99;

		const FVector G = GravityBuffer->Read(Node.PointIndex);
		const FVector P = (*ReadBuffer)[Node.Index].GetLocation();
		AddDelta(Node.Index, G * (TimeStep * TimeStep)); // Add delta of force

		// Write buffer is the old position at this point
		const FVector V = (P - (*WriteBuffer)[Node.Index].GetLocation()) * F;

		// Compute predicted position, NOT accounting for deltas, only verlet velocity
		(*WriteBuffer)[Node.Index].SetLocation(P + V);
	}

	virtual void Step2(const PCGExGraphs::FEdge& Edge) override
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

		const double Stiffness = (StiffnessBuffer->Read(Edge.Index)) * 0.32;

		FVector Correction = (L > RestLength ? (PA - PB) : (PB - PA)).GetSafeNormal() * FMath::Abs(L - RestLength);

		AddDelta(A, Correction * -Stiffness);
		AddDelta(B, Correction * Stiffness);
	}

	virtual void Step3(const PCGExClusters::FNode& Node) override
	{
		// Update positions based on accumulated forces
		if (FrictionBuffer->Read(Node.Index) >= 1) { return; }
		(*WriteBuffer)[Node.Index].SetLocation((*WriteBuffer)[Node.Index].GetLocation() + GetDelta(Node.Index));
	}

protected:
	TSharedPtr<TArray<double>> EdgeLengths;
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> GravityBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> StiffnessBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> ScalingBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> FrictionBuffer;

	TArray<int8> Hits;
	TArray<FVector> HitLocations;
};
