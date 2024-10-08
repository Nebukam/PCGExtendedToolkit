// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExEdgeRefineByAdjacency.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Refine Edge Threshold Mode"))
enum class EPCGExRefineEdgeThresholdMode : uint8
{
	Sum  = 0 UMETA(DisplayName = "Sum", Tooltip="The sum of adjacencies must be below the specified threshold"),
	Any  = 1 UMETA(DisplayName = "Any Endpoint", Tooltip="At least one endpoint adjacency count must be below the specified threshold"),
	Both = 2 UMETA(DisplayName = "Both Endpoints", Tooltip="Both endpoint adjacency count must be below the specified threshold"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine by Adjacency"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineByAdjacency : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineByAdjacency* TypedOther = Cast<UPCGExEdgeRefineByAdjacency>(Other))
		{
			ThresholdSource = TypedOther->ThresholdSource;
			ThresholdConstant = TypedOther->ThresholdConstant;
			ThresholdAttribute = TypedOther->ThresholdAttribute;
			Mode = TypedOther->Mode;
			Comparison = TypedOther->Comparison;
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& InHeuristics) override
	{
		Super::PrepareForCluster(InCluster, InHeuristics);
		if (ThresholdSource == EPCGExFetchType::Attribute)
		{
			ThresholdBuffer = SecondaryDataFacade->GetScopedBroadcaster<int32>(ThresholdAttribute);
			if(!ThresholdBuffer)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Threshold Attribute ({0}) is not valid."), FText::FromString(ThresholdAttribute.GetName().ToString())));
			}
		}
	}

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const PCGExCluster::FNode* From = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start];
		const PCGExCluster::FNode* To = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End];

		const int32 Threshold = ThresholdBuffer ? ThresholdBuffer->Read(Edge.PointIndex) : ThresholdConstant;
		
		if (Mode == EPCGExRefineEdgeThresholdMode::Both)
		{
			if (PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) && PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance)) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Any)
		{
			if (PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) || PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance)) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Sum)
		{
			if (PCGExCompare::Compare(Comparison, (From->Adjacency.Num() + To->Adjacency.Num()), Threshold, Tolerance)) { return; }
		}

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, bInvert ? 1 : 0);
	}

	/** Whether to read the threshold from an attribute on the edge or a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType ThresholdSource = EPCGExFetchType::Constant;

	/** The number of connection endpoints must have to be considered a Bridge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="1", DisplayName="Threshold", EditCondition="ThresholdSource == EPCGExFetchType::Constant", EditConditionHides))
	int32 ThresholdConstant = 2;

	/** Attribute to fetch threshold from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold", EditCondition="ThresholdSource == EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** How should we check if the threshold is reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeThresholdMode Mode = EPCGExRefineEdgeThresholdMode::Sum;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	int32 Tolerance = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	virtual void Cleanup() override
	{
		ThresholdBuffer.Reset();
		Super::Cleanup();
	}

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> ThresholdBuffer;

	virtual void ApplyOverrides() override
	{
		Super::ApplyOverrides();

		PCGEX_OVERRIDE_OPERATION_PROPERTY_SELECTOR(ThresholdAttribute, "Refine/ThresholdAttribute")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(ThresholdConstant, "Refine/ThresholdConstant")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(Tolerance, "Refine/Tolerance")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(bInvert, "Refine/Invert")
	}
};
