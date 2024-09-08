// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPointsProcessor.h"
#include "PCGExPrunePath.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action"))
enum class EPCGExPathPruneTriggerMode : uint8
{
	Once UMETA(DisplayName = "Once", ToolTip="Each point is pruned individually based on filters."),
	Switch UMETA(DisplayName = "Switch", ToolTip="Switch between pruning/non-pruning based on filters"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPrunePathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PrunePath, "Path : Prune", "Prune paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual bool RequiresPointFilters() const override { return true; }
	virtual FName GetPointFilterLabel() const override;

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Select pruning mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPathPruneTriggerMode TriggerMode = EPCGExPathPruneTriggerMode::Once;

	/** Initial switch state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TriggerMode==EPCGExPathPruneTriggerMode::Switch", EditConditionHides))
	bool bInitialSwitchValue = false;

	/** Initial switch state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGenerateNewPaths = false;

	/** Initial switch state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertFilterValue = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPrunePathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPrunePathElement;

	virtual ~FPCGExPrunePathContext() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPrunePathElement final : public FPCGExPathProcessorElement
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

namespace PCGExPrunePath
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExPrunePathContext* LocalTypedContext = nullptr;
		const UPCGExPrunePathSettings* LocalSettings = nullptr;

		bool bClosedPath = false;

		int32 CachedIndex = 0;
		int32 LastValidIndex = -1;
		bool bCurrentSwitch = false;

		PCGExData::FPointIO* PathBegin = nullptr;
		PCGExData::FPointIO* CurrentPath = nullptr;
		TArray<FPCGPoint>* OutPoints = nullptr;
		UPCGMetadata* OutMetadata = nullptr;

		TArray<PCGExData::FPointIO*> Outputs;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		PCGExData::FPointIO* NewPathIO();
		virtual void CompleteWork() override;
		virtual void Output() override;
	};
}
