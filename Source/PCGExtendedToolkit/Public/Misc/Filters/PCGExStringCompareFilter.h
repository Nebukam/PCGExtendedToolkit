// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExStringCompareFilter.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] String Comparison"))
enum class EPCGExStringComparison : uint8
{
	StrictlyEqual UMETA(DisplayName = " == ", Tooltip="Operand A Strictly Equal to Operand B"),
	StrictlyNotEqual UMETA(DisplayName = " != ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthStrictlyEqual UMETA(DisplayName = " == (Length) ", Tooltip="Operand A Strictly Equal to Operand B"),
	LengthStrictlyUnequal UMETA(DisplayName = " != (Length) ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthEqualOrGreater UMETA(DisplayName = " >= (Length)", Tooltip="Operand A Equal or Greater to Operand B"),
	LengthEqualOrSmaller UMETA(DisplayName = " <= (Length)", Tooltip="Operand A Equal or Smaller to Operand B"),
	StrictlyGreater UMETA(DisplayName = " > (Length)", Tooltip="Operand A Strictly Greater to Operand B"),
	StrictlySmaller UMETA(DisplayName = " < (Length)", Tooltip="Operand A Strictly Smaller to Operand B"),
	LocaleStrictlyGreater UMETA(DisplayName = " > (Locale)", Tooltip="Operand A Locale Strictly Greater to Operand B Locale"),
	LocaleStrictlySmaller UMETA(DisplayName = " < (Locale)", Tooltip="Operand A Locale Strictly Smaller to Operand B Locale"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExStringCompareFilterDescriptor
{
	GENERATED_BODY()

	FPCGExStringCompareFilterDescriptor()
	{
	}

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	FString OperandBConstant = TEXT("PCGEx");

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExStringCompareFilterDefinition : public UPCGExFilterDefinitionBase
{
	GENERATED_BODY()

public:
	FPCGExInputDescriptor OperandA;
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;
	FPCGExInputDescriptor OperandB;
	FString OperandBConstant;

	void ApplyDescriptor(const FPCGExStringCompareFilterDescriptor& Descriptor)
	{
		OperandA = FPCGExInputDescriptor(Descriptor.OperandA);
		Comparison = Descriptor.Comparison;
		CompareAgainst = Descriptor.CompareAgainst;
		OperandB = FPCGExInputDescriptor(Descriptor.OperandB);
		OperandBConstant = Descriptor.OperandBConstant;
	}

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;
	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TStringComparisonHandler : public PCGExDataFilter::TFilterHandler
	{
	public:
		explicit TStringComparisonHandler(const UPCGExStringCompareFilterDefinition* InDefinition)
			: TFilterHandler(InDefinition), CompareFilter(InDefinition)
		{
		}

		const UPCGExStringCompareFilterDefinition* CompareFilter;

		PCGEx::TFAttributeReader<FString>* OperandA = nullptr;
		PCGEx::TFAttributeReader<FString>* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TStringComparisonHandler() override
		{
			CompareFilter = nullptr;
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExStringCompareFilterDefinitionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterDefinition, "Filter : String Compare", "Creates a filter definition that compares two attribute values.",
		FName(Descriptor.GetDisplayName()))
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorFilter; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** State name.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStringCompareFilterDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExStringCompareFilterDefinitionElement : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};
