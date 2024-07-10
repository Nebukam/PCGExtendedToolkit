// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExSplitPath.generated.h"

class UPCGExEdgeRefineOperation;

namespace PCGExSplitPath
{
	const FName SourceSplitFilters = TEXT("SplitConditions");
	const FName SourceRemoveFilters = TEXT("RemoveConditions");

	struct PCGEXTENDEDTOOLKIT_API FPath
	{
		int32 Start = -1;
		int32 End = -1;
		int32 Count = 0;

		FPath()
		{
		}
	};
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action"))
enum class EPCGExPathSplitAction : uint8
{
	Split UMETA(DisplayName = "Split", ToolTip="Duplicate the split point so the original becomes a new end, and the copy a new start."),
	Remove UMETA(DisplayName = "Remove", ToolTip="Remove the split point, shrinking both the previous and next paths."),
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSplitPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SplitPath, "Path : Split", "Split existing paths into multiple new paths.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** If both split and remove are true, the selected behavior takes priority */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathSplitAction Prioritize = EPCGExPathSplitAction::Split;

	/** Whether to output single-point data or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOmitSinglePointOutputs = true;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSplitPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExSplitPathElement;

	virtual ~FPCGExSplitPathContext() override;

	TArray<UPCGExFilterFactoryBase*> SplitFilterFactories;
	TArray<UPCGExFilterFactoryBase*> RemoveFilterFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSplitPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExSplitPath
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExSplitPathContext* LocalTypedContext = nullptr;
		const UPCGExSplitPathSettings* LocalSettings = nullptr;

		TArray<bool> DoSplit;
		TArray<bool> DoRemove;
		TArray<FPath> Paths;

		int32 LastValidIndex = -1;
		int32 CurrentPath = -1;

		bool bPriorityToSplit = true;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;

		UPCGExEdgeRefineOperation* Refinement = nullptr;
	};
}
