// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExPartitionByValues.generated.h"

namespace PCGExPartition
{
	class FKPartition;

	class /*PCGEXTENDEDTOOLKIT_API*/ FKPartition : public TSharedFromThis<FKPartition>
	{
	protected:
		mutable FRWLock LayersLock;
		mutable FRWLock PointLock;

	public:
		FKPartition(const TWeakPtr<FKPartition>& InParent, int64 InKey, FPCGExFilter::FRule* InRule, int32 InPartitionIndex);
		~FKPartition();

		TWeakPtr<FKPartition> Parent;
		int32 IOIndex = -1;
		int32 PartitionIndex = 0;
		int64 PartitionKey = 0;
		FPCGExFilter::FRule* Rule = nullptr;

		TSet<int64> UniquePartitionKeys;
		TMap<int64, TSharedPtr<FKPartition>> SubLayers;
		TArray<int32> Points;

		int32 GetNum() const { return Points.Num(); }
		int32 GetSubPartitionsNum();

		TSharedPtr<FKPartition> GetPartition(int64 Key, FPCGExFilter::FRule* InRule);
		FORCEINLINE void Add(const int64 Index)
		{
			FWriteScopeLock WriteLock(PointLock);
			Points.Add(Index);
		}

		void Register(TArray<TSharedPtr<FKPartition>>& Partitions);

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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** If false, will only write partition identifier values instead of splitting partitions into new point datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-2))
	bool bSplitOutput = true;

	/** Write the sum of partition values to an attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteKeySum = false;

	/** The Attribute name to write key sum to. Note that this value is not guaranteed to be unique. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteKeySum"))
	FName KeySumAttributeName = "KeySum";

	virtual bool GetPartitionRules(FPCGExContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const;
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

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExPartitonRuleConfig> PartitionRules;

	virtual bool GetPartitionRules(FPCGExContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const override;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPartitionByValuesBaseContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesBaseElement;

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
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPartitionByValuesBaseContext, UPCGExPartitionByValuesBaseSettings>
	{
		TArray<FPCGExFilter::FRule> Rules;
		TArray<int64> KeySums;

		TSharedPtr<PCGExPartition::FKPartition> RootPartition;

		int32 NumPartitions = -1;
		TArray<TSharedPtr<PCGExPartition::FKPartition>> Partitions;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
