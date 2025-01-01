// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"

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

	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

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
	void GetTargetGraphPins(TArray<FPCGPinProperties>& OutPins) const;
	virtual FName GetMainInputPin() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Actor reference that we will be waiting for PCG Components with the target graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Graph instance to look for. Will wait until a PCGComponent is found with that instance set, and its output generated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UPCGGraph> TemplateGraph;

	/** If enabled, will skip components which graph instances is not the same as the specified template. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable))
	bool bMustMatchTemplate = true;

	/** If not None, will only consider components with the specified tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable))
	FName MustHaveTag = NAME_None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoMatchGenerationTrigger = false;

	/** If enabled, only process component with the specified generation trigger. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable, EditCondition="bDoMatchGenerationTrigger"))
	EPCGComponentGenerationTrigger MatchGenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;

	/** If enabled, only process component that do not match the specified generation trigger */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Filtering", meta = (PCG_Overridable, DisplayName=" └─ Invert", EditCondition="bDoMatchGenerationTrigger", EditConditionHides, HideEditConditionToggle))
	bool bInvertGenerationTrigger = false;
		
	/** If enabled, will wait for actor references to exist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable))
	bool bWaitForMissingActors = true;

	/** Time after which the search is considered a fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable, DisplayName=" └─ Timeout", EditCondition="bWaitForMissingActors", EditConditionHides, ClampMin=0.001, ClampMax=30))
	double WaitForActorTimeout = 1;

	/** If enabled, will wait for at least a single PCG component to be found that uses the target Graph. Use carefully, and only if you know for sure it will be found at some point! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable))
	bool bWaitForMissingComponents = false;
	
	/** Time after which the search is considered a fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable, DisplayName=" └─ Timeout", EditCondition="bWaitForMissingActors", EditConditionHides, ClampMin=0.001, ClampMax=30))
	double WaitForComponentTimeout = 1;

	/** If enabled, will request generation on on-demand components found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable))
	bool bTriggerOnDemand = true;

	/** Whether to force generation or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Wait Settings", meta = (PCG_Overridable, DisplayName=" └─ Force Generation", EditCondition="bTriggerOnDemand", EditConditionHides))
	bool bForceGeneration = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietActorNotFoundWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietComponentNotFoundWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietTimeoutError = false;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="Refresh Pins", ShortToolTip="Refreshes the cached pins"))
	void EDITOR_RefreshPins();
#endif

	UPROPERTY()
	TArray<FPCGPinProperties> CachedPins;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWaitForPCGDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWaitForPCGDataElement;
	TArray<FPCGPinProperties> RequiredPinProperties;
	TSet<FName> AllLabels;
	TSet<FName> RequiredLabels;
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
		FRWLock ValidComponentLock;

		TWeakPtr<PCGExMT::FAsyncToken> SearchComponentsToken;
		TWeakPtr<PCGExMT::FAsyncToken> SearchActorsToken;
		TWeakPtr<PCGExMT::FAsyncToken> WatchToken;
		TSharedPtr<PCGEx::FIntTracker> InspectionTracker;
		TSharedPtr<PCGEx::FIntTracker> WatcherTracker;
		double StartTime = 0;

		TSet<FSoftObjectPath> UniqueActorReferences;
		TArray<AActor*> QueuedActors;
		TArray<TArray<UPCGComponent*>> PerActorGatheredComponents;

		TArray<UPCGComponent*> ValidComponents;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		void GatherActors();

		void GatherComponents();

		void StartComponentSearch();
		void StopComponentSearch(bool bTimeout = false);

		void InspectGatheredComponents();
		void Inspect(int32 Index);

		void OnInspectionComplete();

	protected:
		void AddValidComponent(UPCGComponent* InComponent);
		void WatchComponent(UPCGComponent* TargetComponent, int32 Index);
		void ProcessComponent(int32 Index);
		void StageComponentData(int32 Index);
	};
}
