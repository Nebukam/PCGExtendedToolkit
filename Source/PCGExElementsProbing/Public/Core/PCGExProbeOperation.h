// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataHelpers.h"
#include "Factories/PCGExOperation.h"
#include "Details/PCGExSettingsMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "UObject/Object.h"
#include "PCGExProbeOperation.generated.h"

namespace PCGExMT
{
	class FScopedContainer;
}

namespace PCGExData
{
	class FPointIO;
}

namespace PCGExProbing
{
	struct FBestCandidate;
}

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct PCGEXELEMENTSPROBING_API FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigBase()
	{
	}

	explicit FPCGExProbeConfigBase(const bool SupportsRadius)
		: bSupportRadius(SupportsRadius)
	{
	}

	UPROPERTY(meta=(PCG_NotOverridable))
	bool bSupportRadius = true; // Internal toggle, hidden

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportRadius", EditConditionHides))
	EPCGExInputValueType SearchRadiusInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Search Radius (Attr)", EditCondition="bSupportRadius && SearchRadiusInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SearchRadiusAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Search Radius", ClampMin=0, EditCondition="bSupportRadius && SearchRadiusInput == EPCGExInputValueType::Constant", EditConditionHides))
	double SearchRadiusConstant = 100;

	/** A convenient static offset added to the attribute value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Offset", EditCondition="bSupportRadius && SearchRadiusInput != EPCGExInputValueType::Constant", EditConditionHides))
	double SearchRadiusOffset = 0;

	PCGEX_SETTING_VALUE_DECL(SearchRadius, double)
};

/**
 * 
 */
class PCGEXELEMENTSPROBING_API FPCGExProbeOperation : public FPCGExOperation
{
public:
	virtual bool PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO);
	virtual bool RequiresOctree();
	virtual bool RequiresChainProcessing();
	virtual void ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container);

	virtual void PrepareBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container);
	virtual void ProcessCandidateChained(const int32 Index, const FTransform& WorkingTransform, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container);
	virtual void ProcessBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container);

	virtual void ProcessNode(const int32 Index, const FTransform& WorkingTransform, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, const TArray<int8>& AcceptConnections, PCGExMT::FScopedContainer* Container);

	FPCGExProbeConfigBase* BaseConfig = nullptr;

	double SearchRadiusOffset = 0;
	double GetSearchRadius(const int32 Index) const;
	TSharedPtr<PCGExDetails::TSettingValue<double>> SearchRadius;

protected:
	TSharedPtr<PCGExData::FPointIO> PointIO;
	TArray<double> LocalWeightMultiplier;
};
