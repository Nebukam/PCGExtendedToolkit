// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilter.h"

#include "Graph/PCGExCustomGraphProcessor.h"

#include "PCGExPartitionByValues.generated.h"

namespace PCGExPartition
{
	constexpr PCGExMT::AsyncState State_DistributeToPartition = __COUNTER__;

	class FKPartition;

	class PCGEXTENDEDTOOLKIT_API FKPartition
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
		void Add(const int64 Index);
		void Register(TArray<FKPartition*>& Partitions);

		void SortPartitions();
	};
}


/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPartitionByValuesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionByValues, "Partition by Values", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a Merge before.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual bool GetMainAcceptMultipleData() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** If false, will only write partition identifier values instead of splitting partitions into new point datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSplitOutput = true;

	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExFilterRuleDescriptor> PartitionRules;

	/** Write the sum of partition values to an attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteKeySum = false;

	/** The Attribute name to write key sum to. Note that this value is not guaranteed to be unique. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteKeySum"))
	FName KeySumAttributeName = "KeySum";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPartitionByValuesContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesElement;

	virtual ~FPCGExPartitionByValuesContext() override;

	TArray<FPCGExFilterRuleDescriptor> RulesDescriptors;
	TArray<FPCGExFilter::FRule> Rules;
	mutable FRWLock RulesLock;

	TArray<int64> KeySums;

	bool bSplitOutput = true;
	PCGExPartition::FKPartition* RootPartition = nullptr;

	int32 NumPartitions = -1;
	TArray<PCGExPartition::FKPartition*> Partitions;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPartitionByValuesElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
