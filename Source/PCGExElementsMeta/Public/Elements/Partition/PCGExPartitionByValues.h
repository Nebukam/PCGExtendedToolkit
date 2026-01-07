// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPartition.h"
#include "Core/PCGExPointsProcessor.h"


#include "PCGExPartitionByValues.generated.h"

namespace PCGExPartition
{
	class FKPartition;

	class FKPartition : public TSharedFromThis<FKPartition>
	{
	protected:
		mutable FRWLock LayersLock;
		mutable FRWLock PointLock;

	public:
		FKPartition(const TWeakPtr<FKPartition>& InParent, int64 InKey, FRule* InRule, int32 InPartitionIndex);
		~FKPartition();

		TWeakPtr<FKPartition> Parent;
		int32 IOIndex = -1;
		int32 PartitionIndex = 0;
		int64 PartitionKey = 0;
		FRule* Rule = nullptr;

		TSet<int64> UniquePartitionKeys;
		TMap<int64, TSharedPtr<FKPartition>> SubLayers;
		TArray<int32> Points;

		int32 GetNum() const { return Points.Num(); }
		int32 GetSubPartitionsNum();

		TSharedPtr<FKPartition> GetPartition(int64 Key, FRule* InRule);

		void Add(const int64 Index);

		void Register(TArray<TSharedPtr<FKPartition>>& Partitions);

		void SortPartitions();
	};
}


/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/partition-by-values"))
class UPCGExPartitionByValuesBaseSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionByValuesStatic, "Partition by Values (Static)", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a Merge before.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool GetMainAcceptMultipleData() const override;
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
class UPCGExPartitionByValuesSettings : public UPCGExPartitionByValuesBaseSettings
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

struct FPCGExPartitionByValuesBaseContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesBaseElement;

	TArray<FPCGExPartitonRuleConfig> RulesConfigs;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExPartitionByValuesBaseElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PartitionByValuesBase)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExPartitionByValuesBase
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPartitionByValuesBaseContext, UPCGExPartitionByValuesBaseSettings>
	{
		TArray<PCGExPartition::FRule> Rules;
		TArray<int64> KeySums;

		TSharedPtr<PCGExPartition::FKPartition> RootPartition;

		int32 NumPartitions = -1;
		TArray<TSharedPtr<PCGExPartition::FKPartition>> Partitions;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
