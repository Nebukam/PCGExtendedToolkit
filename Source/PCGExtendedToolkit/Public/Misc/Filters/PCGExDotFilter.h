// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
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
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterFactory : public UPCGExFilterFactoryBase
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

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TDotFilter : public PCGExDataFilter::TFilter
	{
	public:
		explicit TDotFilter(const UPCGExDotFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExDotFilterFactory* TypedFilterFactory;

		PCGEx::FLocalVectorGetter* OperandA = nullptr;
		PCGEx::FLocalVectorGetter* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TDotFilter() override
		{
			TypedFilterFactory = nullptr;
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DotFilterDefinition, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

public:
	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
