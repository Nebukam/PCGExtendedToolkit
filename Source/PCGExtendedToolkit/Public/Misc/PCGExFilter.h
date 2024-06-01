// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExFilter.generated.h"

struct FPCGPoint;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFilterRuleDescriptor : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExFilterRuleDescriptor()
	{
	}

	FPCGExFilterRuleDescriptor(const FPCGExFilterRuleDescriptor& Other)
		: FPCGExInputDescriptor(Other),
		  bEnabled(Other.bEnabled),
		  FilterSize(Other.FilterSize),
		  Upscale(Other.Upscale),
		  Offset(Other.Offset),
		  bWriteKey(Other.bWriteKey),
		  KeyAttributeName(Other.KeyAttributeName),
		  bUsePartitionIndexAsKey(Other.bUsePartitionIndexAsKey),
		  bWriteTag(Other.bWriteTag),
		  TagPrefixName(Other.TagPrefixName),
		  bTagUsePartitionIndexAsKey(Other.bTagUsePartitionIndexAsKey)
	{
	}

#if WITH_EDITOR
	virtual FString GetDisplayName() const override
	{
		if (bEnabled) { return FPCGExInputDescriptor::GetDisplayName(); }
		return "(Disabled) " + FPCGExInputDescriptor::GetDisplayName();
	}

#endif

	/** Enable or disable this partition. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	bool bEnabled = true;

	/** Filter Size. Higher values means fewer, larger groups. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0.001))
	double FilterSize = 1.0;

	/** Upscale multiplier, applied before filtering. Handy to deal with floating point values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0.001))
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

	/** Output the partition index instead of the value used for partitioning. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteKey"))
	bool bUsePartitionIndexAsKey = false;

	/** Whether to write the partition Key to a tag. Will write tags as 'Prefix::Key' */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTag = false;

	/** Name of the tag prefix used for this partition. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteTag"))
	FName TagPrefixName = "Partition";

	/** Output the partition index to the tag instead of the value used for partitioning. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteTag"))
	bool bTagUsePartitionIndexAsKey = false;
};


namespace FPCGExFilter
{
	struct PCGEXTENDEDTOOLKIT_API FRule : public PCGEx::FLocalSingleFieldGetter
	{
		explicit FRule(FPCGExFilterRuleDescriptor& InRule)
			: FLocalSingleFieldGetter(),
			  RuleDescriptor(&InRule),
			  FilterSize(InRule.FilterSize),
			  Upscale(InRule.Upscale),
			  Offset(InRule.Offset)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InRule);
		}

		virtual ~FRule() override
		{
			FRule::Cleanup();
			RuleDescriptor = nullptr;
		}

		FPCGExFilterRuleDescriptor* RuleDescriptor;
		TArray<int64> FilteredValues;
		double FilterSize = 1.0;
		double Upscale = 1.0;
		double Offset = 0.0;

		FORCEINLINE int64 Filter(const int32 Index) const
		{
			const double Upscaled = Values[Index] * Upscale + (Offset);
			const double Filtered = (Upscaled - FMath::Fmod(Upscaled, FilterSize)) / FilterSize + PCGExMath::SignPlus(Upscaled);
			return static_cast<int64>(Filtered);
		}

		virtual void Cleanup() override
		{
			Values.Empty();
			FLocalSingleFieldGetter::Cleanup();
		}
	};
};
