// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExUnionOpsManager.h"

#include "PCGExDetailsData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExUnionData.h"

namespace PCGExDataBlending
{
	FUnionOpsManager::FUnionOpsManager(const TArray<TObjectPtr<const UPCGExBlendOpFactory>>* InBlendingFactories, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails)
		: BlendingFactories(InBlendingFactories), DistanceDetails(InDistanceDetails)
	{
	}

	FUnionOpsManager::~FUnionOpsManager()
	{
	}

	bool FUnionOpsManager::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources)
	{
		// Create an OpBlenderManager per Source/target pair

		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { MaxIndex = FMath::Max(Src->Source->IOIndex, MaxIndex); }
		IOLookup = MakeShared<PCGEx::FIndexLookup>(MaxIndex + 1);

		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { IOLookup->Set(Src->Source->IOIndex, SourcesData.Add(Src->GetIn())); }

		return true;
	}

	bool FUnionOpsManager::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata)
	{
		CurrentUnionMetadata = InUnionMetadata;
		return Init(InContext, TargetData, InSources);
	}

	void FUnionOpsManager::InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const
	{
		check(!Blenders.IsEmpty())
		Blenders[0]->InitTrackers(Trackers);
	}

	void FUnionOpsManager::MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const
	{
		check(InUnionData)
		check(!Blenders.IsEmpty())

		PCGExData::FConstPoint Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);
		const int32 UnionCount = InUnionData->ComputeWeights(SourcesData, IOLookup, Target, DistanceDetails, OutWeightedPoints);

		if (UnionCount == 0) { return; }

		Blenders[0]->BeginMultiBlend(WriteIndex, Trackers);

		// For each point in the union, check if there is an attribute blender for that source; and if so, add it to the blend
		for (const PCGExData::FWeightedPoint& P : OutWeightedPoints)
		{
			Blenders[P.IO]->MultiBlend(P.Index, WriteIndex, P.Weight, Trackers);
		}

		Blenders[0]->EndMultiBlend(WriteIndex, Trackers);
	}

	void FUnionOpsManager::MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const
	{
		MergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), OutWeightedPoints, Trackers);
	}
}
