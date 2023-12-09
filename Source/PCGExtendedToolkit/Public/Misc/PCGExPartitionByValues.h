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

class UPCGExPartitionLayer;

class PCGEXTENDEDTOOLKIT_API UPCGExPartitionLayer
{
public:
	~UPCGExPartitionLayer();

	TMap<int64, UPCGExPartitionLayer*> SubLayers;
	UPCGPointData* PointData = nullptr;
	TArray<FPCGPoint>* Points = nullptr;;

protected:
	mutable FRWLock LayersLock;
	mutable FRWLock PointLock;

public:
	UPCGExPartitionLayer* GetLayer(int64 Key)
	{
		UPCGExPartitionLayer** LayerPtr;

		{
			FReadScopeLock ReadLock(LayersLock);
			LayerPtr = SubLayers.Find(Key);
		}

		if (!LayerPtr)
		{
			FWriteScopeLock WriteLock(LayersLock);
			UPCGExPartitionLayer* Layer = new UPCGExPartitionLayer();
			SubLayers.Add(Key, Layer);
			return Layer;
		}
		return *LayerPtr;
	}

	FPCGPoint& NewPoint(const FPCGPoint& Point) const
	{
		FWriteScopeLock WriteLock(PointLock);
		return Points->Add_GetRef(Point);
	}
};

namespace PCGExPartition
{
	struct PCGEXTENDEDTOOLKIT_API FRule : public PCGEx::FLocalSingleFieldGetter
	{
		FRule(FPCGExPartitionRuleDescriptor& InRule):
			FLocalSingleFieldGetter(InRule.Field, InRule.Axis),
			FilterSize(InRule.FilterSize),
			Upscale(InRule.Upscale),
			Offset(InRule.Offset)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InRule);
			RuleDescriptor = &InRule;
		}

	public:
		FPCGMetadataAttribute<int64>* KeyAttribute;
		FPCGExPartitionRuleDescriptor* RuleDescriptor;
		double FilterSize = 1.0;
		double Upscale = 1.0;
		double Offset = 0.0;


		int64 Filter(const FPCGPoint& Point) const
		{
			const double Upscaled = GetValue(Point) * Upscale + Offset;
			const double Filtered = (Upscaled - FMath::Fmod(Upscaled, FilterSize)) / FilterSize;
			return static_cast<int64>(Filtered);
		}
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
	UPCGExPartitionLayer* RootPartitionLayer;

	void PrepareForPoints(const PCGExData::FPointIO& PointIO);
	void PrepareForPointsWithMetadataEntries(PCGExData::FPointIO& PointIO);
};

class PCGEXTENDEDTOOLKIT_API FPCGExPartitionByValuesElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
