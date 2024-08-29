// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPartitionByValues.generated.h"

namespace PCGExPartition
{
	PCGEX_ASYNC_STATE(State_DistributeToPartition)

	class FKPartition;

	class /*PCGEXTENDEDTOOLKIT_API*/ FKPartition
	{
	protected:
		mutable FRWLock LayersLock;
		mutable FRWLock PointLock;

	public:
		FKPartition(FKPartition* InParent, int64 InKey, FPCGExFilter::FRule* InRule, int32 InPartitionIndex);
		~FKPartition();

		FKPartition* Parent = nullptr;
		int32 PartitionIndex = 0;
		int64 PartitionKey = 0;
		FPCGExFilter::FRule* Rule = nullptr;

		TSet<int64> UniquePartitionKeys;
		TMap<int64, FKPartition*> SubLayers;
		TArray<int32> Points;

		int32 GetNum() const { return Points.Num(); }
		int32 GetSubPartitionsNum();

		FKPartition* GetPartition(int64 Key, FPCGExFilter::FRule* InRule);
		FORCEINLINE void Add(const int64 Index)
		{
			FWriteScopeLock WriteLock(PointLock);
			Points.Add(Index);
		}

		void Register(TArray<FKPartition*>& Partitions);

		void SortPartitions();
	};
}


/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPartitionByValuesBaseSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionByValuesStatic, "Partition by Values (Static)", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a Merge before.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool GetMainAcceptMultipleData() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** If false, will only write partition identifier values instead of splitting partitions into new point datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-2))
	bool bSplitOutput = true;

	/** Write the sum of partition values to an attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteKeySum = false;

	/** The Attribute name to write key sum to. Note that this value is not guaranteed to be unique. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteKeySum"))
	FName KeySumAttributeName = "KeySum";

	virtual bool GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const;
};

/**
 * 
 */
UCLASS(BlueprintType, MinimalAPI, Hidden, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPartitionByValuesSettings : public UPCGExPartitionByValuesBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionByValuesStatic, "Partition by Values (Static)", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a Merge before.");
#endif

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExPartitonRuleConfig> PartitionRules;

	virtual bool GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const override;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPartitionByValuesBaseContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesBaseElement;

	virtual ~FPCGExPartitionByValuesBaseContext() override;

	TArray<FPCGExPartitonRuleConfig> RulesConfigs;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPartitionByValuesBaseElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPartitionByValues
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		TArray<FPCGExFilter::FRule> Rules;
		TArray<int64> KeySums;

		PCGExPartition::FKPartition* RootPartition = nullptr;

		int32 NumPartitions = -1;
		TArray<PCGExPartition::FKPartition*> Partitions;

		FPCGExPartitionByValuesBaseContext* LocalTypedContext = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
