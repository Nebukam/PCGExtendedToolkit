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
		  bWriteKey(Other.bWriteKey),
		  KeyAttributeName(Other.KeyAttributeName)
	{
	}

public:
	/** Enable or disable this partition. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	bool bEnabled = true;

	/** Filter Size. Higher values means fewer, larger groups. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double FilterSize = 1.0;

	/** Upscale multiplier, applied before filtering. Handy to deal with floating point values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Upscale = 1.0;

	/** Whether to write the partition Key to an attribute. Useful for debugging. Note: They key is not the index, but instead the filtered value used to distribute into partitions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteKey;

	/** Name of the int64 attribute to write the partition Key to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteKey"))
	FName KeyAttributeName = "PartitionKey";
};

namespace PCGExPartition
{
	struct PCGEXTENDEDTOOLKIT_API FRule : public PCGEx::FLocalSingleComponentInput
	{
		FRule()
		{
		}

		FRule(FPCGExPartitionRuleDescriptor& InRule):
			FLocalSingleComponentInput(InRule.Field, InRule.Axis),
			FilterSize(InRule.FilterSize),
			Upscale(InRule.Upscale)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InRule);
			RuleDescriptor = &InRule;
		}

	public:
		FPCGExPartitionRuleDescriptor* RuleDescriptor;
		double FilterSize = 1.0;
		double Upscale = 1.0;

		int64 Filter(const FPCGPoint& Point) const
		{
			const double Upscaled = static_cast<double>(GetValue(Point)) * Upscale;
			const double Filtered = (Upscaled - FMath::Fmod(Upscaled, FilterSize)) / FilterSize;
			return static_cast<int64>(Filtered);
		}

		void WriteKey(PCGMetadataEntryKey Key, int64 Value)
		{
		}
	};

	struct FLayer;

	struct PCGEXTENDEDTOOLKIT_API FLayer
	{
		TMap<int64, FLayer*> SubLayers;
		UPCGPointData* PointData = nullptr;

		FLayer* GetLayer(int64 Key, UPCGPointData* InData, TArray<PCGExPartition::FLayer>& Array)
		{
			FLayer** LayerPtr = SubLayers.Find(Key);
			if (!LayerPtr)
			{
				FLayer* Layer = &Array.Emplace_GetRef();
				SubLayers.Add(Key, Layer);
				return Layer;
			}
			else
			{
				return *LayerPtr;
			}
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
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

public:
	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties, FullyExpand=true))
	TArray<FPCGExPartitionRuleDescriptor> PartitionRules;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSplitByValuesContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPartitionByValuesElement;

public:
	int32 Depth = 0;
	TArray<FPCGExPartitionRuleDescriptor> RulesDescriptors;
	TArray<PCGExPartition::FLayer> LayerBuffer;
	TArray<PCGExPartition::FRule> Rules;

	PCGExPartition::FLayer RootLayer;

	mutable FRWLock RulesLock;
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
