// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExCompare.h"
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

	/** Transform OperandA with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandA = false;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Constant;

	/** Operand B for computing the dot product */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for computing the dot product. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	FVector OperandBConstant = FVector::UpVector;

	/** Transform OperandB with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandB = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonSettings DotComparisonSettings;

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
	FPCGExDotFilterDescriptor Descriptor;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TDotFilter final : public PCGExDataFilter::TFilter
	{
	public:
		explicit TDotFilter(const UPCGExDotFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExDotFilterFactory* TypedFilterFactory;

		double DotTolerance = 0;

		PCGEx::FLocalVectorGetter* OperandA = nullptr;
		PCGEx::FLocalVectorGetter* OperandB = nullptr;
		PCGEx::FLocalSingleFieldGetter* DotValue = nullptr;

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
		DotFilterFactory, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
		PCGEX_FACTORY_NAME_PRIORITY)
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
