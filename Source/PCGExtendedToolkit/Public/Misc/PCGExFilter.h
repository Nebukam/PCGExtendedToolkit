// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExFilter.generated.h"

struct FPCGPoint;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFilterRuleDescriptor : public FPCGExInputDescriptorWithSingleField
{
	GENERATED_BODY()

	FPCGExFilterRuleDescriptor()
	{
	}

	FPCGExFilterRuleDescriptor(const FPCGExFilterRuleDescriptor& Other)
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
	virtual FString GetDisplayName() const override
	{
		if (bEnabled) { return FPCGExInputDescriptorWithSingleField::GetDisplayName(); }
		return "(Disabled) " + FPCGExInputDescriptorWithSingleField::GetDisplayName();
	}

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


namespace FPCGExFilter
{
	struct PCGEXTENDEDTOOLKIT_API FRule : public PCGEx::FLocalSingleFieldGetter
	{
		explicit FRule(FPCGExFilterRuleDescriptor& InRule)
			: FLocalSingleFieldGetter(InRule.Field, InRule.Axis),
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

		int64 Filter(const int32 Index) const
		{
			const double Upscaled = Values[Index] * Upscale + (Offset + 1);
			const double Filtered = (Upscaled - FMath::Fmod(Upscaled, FilterSize)) / FilterSize;
			return static_cast<int64>(Filtered);
		}

		virtual void Cleanup() override
		{
			Values.Empty();
			FLocalSingleFieldGetter::Cleanup();
		}
	};

	template <typename T, typename dummy = void>
	static int64 Filter(const T& InValue, const FRule& Settings)
	{
		const double Upscaled = static_cast<double>(InValue) * Settings.Upscale;
		const double Filtered = (Upscaled - FGenericPlatformMath::Fmod(Upscaled, Settings.FilterSize)) / Settings.FilterSize;
		return static_cast<int64>(Filtered);
	}

	template <typename dummy = void>
	static int64 Filter(const FVector2D& InValue, const FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Field)
		{
		case EPCGExOrderedFieldSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExOrderedFieldSelection::Y:
		case EPCGExOrderedFieldSelection::Z:
		case EPCGExOrderedFieldSelection::W:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExOrderedFieldSelection::XYZ:
		case EPCGExOrderedFieldSelection::XZY:
		case EPCGExOrderedFieldSelection::ZXY:
		case EPCGExOrderedFieldSelection::YXZ:
		case EPCGExOrderedFieldSelection::YZX:
		case EPCGExOrderedFieldSelection::ZYX:
		case EPCGExOrderedFieldSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector& InValue, const FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Field)
		{
		case EPCGExOrderedFieldSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExOrderedFieldSelection::Y:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExOrderedFieldSelection::Z:
		case EPCGExOrderedFieldSelection::W:
			Result = Filter(InValue.Z, Settings);
			break;
		case EPCGExOrderedFieldSelection::XYZ:
		case EPCGExOrderedFieldSelection::XZY:
		case EPCGExOrderedFieldSelection::YXZ:
		case EPCGExOrderedFieldSelection::YZX:
		case EPCGExOrderedFieldSelection::ZXY:
		case EPCGExOrderedFieldSelection::ZYX:
		case EPCGExOrderedFieldSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector4& InValue, const FRule& Settings)
	{
		if (Settings.Field == EPCGExSingleField::W) { return Filter(InValue.W, Settings); }
		return Filter(FVector{InValue}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FRotator& InValue, const FRule& Settings) { return Filter(FVector{InValue.Euler()}, Settings); }

	template <typename dummy = void>
	static int64 Filter(const FQuat& InValue, const FRule& Settings) { return Filter(FVector{InValue.Euler()}, Settings); }

	template <typename dummy = void>
	static int64 Filter(const FTransform& InValue, const FRule& Settings) { return Filter(InValue.GetLocation(), Settings); }

	template <typename dummy = void>
	static int64 Filter(const FString& InValue, const FRule& Settings) { return GetTypeHash(InValue); }

	template <typename dummy = void>
	static int64 Filter(const FName& InValue, const FRule& Settings) { return Filter(InValue.ToString(), Settings); }
};
