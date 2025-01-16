// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExMeanFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeanFilterConfig
{
	GENERATED_BODY()

	FPCGExMeanFilterConfig()
	{
	}

	/** Target value to compile -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector Target;

	/** Measure mode. If using relative, threshold values should be kept between 0-1, while absolute use the world-space length of the edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMeanMeasure Measure = EPCGExMeanMeasure::Relative;

	/** Which mean value is used to check whether the tested value is above or below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMeanMethod MeanMethod = EPCGExMeanMethod::Average;

	/** Minimum value threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="MeanMethod==EPCGExMeanMethod::Fixed", ClampMin=0))
	double MeanValue = 0;

	/** Used to estimate the mode value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="MeanMethod==EPCGExMeanMethod::ModeMin || MeanMethod==EPCGExMeanMethod::ModeMax", ClampMin=0))
	double ModeTolerance = 5;

	/** Exclude if value is below a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoExcludeBelowMean = false;

	/** Minimum value threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoExcludeBelowMean"))
	double ExcludeBelow = 0.2;

	/** Exclude if value is above a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoExcludeAboveMean = false;

	/** Maximum threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoExcludeAboveMean"))
	double ExcludeAbove = 0.2;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeanFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExMeanFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FMeanFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FMeanFilter(const TObjectPtr<const UPCGExMeanFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const TObjectPtr<const UPCGExMeanFilterFactory> TypedFilterFactory;

		TArray<double> Values;

		double DataMin = 0;
		double DataMax = 0;

		double ReferenceValue = 0;
		double ReferenceMin = 0;
		double ReferenceMax = 0;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual void PostInit() override;

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			return FMath::IsWithin(Values[PointIndex], ReferenceMin, ReferenceMax);
		}

		virtual ~FMeanFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeanFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		MeanFilterFactory, "Filter : Mean", "Creates a filter definition that compares values against their mean.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExMeanFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
