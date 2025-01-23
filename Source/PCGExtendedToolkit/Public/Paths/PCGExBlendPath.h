// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "PCGExPaths.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExBlendPath.generated.h"

class UPCGExSubPointsBlendOperation;

UENUM()
enum class EPCGExPathBlendMode : uint8
{
	Full   = 0 UMETA(DisplayName = "Start to End", ToolTip="Blend properties & attributes of all path' points from start point to last point"),
	Switch = 1 UMETA(DisplayName = "Switch", ToolTip="Switch between pruning/non-pruning based on filters"),
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBlendPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExBlendPathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BlendPath, "Path : Blend", "Blend path individual points between its start and end points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExBlendOver::Fixed", EditConditionHides))
	EPCGExInputValueType LerpInput = EPCGExInputValueType::Constant;

	/** Attribute to read the direction from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Lerp (Attr)", EditCondition="BlendOver==EPCGExBlendOver::Fixed && LerpInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LerpAttribute;

	/** Constant direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Lerp", ClampMin=0, EditCondition="BlendOver==EPCGExBlendOver::Fixed && LerpInput==EPCGExInputValueType::Constant", EditConditionHides))
	double LerpConstant = 0.5;

	/** Blending settings used to smooth attributes.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Lerp, EPCGExDataBlendingType::None);

	/** If enabled, will apply blending to othe first point. Can be useful with some blendmodes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bBlendFirstPoint = false;

	/** If enabled, will apply blending to the last  point. Can be useful with some blendmodes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bBlendLastPoint = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExBlendPathElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExBlendPath
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBlendPathContext, UPCGExBlendPathSettings>
	{
		int32 MaxIndex = 0;

		PCGExPaths::FPathMetrics Metrics;

		TSharedPtr<PCGExData::TBuffer<double>> LerpCache;
		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;

		TSharedPtr<PCGExData::FPointRef> Start;
		TSharedPtr<PCGExData::FPointRef> End;

		TArray<double> Length;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
