﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "Data/PCGExPointFilter.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExSmooth.generated.h"

class UPCGExBlendOpFactory;
class UPCGExSmoothingInstancedFactory;
class FPCGExSmoothingOperation;

namespace PCGExDataBlending
{
	class IBlender;
	class FBlendOpsManager;
	class FMetadataBlender;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/smooth"))
class UPCGExSmoothSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSmooth, "Path : Smooth", "Smooth paths points.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetIOPreInitForMainPoints() const override;
	
	//~Begin UPCGExPointsProcessorSettings
public:
#if WITH_EDITORONLY_DATA
	// UObject interface
	virtual void PostInitProperties() override;
	// End of UObject interface
#endif

	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get smoothed.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveStart = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveEnd = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSmoothingInstancedFactory> SmoothingMethod;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType InfluenceInput = EPCGExInputValueType::Constant;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Influence (Attr)", EditCondition="InfluenceInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector InfluenceAttribute;

	/** The amount of smoothing applied. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Influence", ClampMin=-1, ClampMax=1, EditCondition="InfluenceInput == EPCGExInputValueType::Constant", EditConditionHides))
	double InfluenceConstant = 1.0;

	PCGEX_SETTING_VALUE_DECL(Influence, double)

	/** Fetch the smoothing from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType SmoothingAmountType = EPCGExInputValueType::Constant;

	/** Fetch the smoothing amount from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Smoothing (Attr)", EditCondition="SmoothingAmountType != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SmoothingAmountAttribute;

	/** The amount of smoothing applied.  Range of this value is highly dependant on the chosen smoothing method. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Smoothing", ClampMin=1, EditCondition="SmoothingAmountType == EPCGExInputValueType::Constant", EditConditionHides))
	double SmoothingAmountConstant = 5;

	PCGEX_SETTING_VALUE_DECL(SmoothingAmount, double)

	/** Static multiplier for the local smoothing amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001))
	double ScaleSmoothingAmountAttribute = 1;

	/** How to blend data from sampled points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable))
	EPCGExBlendingInterface BlendingInterface = EPCGExBlendingInterface::Individual;

	/** Blending settings used to smooth attributes.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="BlendingInterface == EPCGExBlendingInterface::Monolithic", EditConditionHides))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Average);
};

struct FPCGExSmoothContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSmoothElement;

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;

	UPCGExSmoothingInstancedFactory* SmoothingMethod = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExSmoothElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Smooth)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSmooth
{
	const FName SourceOverridesSmoothing = TEXT("Overrides : Smoothing");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSmoothContext, UPCGExSmoothSettings>
	{
		int32 NumPoints = 0;

		TSharedPtr<PCGExDetails::TSettingValue<double>> Influence;
		TSharedPtr<PCGExDetails::TSettingValue<double>> Smoothing;

		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;
		TSharedPtr<PCGExDataBlending::FBlendOpsManager> BlendOpsManager;
		TSharedPtr<PCGExDataBlending::IBlender> DataBlender;

		TSharedPtr<FPCGExSmoothingOperation> SmoothingOperation;

		bool bClosedLoop = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};
}
