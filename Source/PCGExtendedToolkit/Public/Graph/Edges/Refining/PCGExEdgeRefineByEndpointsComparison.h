// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExEdgeRefineByEndpointsComparison.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Refine Edge Comparison Type"))
enum class EPCGExRefineEdgeComparisonType : uint8
{
	Numeric = 0 UMETA(DisplayName = "Numeric", Tooltip="Numeric comparison"),
	String  = 1 UMETA(DisplayName = "String", Tooltip="String comparison"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine by Endpoints Comparison"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeEndpointsComparison : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeEndpointsComparison* TypedOther = Cast<UPCGExEdgeEndpointsComparison>(Other))
		{
			Attribute = TypedOther->Attribute;
			CompareAs = TypedOther->CompareAs;
			NumericComparison = TypedOther->NumericComparison;
			Tolerance = TypedOther->Tolerance;
			StringComparison = TypedOther->StringComparison;
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }

	virtual void PrepareVtxFacade(const TSharedPtr<PCGExData::FFacade>& InVtxFacade) const override
	{
		if (CompareAs == EPCGExRefineEdgeComparisonType::Numeric)
		{
			InVtxFacade->GetBroadcaster<double>(Attribute);
		}
		else
		{
			InVtxFacade->GetBroadcaster<FString>(Attribute);
		}
	}

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& InHeuristics) override
	{
		Super::PrepareForCluster(InCluster, InHeuristics);

		if (CompareAs == EPCGExRefineEdgeComparisonType::Numeric)
		{
			NumericBuffer = PrimaryDataFacade->GetBroadcaster<double>(Attribute);
		}
		else
		{
			StringBuffer = PrimaryDataFacade->GetBroadcaster<FString>(Attribute);
		}
	}

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const PCGExCluster::FNode* From = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start];
		const PCGExCluster::FNode* To = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End];

		if (CompareAs == EPCGExRefineEdgeComparisonType::Numeric)
		{
			if (PCGExCompare::Compare(NumericComparison, NumericBuffer->Read(Edge.Start), NumericBuffer->Read(Edge.End), Tolerance)) { return; }
		}
		else
		{
			if (PCGExCompare::Compare(StringComparison, StringBuffer->Read(Edge.Start), StringBuffer->Read(Edge.End))) { return; }
		}

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, bInvert ? 1 : 0);
	}

	/** Attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeComparisonType CompareAs = EPCGExRefineEdgeComparisonType::Numeric;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Comparison", EditCondition="CompareAs == EPCGExRefineEdgeComparisonType::Numeric", EditConditionHides))
	EPCGExComparison NumericComparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="(Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual) && CompareAs == EPCGExRefineEdgeComparisonType::Numeric", EditConditionHides))
	double Tolerance = 0.001;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Comparison", EditCondition="CompareAs == EPCGExRefineEdgeComparisonType::String", EditConditionHides))
	EPCGExStringComparison StringComparison = EPCGExStringComparison::StrictlyEqual;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	TSharedPtr<PCGExData::TBuffer<double>> NumericBuffer;
	TSharedPtr<PCGExData::TBuffer<FString>> StringBuffer;

	virtual void Cleanup() override
	{
		Super::Cleanup();
	}
};
