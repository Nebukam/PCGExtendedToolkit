// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExMeanFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMeanFilterDescriptor
{
	GENERATED_BODY()

	FPCGExMeanFilterDescriptor()
	{
	}

	/** Target value to compile -- Will be broadcasted to `double` under the hood. */
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bExcludeBelowMean"))
	double ExcludeBelow = 0.2;

	/** Exclude if value is above a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoExcludeAboveMean = false;

	/** Maximum threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bExcludeAboveMean"))
	double ExcludeAbove = 0.2;

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMeanFilterDefinition : public UPCGExFilterDefinitionBase
{
	GENERATED_BODY()

public:
	FPCGAttributePropertyInputSelector Target;
	EPCGExMeanMeasure Measure = EPCGExMeanMeasure::Relative;
	EPCGExMeanMethod MeanMethod = EPCGExMeanMethod::Average;
	double MeanValue = 0;
	double ModeTolerance = 5;
	bool bPruneBelowMean = false;
	double PruneBelow = 0.2;
	bool bPruneAboveMean = false;
	double PruneAbove = 0.2;

	void ApplyDescriptor(const FPCGExMeanFilterDescriptor& Descriptor)
	{
		Target = Descriptor.Target;
		Measure = Descriptor.Measure;
		MeanMethod = Descriptor.MeanMethod;
		MeanValue = Descriptor.MeanValue;
		ModeTolerance = Descriptor.ModeTolerance;
		bPruneBelowMean = Descriptor.bDoExcludeBelowMean;
		PruneBelow = Descriptor.ExcludeBelow;
		bPruneAboveMean = Descriptor.bDoExcludeAboveMean;
		PruneAbove = Descriptor.ExcludeAbove;
	}

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;
	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TMeanHandler : public PCGExDataFilter::TFilterHandler
	{
	public:
		explicit TMeanHandler(const UPCGExMeanFilterDefinition* InDefinition)
			: TFilterHandler(InDefinition), MeanFilter(InDefinition)
		{
		}

		const UPCGExMeanFilterDefinition* MeanFilter;

		PCGEx::FLocalSingleFieldGetter* Target = nullptr;

		double ReferenceValue = 0;
		double ReferenceMin = 0;
		double ReferenceMax = 0;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual void PrepareForTesting(PCGExData::FPointIO* PointIO) override;

		virtual ~TMeanHandler() override
		{
			MeanFilter = nullptr;
			PCGEX_DELETE(Target)
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExMeanFilterDefinitionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		MeanFilterDefinition, "Filter : Mean", "Creates a filter definition that compares values against their mean.",
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
	FPCGExMeanFilterDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExMeanFilterDefinitionElement : public IPCGElement
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
