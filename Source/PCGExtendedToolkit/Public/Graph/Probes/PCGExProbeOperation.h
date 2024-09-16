// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExProbeOperation.generated.h"

namespace PCGExProbing
{
	struct FBestCandidate;
}

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigBase()
	{
	}

	explicit FPCGExProbeConfigBase(const bool SupportsRadius)
		: bSupportRadius(SupportsRadius)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="false", EditConditionHides, HideInDetailPanel))
	bool bSupportRadius = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportRadius", EditConditionHides))
	EPCGExFetchType SearchRadiusSource = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, EditCondition="bSupportRadius && SearchRadiusSource==EPCGExFetchType::Constant", EditConditionHides))
	double SearchRadiusConstant = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportRadius && SearchRadiusSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SearchRadiusAttribute;
};

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool PrepareForPoints(const PCGExData::FPointIO* InPointIO);
	virtual bool RequiresDirectProcessing();
	virtual bool RequiresChainProcessing();
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges);

	virtual void PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate);
	virtual void ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate);
	virtual void ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges);

	virtual void ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges);

	virtual void Cleanup() override;

	double SearchRadius = -1;
	double SearchRadiusSquared = -1;
	PCGExData::TCache<double>* SearchRadiusCache = nullptr;
	FPCGExProbeConfigBase* BaseConfig = nullptr;

protected:
	const PCGExData::FPointIO* PointIO = nullptr;
	TArray<double> LocalWeightMultiplier;
};
