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
UCLASS(MinimalAPI, meta=(DisplayName = "Verlet (Gravity)", PCGExNodeLibraryDoc="clusters/relax-cluster/Velocity"))
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
		if (VelocityInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, VelocityAttribute); }
		if (MassInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, MassAttribute); }
	}

	/** Type of Velocity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType VelocityInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Velocity (Attr)", EditCondition="VelocityInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector VelocityAttribute;

	/** Constant Velocity value. Think of it as gravity vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Velocity", EditCondition="VelocityInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Velocity = FVector::DownVector;

	PCGEX_SETTING_VALUE_GET(Velocity, FVector, VelocityInput, VelocityAttribute, Velocity)

	/** Type of Mass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType MassInput = EPCGExInputValueType::Constant;

	/** Attribute to read mass (weight) value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Mass (Attr)", EditCondition="MassInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector MassAttribute;

	/** Constant mass (weight) value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Mass", EditCondition="MassInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Mass = 100;

	PCGEX_SETTING_VALUE_GET(Mass, double, MassInput, MassAttribute, Mass)

	/** Which edge length should the computation attempt to preserve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRelaxEdgeRestLength EdgeRestLength = EPCGExRelaxEdgeRestLength::Existing;

	/** The desired edge length. Low priority in the algorithm, but help keep edge topology more consistent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Desired Edge Length", EditCondition="EdgeRestLength == EPCGExRelaxEdgeRestLength::Fixed", EditConditionHides))
	double DesiredEdgeLength = 100;

	/** Per-edge attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Desired Edge Length", EditCondition="EdgeRestLength == EPCGExRelaxEdgeRestLength::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DesiredEdgeLengthAttribute;

	/** Scale factor applied to the edge length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Scale", EditCondition="EdgeRestLength == EPCGExRelaxEdgeRestLength::Attribute || EdgeRestLength == EPCGExRelaxEdgeRestLength::Existing", EditConditionHides))
	double Scale = 1;


	/** Type of Edge stiffness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeStiffnessInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge stiffness value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness (Attr)", EditCondition="EdgeStiffnessInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeStiffnessAttribute;

	/** Constant Edge stiffness value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness", EditCondition="EdgeStiffnessInput == EPCGExInputValueType::Constant", EditConditionHides))
	double EdgeStiffness = 0.5;

	PCGEX_SETTING_VALUE_GET(EdgeStiffness, double, EdgeStiffnessInput, EdgeStiffnessAttribute, EdgeStiffness)


	/** If this was a physic simulation, represent the time advance each iteration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TimeStep = 0.1;

	/** Under the hood updates are operated on a FIntVector3. The regular FVector value is multiplied by this factor, and later divided by it. Default value of 100 means .00 precision. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Floating Point Precision"), AdvancedDisplay)
	double Precision = 100;


	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

		VelocityBuffer = GetValueSettingVelocity();
		if (!VelocityBuffer->Init(InContext, PrimaryDataFacade)) { return false; }

		MassBuffer = GetValueSettingMass();
		if (!MassBuffer->Init(InContext, PrimaryDataFacade)) { return false; }

		StiffnessBuffer = GetValueSettingEdgeStiffness();
		if (!StiffnessBuffer->Init(InContext, SecondaryDataFacade)) { return false; }

		Forces.Reset(Cluster->Nodes->Num());
		Forces.Init(FIntVector3(0), Cluster->Nodes->Num());

		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
		Forces.Init(FIntVector3(0), Cluster->Nodes->Num());

		for (int i = 0; i < Cluster->Nodes->Num(); i++) { const double M = MassBuffer->Read(Cluster->GetNodePointIndex(i)); }

		if (EdgeRestLength == EPCGExRelaxEdgeRestLength::Attribute)
		{
			const TSharedPtr<PCGExData::TBuffer<double>> Buffer = SecondaryDataFacade->GetBroadcaster<double>(DesiredEdgeLengthAttribute);

			if (!Buffer)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, "Edge Length", DesiredEdgeLengthAttribute)
				return false;
			}

			EdgeLengths = MakeShared<TArray<double>>();
			EdgeLengths->SetNumUninitialized(Cluster->Edges->Num());
			Buffer->DumpValues(EdgeLengths);
		}
		else
		{
			if (EdgeRestLength == EPCGExRelaxEdgeRestLength::Fixed)
			{
				EdgeLengths = MakeShared<TArray<double>>();
				EdgeLengths->Init(DesiredEdgeLength, Cluster->Edges->Num());
				Scale = 1;
			}
			else if (EdgeRestLength == EPCGExRelaxEdgeRestLength::Existing)
			{
				Cluster->ComputeEdgeLengths();
				EdgeLengths = Cluster->EdgeLengths;
			}
		}

		Forces.Init(FIntVector3(0), Cluster->Nodes->Num());


		return true;
	}

	virtual int32 GetNumSteps() override { return 3; }

	virtual EPCGExClusterElement PrepareNextStep(const int32 InStep) override
	{
		// Step 1 : Apply Velocity Force on each node
		if (InStep == 0)
		{
			Super::PrepareNextStep(InStep);
			Forces.Reset(Cluster->Nodes->Num());
			Forces.Init(FIntVector3(0), Cluster->Nodes->Num());
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
		const double M = MassBuffer->Read(Node.PointIndex) *  InfluenceDetails->GetInfluence(Node.PointIndex);

		if (M <= 0) { return; }

		const FVector Accel = VelocityBuffer->Read(Node.PointIndex) * M;

		// Apply raw gravity force
		(*WriteBuffer)[Node.Index].SetLocation((*ReadBuffer)[Node.Index].GetLocation() + Accel * TimeStep);
	}

	virtual void Step2(const PCGExGraph::FEdge& Edge) override
	{
		// Compute position corrections based on edges
		const PCGExCluster::FNode* Start = Cluster->GetEdgeStart(Edge);
		const PCGExCluster::FNode* End = Cluster->GetEdgeEnd(Edge);

		const FVector StartPos = (*WriteBuffer)[Start->Index].GetLocation();
		const FVector EndPos = (*WriteBuffer)[End->Index].GetLocation();

		const FVector Delta = EndPos - StartPos;
		const double CurrentLength = Delta.Size();

		if (CurrentLength < KINDA_SMALL_NUMBER) { return; }

		const double RestLength = *(EdgeLengths->GetData() + Edge.Index) * Scale;
		const double Diff = (CurrentLength - RestLength) / CurrentLength;
		const FVector Correction = Delta * StiffnessBuffer->Read(Edge.Index) * Diff;
		
		const double MassA = MassBuffer->Read(Start->PointIndex);
		const double MassB = MassBuffer->Read(End->PointIndex);

		if (const double TotalMass = MassA + MassB; TotalMass > 0.f)
		{
			const FVector RatioA = Correction * (MassA / TotalMass);
			const FVector RatioB = Correction * (MassB / TotalMass);

			AddDelta(Start->Index, RatioA);
			AddDelta(End->Index, -RatioB);
		}
	}

	virtual void Step3(const PCGExCluster::FNode& Node) override
	{
		// Update positions based on accumulated forces
		(*WriteBuffer)[Node.Index].SetLocation((*WriteBuffer)[Node.Index].GetLocation() + GetForce(Node.Index));
	}

protected:
	TSharedPtr<TArray<double>> EdgeLengths;
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> VelocityBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> MassBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> StiffnessBuffer;

	TArray<FIntVector3> Forces;

	TArray<int8> Hits;
	TArray<FVector> HitLocations;

	FVector GetForce(const int32 Index)
	{
		const FIntVector3& P = Forces[Index];
		return FVector(P.X, P.Y, P.Z) / Precision;
	}

	void AddDelta(const int32 Index, const FVector& Delta)
	{
		FPlatformAtomics::InterlockedAdd(&Forces[Index].X, Delta.X * Precision);
		FPlatformAtomics::InterlockedAdd(&Forces[Index].Y, Delta.Y * Precision);
		FPlatformAtomics::InterlockedAdd(&Forces[Index].Z, Delta.Z * Precision);
	}

	void ApplyDeleta(const int32 AddIndex, const int32 SubtractIndex, const FVector& Delta)
	{
		AddDelta(AddIndex, Delta);
		AddDelta(SubtractIndex, -Delta);
	}
};
