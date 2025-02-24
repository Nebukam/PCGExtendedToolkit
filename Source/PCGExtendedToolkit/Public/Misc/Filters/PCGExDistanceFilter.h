// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExDistanceFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDistanceFilterConfig
{
	GENERATED_BODY()

	FPCGExDistanceFilterConfig()
	{
	}

	/** Distance method to be used for source & target points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExDistanceDetails DistanceDetails;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distance Threshold (Attr)", EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DistanceThreshold;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distance Threshold", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	double DistanceThresholdConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** If enabled, a collection will never be tested against itself */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDistanceFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExDistanceFilterConfig Config;

	TArray<const PointOctree*> Octrees;
	TArray<uintptr_t> TargetsPtr;

	virtual bool SupportsDirectEvaluation() const override;

	virtual bool Init(FPCGExContext* InContext) override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual bool Prepare(FPCGExContext* InContext) override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FDistanceFilter final : public FSimpleFilter
	{
	public:
		explicit FDistanceFilter(const TObjectPtr<const UPCGExDistanceFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
			Octrees = &TypedFilterFactory->Octrees;
			TargetsPtr = &TypedFilterFactory->TargetsPtr;
			bIgnoreSelf = TypedFilterFactory->Config.bIgnoreSelf;
		}

		const TObjectPtr<const UPCGExDistanceFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExDetails::FDistances> Distances;

		const TArray<const UPCGPointData::PointOctree*>* Octrees = nullptr;
		const TArray<uintptr_t>* TargetsPtr = nullptr;
		uintptr_t SelfPtr = 0;
		bool bIgnoreSelf = false;

		const FPCGPoint* InPointsStart = nullptr;
		int32 NumTargets = -1;

		const double DistanceThreshold = 0;
		TSharedPtr<PCGExData::TBuffer<double>> DistanceThresholdGetter;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const FPCGPoint& Point) const override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FDistanceFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDistanceFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DistanceFilterFactory, "Filter : Distance", "Creates a filter definition that compares the distance from the point to the nearest target.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDistanceFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
