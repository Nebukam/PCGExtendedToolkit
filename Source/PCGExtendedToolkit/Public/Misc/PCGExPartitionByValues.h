// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExGraphProcessor.h"

#include "PCGExPartitionByValues.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPartitionRuleDescriptor : public FPCGExInputDescriptorWithSingleField
{
	GENERATED_BODY()

	FPCGExPartitionRuleDescriptor()
		: FPCGExInputDescriptorWithSingleField()
	{
	}

	FPCGExPartitionRuleDescriptor(const FPCGExPartitionRuleDescriptor& Other)
		: FPCGExInputDescriptorWithSingleField(Other),
		  bEnabled(Other.bEnabled),
		  FilterSize(Other.FilterSize),
		  Upscale(Other.Upscale),
		  Offset(Other.Offset),
		  bWriteKey(Other.bWriteKey),
		  KeyAttributeName(Other.KeyAttributeName)
	{
	}

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Enable or disable this partition. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	bool bEnabled = true;

	/** Filter Size. Higher values means fewer, larger groups. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double FilterSize = 1.0;

	/** Upscale multiplier, applied before filtering. Handy to deal with floating point values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Upscale = 1.0;

	/** Offset input value. Applied after upscaling the raw value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Offset = 0.0;

	/** Whether to write the partition Key to an attribute. Useful for debugging. Note: They key is not the index, but instead the filtered value used to distribute into partitions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteKey = false;

	/** Name of the int64 attribute to write the partition Key to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteKey"))
	FName KeyAttributeName = "PartitionKey";
};

namespace PCGExPartition
{
	const PCGExMT::AsyncState State_DistributeToPartition = PCGExMT::AsyncStateCounter::Unique();

	struct PCGEXTENDEDTOOLKIT_API FRule : public PCGEx::FLocalSingleFieldGetter
	{
		FRule(FPCGExPartitionRuleDescriptor& InRule)
			: FLocalSingleFieldGetter(InRule.Field, InRule.Axis),
			  RuleDescriptor(&InRule),
			  FilterSize(InRule.FilterSize),
			  Upscale(InRule.Upscale),
			  Offset(InRule.Offset)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InRule);
		}

		~FRule();

		FPCGExPartitionRuleDescriptor* RuleDescriptor;
		TArray<int64> Values;
		double FilterSize = 1.0;
		double Upscale = 1.0;
		double Offset = 0.0;

		int64 GetPartitionKey(const FPCGPoint& Point) const;
	};

	class FKPartition;

	class PCGEXTENDEDTOOLKIT_API FKPartition
	{
	protected:
		mutable FRWLock LayersLock;
		mutable FRWLock PointLock;

	public:
		FKPartition(FKPartition* InParent, int64 InKey, FRule* InRule);
		~FKPartition();

		FKPartition* Parent = nullptr;
		int64 PartitionKey = 0;
		FRule* Rule = nullptr;
		
		TMap<int64, FKPartition*> SubLayers;
		TArray<int32> Points;

		int32 GetNum() const { return Points.Num(); }
		int32 GetSubPartitionsNum();

		FKPartition* GetPartition(int64 Key, FRule* InRule);
		void Add(const int64 Index);
		void Register(TArray<FKPartition*>& Partitions);
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
	PCGEX_NODE_INFOS(PartitionByValues, "Partition by Values", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a MERGE before.");
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual bool GetMainPointsInputAcceptMultipleData() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGExData::EInit GetPointOutputInitMode() const override;

public:
	/** If false, will only write partition identifier values instead of splitting partitions into new point datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSplitOutput = true;

	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExPartitionRuleDescriptor> PartitionRules;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSplitByValuesContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesElement;

	~FPCGExSplitByValuesContext();

	TArray<FPCGExPartitionRuleDescriptor> RulesDescriptors;
	TArray<PCGExPartition::FRule> Rules;
	mutable FRWLock RulesLock;
	
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
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
