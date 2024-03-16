// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExDotFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDotFilterDescriptor
{
	GENERATED_BODY()

	FPCGExDotFilterDescriptor()
	{
	}

	/** Vector operand A */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector OperandA;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Constant;

	/** Operand B for computing the dot product */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for computing the dot product. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	FVector OperandBConstant = FVector::UpVector;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUnsignedDot = false;

	/** Exclude if value is below a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoExcludeBelowDot = false;

	/** Minimum value threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bExcludeBelowDot"))
	double ExcludeBelow = 0;

	/** Exclude if value is above a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoExcludeAboveDot = false;

	/** Maximum threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bExcludeAboveDot"))
	double ExcludeAbove = 0.5;


#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterDefinition : public UPCGExFilterDefinitionBase
{
	GENERATED_BODY()

public:
	FPCGAttributePropertyInputSelector OperandA;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Constant;
	FPCGAttributePropertyInputSelector OperandB;
	FVector OperandBConstant = FVector::UpVector;
	bool bUnsignedDot = false;
	bool bExcludeBelowDot = false;
	double ExcludeBelow = 0;
	bool bExcludeAboveDot = false;
	double ExcludeAbove = 0.5;

	void ApplyDescriptor(const FPCGExDotFilterDescriptor& Descriptor)
	{
		OperandA = Descriptor.OperandA;
		CompareAgainst = Descriptor.CompareAgainst;
		OperandB = Descriptor.OperandB;
		OperandBConstant = Descriptor.OperandBConstant;
		bUnsignedDot = Descriptor.bUnsignedDot;
		bExcludeBelowDot = Descriptor.bDoExcludeBelowDot;
		ExcludeBelow = Descriptor.ExcludeBelow;
		bExcludeAboveDot = Descriptor.bDoExcludeAboveDot;
		ExcludeAbove = Descriptor.ExcludeAbove;
	}

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;
	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TDotHandler : public PCGExDataFilter::TFilterHandler
	{
	public:
		explicit TDotHandler(const UPCGExDotFilterDefinition* InDefinition)
			: TFilterHandler(InDefinition), DotFilter(InDefinition)
		{
		}

		const UPCGExDotFilterDefinition* DotFilter;

		PCGEx::FLocalVectorGetter* OperandA = nullptr;
		PCGEx::FLocalVectorGetter* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TDotHandler() override
		{
			DotFilter = nullptr;
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterDefinitionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DotFilterDefinition, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
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
	FPCGExDotFilterDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDotFilterDefinitionElement : public IPCGElement
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
