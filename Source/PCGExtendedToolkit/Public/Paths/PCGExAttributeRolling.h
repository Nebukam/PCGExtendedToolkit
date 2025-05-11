// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExAttributeRolling.generated.h"

UENUM()
enum class EPCGExRollingMode : uint8
{
	StartStop  = 0 UMETA(DisplayName = "Start/Stop", ToolTip="Uses two separate set of filters to start & stop rolling"),
	Toggle = 1 UMETA(DisplayName = "Toggle", ToolTip="Uses a single set of filter that switches roll on/off whenever a point passes"),
};

UENUM()
enum class EPCGExRollingToggleInitialValue : uint8
{
	Constant          = 0 UMETA(DisplayName = "Constant", ToolTip="Use a constant value."),
	ConstantPreserve  = 1 UMETA(DisplayName = "Constant (Preserve)", ToolTip="Use a constant value, but does not switch if the first value is the same."),
	FromPoint         = 2 UMETA(DisplayName = "From Point", ToolTip="Use the first point starting value.")
};

UENUM()
enum class EPCGExRollingTriggerMode : uint8
{
	None  = 0 UMETA(DisplayName = "None", ToolTip="Ignore triggers"),
	Reset = 2 UMETA(DisplayName = "Reset", ToolTip="Reset rolling"),
	Pin   = 4 UMETA(DisplayName = "Pin", ToolTip="Pin triggered value to roll with until next trigger"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExAttributeRollingSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExAttributeRollingSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AttributeRolling, "Path : Attribute Rolling", "Does a rolling blending of properties & attributes.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePinConditionLabel, "Filters used to determine whether a point should be pinned. Pinned points have their values rolled out to the next point.", PCGExFactories::PointFilters, true)
	//~End UPCGExPointsProcessorSettings

	/** Rolling range control */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRollingMode Mode = EPCGExRollingMode::StartStop;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRollingToggleInitialValue ToggleInitialValueMode = EPCGExRollingToggleInitialValue::Constant;

	/** Starting toggle value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ToggleInitialValueMode != EPCGExRollingToggleInitialValue::FromPoint"))
	bool bInitialToggle = false;

	/** Reverse rolling order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bReverseRolling = false;
};

struct FPCGExAttributeRollingContext final : FPCGExPathProcessorContext
{
	friend class FPCGExAttributeRollingElement;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> StartFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> StopFilterFactories;
	
	TArray<TObjectPtr<const UPCGExAttributeBlendFactory>> BlendingFactories;

	FPCGExBlendingDetails BlendingSettings;
};

class FPCGExAttributeRollingElement final : public FPCGExPathProcessorElement
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

namespace PCGExAttributeRolling
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExAttributeRollingContext, UPCGExAttributeRollingSettings>
	{
		int32 MaxIndex = 0;
		int32 SourceIndex = -1;
		bool bRoll = false;

		TSharedPtr<PCGExPointFilter::FManager> StartFilterManager;
		TSharedPtr<PCGExPointFilter::FManager> StopFilterManager;

		TSharedPtr<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>> BlendOps;

		PCGExPaths::FPathMetrics CurrentMetric;

		TArray<FPCGPoint>* OutPoints = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;
		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
