﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"

#include "PCGExDotFilter.generated.h"


USTRUCT(BlueprintType)
struct FPCGExDotFilterConfig
{
	GENERATED_BODY()

	FPCGExDotFilterConfig()
	{
	}

	/** Vector operand A */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector OperandA;

	/** Transform OperandA with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandA = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Invert"))
	bool bInvertOperandA = false;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for computing the dot product */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B (Attr)", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Invert", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	bool bInvertOperandB = false;

	/** Operand B for computing the dot product. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst == EPCGExInputValueType::Constant", EditConditionHides))
	FVector OperandBConstant = FVector::UpVector;

	/** Transform OperandB with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandB = false;

	PCGEX_SETTING_VALUE_DECL(OperandB, FVector)

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonDetails DotComparisonDetails;

	void Sanitize()
	{
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExDotFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExDotFilterConfig Config;

	virtual bool Init(FPCGExContext* InContext) override;

	virtual bool DomainCheck() override;
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointFilter
{
	class FDotFilter final : public ISimpleFilter
	{
	public:
		explicit FDotFilter(const TObjectPtr<const UPCGExDotFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
		}

		const TObjectPtr<const UPCGExDotFilterFactory> TypedFilterFactory;

		FPCGExDotComparisonDetails DotComparison;

		TSharedPtr<PCGExData::TBuffer<FVector>> OperandA;
		double OperandAMultiplier = 1;

		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OperandB;
		double OperandBMultiplier = 1;

		TConstPCGValueRange<FTransform> InTransforms;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const int32 PointIndex) const override;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FDotFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/math-checks/dot-product"))
class UPCGExDotFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DotFilterFactory, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
