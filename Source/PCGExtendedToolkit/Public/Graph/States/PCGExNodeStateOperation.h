// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExNodeStateOperation.generated.h"

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNodeStateDescriptorBase
{
	GENERATED_BODY()

	FPCGExNodeStateDescriptorBase()
	{
	}

};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExNodeStateOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool PrepareForPoints(const PCGExData::FPointIO* InPointIO);
	virtual bool RequiresDirectProcessing();
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks = nullptr, const FVector& ST = FVector::ZeroVector);
	virtual void ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks = nullptr, const FVector& ST = FVector::ZeroVector);

	virtual void Cleanup() override;

	FPCGExNodeStateDescriptorBase* BaseDescriptor = nullptr;

};
