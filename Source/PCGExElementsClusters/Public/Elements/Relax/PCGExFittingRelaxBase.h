// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGExRelaxClusterOperation.h"
#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"


#include "PCGExFittingRelaxBase.generated.h"

UENUM()
enum class EPCGExRelaxEdgeFitting : uint8
{
	Ignore    = 0 UMETA(DisplayName = "Ignore", ToolTip="Ignore edges during fitting."),
	Fixed     = 1 UMETA(DisplayName = "Fixed", ToolTip="Aim for constant edge length while fitting"),
	Existing  = 2 UMETA(DisplayName = "Existing", ToolTip="Attempts to preserve existing edge length"),
	Attribute = 3 UMETA(DisplayName = "Attribute", ToolTip="Uses an attribute on the edges as target length"),
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, DisplayName = "Abstract Fitting")
class UPCGExFittingRelaxBase : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	UPCGExFittingRelaxBase(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}


	/** Amount of translation for a single step. Relative to other parameters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double RepulsionConstant = 100;

	/** Which edge length should the computation attempt to preserve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRelaxEdgeFitting EdgeFitting = EPCGExRelaxEdgeFitting::Existing;

	/** The desired edge length. Low priority in the algorithm, but help keep edge topology more consistent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Desired Edge Length", EditCondition="EdgeFitting == EPCGExRelaxEdgeFitting::Fixed", EditConditionHides))
	double DesiredEdgeLength = 100;

	/** Per-edge attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Desired Edge Length", EditCondition="EdgeFitting == EPCGExRelaxEdgeFitting::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DesiredEdgeLengthAttribute;

	/** Scale factor applied to the edge length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Scale", EditCondition="EdgeFitting == EPCGExRelaxEdgeFitting::Attribute || EdgeFitting == EPCGExRelaxEdgeFitting::Existing", EditConditionHides))
	double Scale = 2;

	/** Stiffness of the edges. Lower values yield better placement (less overlap), but edge topology may be affected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="EdgeFitting != EPCGExRelaxEdgeFitting::Ignore", EditConditionHides))
	double SpringConstant = 0.1;

	/** If this was a physic simulation, represent the time advance each iteration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TimeStep = 0.01;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
		Deltas.Init(FInt64Vector3(0), Cluster->Nodes->Num());

		if (EdgeFitting == EPCGExRelaxEdgeFitting::Attribute)
		{
			const TSharedPtr<PCGExData::TBuffer<double>> Buffer = SecondaryDataFacade->GetBroadcaster<double>(DesiredEdgeLengthAttribute);

			if (!Buffer)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Edge Length, DesiredEdgeLengthAttribute)
				return false;
			}

			EdgeLengths = MakeShared<TArray<double>>();
			EdgeLengths->SetNumUninitialized(Cluster->Edges->Num());
			Buffer->DumpValues(EdgeLengths);
		}
		else
		{
			if (EdgeFitting == EPCGExRelaxEdgeFitting::Fixed)
			{
				EdgeLengths = MakeShared<TArray<double>>();
				EdgeLengths->Init(DesiredEdgeLength, Cluster->Edges->Num());
				Scale = 1;
			}
			else if (EdgeFitting == EPCGExRelaxEdgeFitting::Existing)
			{
				Cluster->ComputeEdgeLengths();
				EdgeLengths = Cluster->EdgeLengths;
			}
		}

		return true;
	}

	virtual int32 GetNumSteps() override { return 3; }

	virtual EPCGExClusterElement PrepareNextStep(const int32 InStep) override
	{
		// Step 1 : Apply spring forces for each edge
		if (InStep == 0)
		{
			Super::PrepareNextStep(InStep);
			Deltas.Reset(Cluster->Nodes->Num());
			Deltas.Init(FInt64Vector3(0), Cluster->Nodes->Num());
			return EPCGExClusterElement::Edge;
		}

		// Step 2 : Apply repulsion forces between all pairs of nodes
		// Step 3 : Update positions based on accumulated forces
		return EPCGExClusterElement::Vtx;
	}

	virtual void Step1(const PCGExGraphs::FEdge& Edge) override
	{
		// Apply spring forces for each edge

		// TODO : Refactor relax host to skip edge processing entirely
		if (EdgeFitting == EPCGExRelaxEdgeFitting::Ignore) { return; }

		const int32 Start = Cluster->GetEdgeStart(Edge)->Index;
		const int32 End = Cluster->GetEdgeEnd(Edge)->Index;

		const FVector& StartPos = (ReadBuffer->GetData() + Start)->GetLocation();
		const FVector& EndPos = (ReadBuffer->GetData() + End)->GetLocation();

		const FVector Delta = EndPos - StartPos;
		const double CurrentLength = Delta.Size();

		if (CurrentLength <= KINDA_SMALL_NUMBER) { return; }

		const FVector Direction = Delta / CurrentLength;
		const double Displacement = CurrentLength - (*(EdgeLengths->GetData() + Edge.Index) * Scale);

		AddDelta(Start, End, (SpringConstant * Displacement * Direction));
	}

	virtual void Step3(const PCGExClusters::FNode& Node) override
	{
		// Update positions based on accumulated forces
		const FVector Position = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		(*WriteBuffer)[Node.Index].SetLocation(Position + GetDelta(Node.Index) * TimeStep);
	}

protected:
	TSharedPtr<TArray<double>> EdgeLengths;

	FVector GetDelta(const int32 Index) const
	{
		const FInt64Vector3& P = Deltas[Index];
		return FVector(P.X, P.Y, P.Z) / Precision;
	}
};
