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
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSmoothSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Smooth, "Path : Smooth", "Smooth paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetPointFilterLabel() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveStart = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPreserveEnd = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSmoothingOperation> SmoothingMethod;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType InfluenceType = EPCGExFetchType::Constant;

	/** The amount of smoothing applied. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1, EditCondition="InfluenceType == EPCGExFetchType::Constant", EditConditionHides))
	double InfluenceConstant = 1.0;

	/** Fetch the influence from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InfluenceType == EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector InfluenceAttribute;

	/** Fetch the smoothing from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType SmoothingAmountType = EPCGExFetchType::Constant;

	/** The amount of smoothing applied. \n Range of this value is highly dependant on the chosen smoothing method. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1, EditCondition="SmoothingAmountType == EPCGExFetchType::Constant", EditConditionHides))
	double SmoothingAmountConstant = 5;

	/** Fetch the smoothing amount from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SmoothingAmountType == EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SmoothingAmountAttribute;

	/** Static multiplier for the local smoothing amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001, EditCondition="SmoothingAmountType == EPCGExFetchType::Attribute", EditConditionHides))
	double ScaleSmoothingAmountAttribute = 1;

	/** Blending settings used to smooth attributes.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Average);
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSmoothContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExSmoothElement;

	virtual ~FPCGExSmoothContext() override;

	UPCGExSmoothingOperation* SmoothingMethod = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSmoothElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSmooth
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		int32 NumPoints = 0;

		TArray<double> Smoothing;
		TArray<double> Influence;

		PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;
		UPCGExSmoothingOperation* TypedOperation = nullptr;
		bool bClosedPath = false;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints): FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
