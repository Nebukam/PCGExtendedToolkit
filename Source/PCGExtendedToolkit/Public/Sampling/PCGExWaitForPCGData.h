// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGExBufferHelper.h"

#include "PCGExWaitForPCGData.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExExpectedPin
{
	GENERATED_BODY()

	FPCGExExpectedPin()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName Label = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDataType AllowedTypes = EPCGDataType::Any;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWaitForPCGDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExWaitForPCGDataSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WaitForPCGData, "Wait for PCG Data", "Wait for PCG Components Generated output.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorDebug; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings

public:
	virtual FName GetMainInputPin() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Actor reference that we will be waiting for PCG Components with the target graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Graph instance to look for. Will wait until a PCGComponent is found with that instance set, and its output generated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UPCGGraph> TargetGraph;

	/** Time after which the search is considered a fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1, ClampMax=30))
	double Timeout = 10;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWaitForPCGDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWaitForPCGDataElement;
	TArray<FPCGPinProperties> RequiredPinProperties;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWaitForPCGDataElement final : public FPCGExPointsProcessorElement
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

namespace PCGExWaitForPCGData
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWaitForPCGDataContext, UPCGExWaitForPCGDataSettings>
	{
		TSharedPtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences;
		TArray<AActor*> QueuedActors;
		TArray<TArray<UPCGComponent*>> QueuedComponents;
		
		int32 Ticks = 0;
		double StartTime = 0;
		
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void StartQueue();
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;
		virtual void CompleteWork() override;
	};
}
