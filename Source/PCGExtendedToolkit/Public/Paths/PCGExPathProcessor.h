// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsMT.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathProcessor.generated.h"

class UPCGExFilterFactoryBase;

namespace PCGExDataFilter
{
	class TEarlyExitFilterManager;
}

class UPCGExNodeStateFactory;

namespace PCGExCluster
{
	class FNodeStateHandler;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Subdivide Mode"))
enum class EPCGExSubdivideMode : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is static"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPath; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface


	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual FName GetPointFilterLabel() const;
	bool SupportsPointFilters() const;
	virtual bool RequiresPointFilters() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;

	virtual ~FPCGExPathProcessorContext() override;

	virtual bool ProcessorAutomation() override;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true) override;

	bool ProcessFilters();
	
	virtual bool DefaultPointFilterResult() const;
	virtual bool PrepareFiltersWithAdvance() const;

	TArray<UPCGExFilterFactoryBase*> FilterFactories;

	PCGExData::FPointIOCollection* MainPaths = nullptr;

	PCGExDataFilter::TEarlyExitFilterManager* CreatePointFilterManagerInstance(const PCGExData::FPointIO* PointIO, const bool bForcePrepare) const;

	PCGExDataFilter::TEarlyExitFilterManager* PointFiltersManager = nullptr;
	bool bRequirePointFilterPreparation = false;
	bool bWaitingOnFilterWork = false;


	bool ProcessPointsBatch();

	PCGExMT::AsyncState State_PointsProcessingDone;
	PCGExPointsMT::FPointsProcessorBatchBase* Batch = nullptr;
	TArray<PCGExData::FPointIO*> BatchablePoints;
	
	template <typename T, class ValidateEntriesFunc, class InitBatchFunc>
	bool StartProcessingClusters(ValidateEntriesFunc&& ValidateEntries, InitBatchFunc&& InitBatch, const PCGExMT::AsyncState InState)
	{
		State_PointsProcessingDone = InState;

		while (AdvancePointsIO(false))
		{
			if (!ValidateEntries(CurrentIO)) { continue; }
			BatchablePoints.Add(CurrentIO);			
		}
		
		if (BatchablePoints.IsEmpty()) { return false; }

		Batch = new T(this, BatchablePoints);
		InitBatch(Batch);

		PCGExPointsMT::ScheduleBatch(GetAsyncManager(), Batch);
		SetAsyncState(PCGExPointsMT::State_WaitingOnPointsProcessing);
		return true;
	}
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
};
