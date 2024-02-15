// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"

class PCGEXTENDEDTOOLKIT_API FPCGExPointIOMerger
{
	friend class FPCGExAttributeMergeTask;

public:
	FPCGExPointIOMerger(PCGExData::FPointIO& OutData);
	virtual ~FPCGExPointIOMerger();

	void Append(PCGExData::FPointIO& InData);
	void Append(const TArray<PCGExData::FPointIO*>& InData);
	void Merge(FPCGExAsyncManager* AsyncManager, bool CleanupInputs = true);
	void Write();

	int32 TotalPoints = 0;

protected:
	TMap<FName, PCGEx::FAttributeIdentity> Identities;
	TMap<FName, PCGEx::FAAttributeIO*> Writers;
	TArray<PCGEx::FAAttributeIO*> WriterList;
	TMap<FName, bool> AllowsInterpolation;
	PCGExData::FPointIO* MergedData = nullptr;
	TArray<PCGExData::FPointIO*> MergedPoints;
	bool bCleanupInputs = true;
};

class PCGEXTENDEDTOOLKIT_API FPCGExAttributeMergeTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExAttributeMergeTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                         FPCGExPointIOMerger* InMerger,
	                         const FName InAttributeName)
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  Merger(InMerger),
		  AttributeName(InAttributeName)
	{
	}

	FPCGExPointIOMerger* Merger = nullptr;
	FName AttributeName;

	virtual bool ExecuteTask() override;
};
