// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"
#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataForwardDetails.h"


#include "PCGExWaitForPCGData.generated.h"

class FPCGExIntTracker;

namespace PCGEx
{
	class FPCGExIntTracker;
}

namespace PCGExMT
{
	class FAsyncToken;
}

UENUM()
enum class EPCGExGenerationTriggerAction : uint8
{
	Ignore        = 0 UMETA(DisplayName = "Ignore", ToolTip="Ignore component if not actively generating already"),
	AsIs          = 1 UMETA(DisplayName = "As-is", ToolTip="Grab the data as-is and doesnt'try to generate if it wasn't."),
	Generate      = 2 UMETA(DisplayName = "Generate", ToolTip="Generate and wait for completion. If the component was already generated, this should not trigger a regeneration."),
	ForceGenerate = 3 UMETA(DisplayName = "Generate (force)", ToolTip="Generate (force) and wait for completion. Already generated component will be re-regenerated."),
};

UENUM()
enum class EPCGExRuntimeGenerationTriggerAction : uint8
{
	Ignore       = 0 UMETA(DisplayName = "Ignore", ToolTip="Ignore component if not actively generating already"),
	AsIs         = 1 UMETA(DisplayName = "As-is", ToolTip="Grab the data as-is and doesnt'try to refresh it."),
	RefreshFirst = 2 UMETA(DisplayName = "Refresh", ToolTip="Refresh and wait for completion"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class UPCGExWaitForPCGDataSettings : public UPCGExPointsProcessorSettings
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
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Action); }
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings


	//~Begin UPCGExPointsProcessorSettings

public:
	void GetTargetGraphPins(TArray<FPCGPinProperties>& OutPins) const;
	virtual FName GetMainInputPin() const override;
	//~End UPCGExPointsProcessorSettings

	/** Actor reference that we will be waiting for PCG Components with the target graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Actor reference that we will be waiting for PCG Components with the target graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExDataInputValueType TemplateInput = EPCGExDataInputValueType::Constant;

	/** Graph instance to look for. Will wait until a PCGComponent is found with that instance set, and its output generated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="TemplateInput == EPCGExDataInputValueType::Constant"))
	TSoftObjectPtr<UPCGGraph> TemplateGraph;

	/** Graph instance to look for. Will wait until a PCGComponent is found with that instance set, and its output generated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="TemplateInput == EPCGExDataInputValueType::Attribute"))
	FName TemplateGraphAttributeName = FName("@Data.TemplateGraph");

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable))
	bool bWaitForMissingActors = true;

	/** Time after which the search is considered a fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable, DisplayName=" └─ Timeout", EditCondition="bWaitForMissingActors", EditConditionHides, ClampMin=0.001, ClampMax=30))
	double WaitForActorTimeout = 1;

	/** If enabled, will wait for at least a single PCG component to be found that uses the target Graph. Use carefully, and only if you know for sure it will be found at some point! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable))
	bool bWaitForMissingComponents = false;

	/** Time after which the search is considered a fail. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable, DisplayName=" └─ Timeout", EditCondition="bWaitForMissingActors", EditConditionHides, ClampMin=0.001, ClampMax=30))
	double WaitForComponentTimeout = 1;

	/** How to deal with found components that have the trigger condition 'GenerateOnLoad'*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable, DisplayName="Grab GenerateOnLoad"))
	EPCGExGenerationTriggerAction GenerateOnLoadAction = EPCGExGenerationTriggerAction::Generate;

	/** How to deal with found components that have the trigger condition 'GenerateOnDemand'*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable, DisplayName="Grab GenerateOnDemand"))
	EPCGExGenerationTriggerAction GenerateOnDemandAction = EPCGExGenerationTriggerAction::Generate;

	/** How to deal with found components that have the trigger condition 'GenerateAtRuntime'*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Gen & Wait Settings", meta = (PCG_Overridable, DisplayName="Grab GenerateAtRuntime"))
	EPCGExRuntimeGenerationTriggerAction GenerateAtRuntime = EPCGExRuntimeGenerationTriggerAction::AsIs;

	/** If enabled, available data will be output even if some required pins have no data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bIgnoreRequiredPin = false;

	/** If enabled, only output component data once per unique actor. Otherwise, output data as many time as found. Note that when enabled, TargetIndexToTag will be disabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bDedupeData = true;

	/** If enabled, target collections' tags will be added to the output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bCarryOverTargetTags = true;

	/** Lets you tag output data with attribute values from target points input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	FPCGExAttributeToTagDetails TargetAttributesToDataTags;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bOutputRoaming = true;

	/** If enabled, adds an extra pin that contains all the data that isn't part of the template. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bOutputRoaming"))
	FName RoamingPin = FName("Roaming Data");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietActorNotFoundWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietComponentNotFoundWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTimeoutError = false;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="Refresh Pins", ShortToolTip="Refreshes the cached pins"))
	void EDITOR_RefreshPins();
#endif

	UPROPERTY()
	TArray<FPCGPinProperties> CachedPins;
};

struct FPCGExWaitForPCGDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWaitForPCGDataElement;

	virtual void RegisterAssetDependencies() override;

	TArray<FPCGPinProperties> RequiredPinProperties;
	TSet<FName> AllLabels;
	TSet<FName> RequiredLabels;

	TArray<FSoftObjectPath> GraphInstancePaths;
	TArray<UPCGGraph*> GraphInstances;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExWaitForPCGDataElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WaitForPCGData)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExWaitForPCGData
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExWaitForPCGDataContext, UPCGExWaitForPCGDataSettings>
	{
		friend class FStageComponentDataTask;

		FRWLock ValidComponentLock;

		TObjectPtr<UPCGGraph> TemplateGraph;

		TWeakPtr<PCGExMT::FAsyncToken> SearchComponentsToken;
		TWeakPtr<PCGExMT::FAsyncToken> SearchActorsToken;
		TWeakPtr<PCGExMT::FAsyncToken> WatchToken;
		TSharedPtr<FPCGExIntTracker> InspectionTracker;
		TSharedPtr<FPCGExIntTracker> WatcherTracker;
		double StartTime = 0;

		TSet<FSoftObjectPath> UniqueActorReferences;
		TArray<AActor*> QueuedActors;
		TArray<TArray<UPCGComponent*>> PerActorGatheredComponents;

		TMap<FSoftObjectPath, TSharedPtr<TArray<int32>>> PerActorPoints;

		TArray<UPCGComponent*> ValidComponents;

		FPCGExAttributeToTagDetails TargetAttributesToDataTags;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

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
		void ScheduleComponentDataStaging(int32 Index);
		void StageComponentData(int32 Index);
	};
}
