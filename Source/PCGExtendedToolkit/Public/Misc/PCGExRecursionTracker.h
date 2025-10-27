// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"

#include "PCGExRecursionTracker.generated.h"

UENUM()
enum class EPCGExRecursionTrackerMode : uint8
{
	Create         = 0 UMETA(DisplayName = "Create", Tooltip="Create a new tracker. This is for creating an initial tracker outside the subgraph."),
	Mutate         = 1 UMETA(DisplayName = "Mutate", Tooltip="Process and update an existing tracker. This is for use inside the recursive subgraph."),
	CreateOrMutate = 2 UMETA(DisplayName = "Create Or Mutate", Tooltip="Create a new tracker if input is empty, otherwise fallback to mutate. Useful to create tracker directly inside the recursive subgraph."),
};

namespace PCGExRecursionTracker
{
	const FName SourceTrackerFilters = FName("Tracker Filters");
	const FName SourceTestData = FName("Test Data");
	const FName OutputProgressLabel = FName("Progress");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="quality-of-life/recursion-tracker"))
class UPCGExRecursionTrackerSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExRecursionTrackerElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(RecursionTracker, "Recursion Tracker", "A Simple Recursion tracker to make working with recursive subgraphs easier.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorConstant; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** How is this recursion tracker supposed to be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExRecursionTrackerMode Mode = EPCGExRecursionTrackerMode::Create;

	/** Name of the bool attribute that will be set on the tracker. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Mutate", EditConditionHides, ClampMin=0))
	FName ContinueAttributeName = "Continue";

	/** Starting count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Mutate", EditConditionHides, ClampMin=0))
	int32 Count = 20;

	/** Tags to be added to the tracker */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable))
	FString AddTags = TEXT("");

	/** Tags to be removed from the tracker(s) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Create", EditConditionHides))
	FString RemoveTags = TEXT("");

	/** If enabled, will create a pin that outputs the normalized progress value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Create", EditConditionHides))
	bool bOutputProgress = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ One Minus", EditCondition="Mode != EPCGExRecursionTrackerMode::Create && bOutputProgress", EditConditionHides))
	bool bOneMinus = false;

	/** If enabled, will override the value of the "Continue" attribute to a valid one. Use this is you give the tracker some attribute set that may already have a boolean with the same name and a wrong value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Create", EditConditionHides), AdvancedDisplay)
	bool bForceOutputContinue = false;

	/** If enabled, will not output data if counter runs out of iterations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Create", EditConditionHides), AdvancedDisplay)
	bool bEmptyOutputOnStop = false;
	
	/** If enabled, does additional collection-level filtering on a separate set of datas. If no data passes those filters, the tracker will return a single false value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExRecursionTrackerMode::Create", EditConditionHides))
	bool bDoAdditionalDataTesting = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExRecursionTrackerMode::CreateOrMutate", EditConditionHides), AdvancedDisplay)
	int32 OffsetOnCreateFromEmpty = -1;
};

class FPCGExRecursionTrackerElement final : public IPCGElement
{
public:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
