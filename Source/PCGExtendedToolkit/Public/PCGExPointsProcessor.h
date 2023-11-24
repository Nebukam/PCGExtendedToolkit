// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGEx.h"
#include "PCGExMT.h"
#include "Data/PCGExPointIO.h"

#include "PCGExPointsProcessor.generated.h"

class PCGEXTENDEDTOOLKIT_API FPointTask : public FNonAbandonableTask
{
public:
	virtual ~FPointTask() = default;

	FPointTask(
		FPCGContext* InContext, UPCGExPointIO* InPointData, const PCGExMT::FTaskInfos& InInfos) :
		TaskContext(InContext), PointData(InPointData), Infos(InInfos)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPointTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork()
	{
		FPCGContext* InContext = TaskContext;
		if (InContext->SourceComponent.IsValid() && !InContext->SourceComponent.IsStale(true, true)) { ExecuteTask(InContext); }
	};

	void PostDoWork() { delete this; }

	virtual void ExecuteTask(FPCGContext* InContext) = 0;

	FPCGContext* TaskContext;
	UPCGExPointIO* PointData;
	PCGExMT::FTaskInfos Infos;

};

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointsProcessorSettings, "Points Processor Settings", "TOOLTIP_TEXT");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const;

	/** Multi thread chunk size, when supported.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	int32 ChunkSize = -1;

protected:
	virtual int32 GetPreferredChunkSize() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;

public:
	UPCGExPointIOGroup* Points = nullptr;

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	UPCGExPointIO* CurrentIO = nullptr;

	bool AdvancePointsIO();
	UWorld* World = nullptr;

	PCGExMT::EState GetState() const { return CurrentState; }
	bool IsState(const PCGExMT::EState OperationId) const { return CurrentState == OperationId; }
	bool IsSetup() const { return IsState(PCGExMT::EState::Setup); }
	bool IsDone() const { return IsState(PCGExMT::EState::Done); }

	virtual void SetState(PCGExMT::EState OperationId);
	virtual void Reset();

	virtual bool ValidatePointDataInput(UPCGPointData* PointData);
	virtual void PostInitPointDataInput(UPCGExPointIO* PointData);

	int32 ChunkSize = 0;

	mutable FRWLock ContextLock;

	void OutputPoints() { Points->OutputTo(this); }

protected:
	PCGExMT::EState CurrentState = PCGExMT::EState::Setup;
	int32 CurrentPointsIndex = -1;

	template <typename T>
	void ScheduleTask(const int32 Index, const PCGMetadataEntryKey Key, const int32 Attempt = 0)
	{
		FAsyncTask<T>* AsyncTask = new FAsyncTask<T>(this, CurrentIO, PCGExMT::FTaskInfos(Index, Key, Attempt));
		AsyncTask->StartBackgroundTask();
	}

	template <typename T>
	void ScheduleTask(const PCGExMT::FTaskInfos Infos)
	{
		FAsyncTask<T>* AsyncTask = new FAsyncTask<T>(this, CurrentIO, Infos);
		AsyncTask->StartBackgroundTask();
	}
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool Validate(FPCGContext* InContext) const;
	virtual void InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	//virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
