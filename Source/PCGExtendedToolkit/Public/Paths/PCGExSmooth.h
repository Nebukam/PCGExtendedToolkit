// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "Smoothing/PCGExSmoothingOperation.h"
#include "PCGExSmooth.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSmoothSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Smooth, "Path : Smooth", "Smooth paths points.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get smoothed.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveStart = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveEnd = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSmoothingOperation> SmoothingMethod;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType InfluenceInput = EPCGExInputValueType::Constant;

	/** The amount of smoothing applied. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1, EditCondition="InfluenceInput == EPCGExInputValueType::Constant", EditConditionHides))
	double InfluenceConstant = 1.0;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InfluenceInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector InfluenceAttribute;

	/** Fetch the smoothing from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType SmoothingAmountType = EPCGExInputValueType::Constant;

	/** The amount of smoothing applied.  Range of this value is highly dependant on the chosen smoothing method. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1, EditCondition="SmoothingAmountType == EPCGExInputValueType::Constant", EditConditionHides))
	double SmoothingAmountConstant = 5;

	/** Fetch the smoothing amount from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SmoothingAmountType == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SmoothingAmountAttribute;

	/** Static multiplier for the local smoothing amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001, EditCondition="SmoothingAmountType == EPCGExInputValueType::Attribute", EditConditionHides))
	double ScaleSmoothingAmountAttribute = 1;

	/** Blending settings used to smooth attributes.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Average);
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSmoothContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSmoothElement;

	UPCGExSmoothingOperation* SmoothingMethod = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSmoothElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSmooth
{
	const FName SourceOverridesSmoothing = TEXT("Overrides : Smoothing");
	
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSmoothContext, UPCGExSmoothSettings>
	{
		int32 NumPoints = 0;

		TSharedPtr<PCGExData::TBuffer<double>> Influence;
		TSharedPtr<PCGExData::TBuffer<double>> Smoothing;

		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;
		UPCGExSmoothingOperation* TypedOperation = nullptr;
		bool bClosedLoop = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
