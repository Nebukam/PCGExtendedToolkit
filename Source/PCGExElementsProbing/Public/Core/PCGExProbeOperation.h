// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOctree.h"
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

	/** Whether to read search radius from an attribute or use a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportRadius", EditConditionHides, HideEditConditionToggle))
	EPCGExInputValueType SearchRadiusInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Search Radius (Attr)", EditCondition="bSupportRadius && SearchRadiusInput != EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector SearchRadiusAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Search Radius", ClampMin=0, EditCondition="bSupportRadius && SearchRadiusInput == EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	double SearchRadiusConstant = 100;

	/** A convenient static offset added to the attribute value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Offset", EditCondition="bSupportRadius && SearchRadiusInput != EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	double SearchRadiusOffset = 0;

	PCGEX_SETTING_VALUE_DECL(SearchRadius, double)
};

/**
 * 
 */
class PCGEXELEMENTSPROBING_API FPCGExProbeOperation : public FPCGExOperation
{
public:
	virtual bool Prepare(FPCGExContext* InContext);
	virtual bool IsDirectProbe() const;
	virtual bool RequiresChainProcessing() const;
	virtual void ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container);

	virtual bool IsGlobalProbe() const;
	virtual bool WantsOctree() const;

	virtual void PrepareBestCandidate(const int32 Index, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container);
	virtual void ProcessCandidateChained(const int32 Index, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container);
	virtual void ProcessBestCandidate(const int32 Index, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container);

	virtual void ProcessNode(const int32 Index, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container);

	virtual void ProcessAll(TSet<uint64>& OutEdges) const;

	FPCGExProbeConfigBase* BaseConfig = nullptr;
	const PCGExOctree::FItemOctree* Octree = nullptr;
	const TArray<FTransform>* WorkingTransforms = nullptr;
	const TArray<FVector>* WorkingPositions = nullptr;
	const TArray<int8>* CanGenerate = nullptr;
	const TArray<int8>* AcceptConnections = nullptr;

	double SearchRadiusOffset = 0;
	double GetSearchRadius(const int32 Index) const;
	TSharedPtr<PCGExDetails::TSettingValue<double>> SearchRadius;

protected:
	TSharedPtr<PCGExData::FPointIO> PointIO;
	TArray<double> LocalWeightMultiplier;
};
