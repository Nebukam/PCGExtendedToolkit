// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"


#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExSubdivide.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class UPCGExSubdivideSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSubdivide, "Path : Subdivide", "Subdivide paths segments.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filter which segments will be subdivided.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Reference for computing the blending interpolation point point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType AmountInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount (Distance)", EditCondition="SubdivideMethod==EPCGExSubdivideMode::Distance && AmountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0.1))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount (Count)", EditCondition="SubdivideMethod==EPCGExSubdivideMode::Count && AmountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount (Attr)", EditCondition="AmountInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SubdivisionAmount;

	PCGEX_SETTING_VALUE_GET(SubdivisionAmount, double, AmountInput, SubdivisionAmount, SubdivideMethod == EPCGExSubdivideMode::Count ? Count : Distance)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Distance", EditConditionHides))
	bool bRedistributeEvenly = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubPoints = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bFlagSubPoints"))
	FName SubPointFlagName = "IsSubPoint";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAlpha = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bWriteAlpha"))
	FName AlphaAttributeName = "Alpha";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bWriteAlpha", EditConditionHides, HideEditConditionToggle))
	double DefaultAlpha = 1;
};

struct FPCGExSubdivideContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSubdivideElement;

	UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class FPCGExSubdivideElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Subdivide)
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSubdivide
{
	struct FSubdivision
	{
		int32 NumSubdivisions = 0;
		int32 InStart = -1;
		int32 InEnd = -1;
		int32 OutStart = -1;
		int32 OutEnd = -1;
		double Dist = 0;
		double StepSize = 0;
		double StartOffset = 0;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Dir = FVector::ZeroVector;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSubdivideContext, UPCGExSubdivideSettings>
	{
		TArray<FSubdivision> Subdivisions;

		bool bClosedLoop = false;

		TSet<FName> ProtectedAttributes;
		UPCGExSubPointsBlendOperation* Blending = nullptr;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;

		TSharedPtr<PCGExDetails::TSettingValue<double>> AmountGetter;

		double ConstantAmount = 0;

		bool bUseCount = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		
		virtual void CompleteWork() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		
		virtual void Write() override;
	};
}
