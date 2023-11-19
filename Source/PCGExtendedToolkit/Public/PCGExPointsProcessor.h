// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGSettings.h"
#include "PCGExPointIO.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointsProcessor.generated.h"

#define PCGEX_OUT_ATTRIBUTE(_NAME, _TYPE)\
bool bWrite##_NAME = false;\
FName OutName##_NAME = NAME_None;\
FPCGMetadataAttribute<_TYPE>* OutAttribute##_NAME = nullptr;
#define PCGEX_FORWARD_ATTRIBUTE(_NAME, _TOGGLE, _SETTING_NAME)\
Context->bWrite##_NAME = Settings->_TOGGLE;\
Context->OutName##_NAME = Settings->_SETTING_NAME;

#define PCGEX_CHECK_OUTNAME(_NAME)\
if(Context->bWrite##_NAME && !PCGEx::Common::IsValidName(Context->OutName##_NAME))\
{ PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidName", "Invalid output attribute name " #_NAME ));\
Context->bWrite##_NAME = false; }

#define PCGEX_SET_OUT_ATTRIBUTE(_NAME, _KEY, _VALUE)\
if (Context->OutAttribute##_NAME) { Context->OutAttribute##_NAME->SetValue(_KEY, _VALUE); }

#define PCGEX_INIT_ATTRIBUTE_OUT(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::Common::TryGetAttribute<_TYPE>(IO->Out, Context->OutName##_NAME, Context->bWrite##_NAME);
#define PCGEX_INIT_ATTRIBUTE_IN(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::Common::TryGetAttribute<_TYPE>(IO->In, Context->OutName##_NAME, Context->bWrite##_NAME);

namespace PCGExMT
{
	enum EState : int
	{
		Setup UMETA(DisplayName = "Setup"),
		ReadyForNextPoints UMETA(DisplayName = "Ready for next points"),
		ReadyForPoints2ndPass UMETA(DisplayName = "Ready for next points - 2nd pass"),
		ReadyForNextGraph UMETA(DisplayName = "Ready for next params"),
		ReadyForGraph2ndPass UMETA(DisplayName = "Ready for next params - 2nd pass"),
		ProcessingPoints UMETA(DisplayName = "Processing points"),
		ProcessingPoints2ndPass UMETA(DisplayName = "Processing points - 2nd pass"),
		ProcessingGraph UMETA(DisplayName = "Processing params"),
		ProcessingGraph2ndPass UMETA(DisplayName = "Processing params - 2nd pass"),
		WaitingOnAsyncTasks UMETA(DisplayName = "Waiting on async tasks"),
		Done UMETA(DisplayName = "Done")
	};

	struct FTaskInfos
	{
		FTaskInfos()
		{
		}

		FTaskInfos(int32 InIndex, PCGMetadataEntryKey InKey, int32 InAttempt = 0):
			Index(InIndex), Key(InKey), Attempt(InAttempt)
		{
		}

		FTaskInfos(const FTaskInfos& Other):
			Index(Other.Index), Key(Other.Key), Attempt(Other.Attempt)
		{
		}

		FTaskInfos GetRetry() const { return FTaskInfos(Index, Key, Attempt + 1); }

		int32 Index = -1;
		PCGMetadataEntryKey Key = PCGInvalidEntryKey;
		int32 Attempt = 0;
	};
}

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

	virtual PCGEx::EIOInit GetPointOutputInitMode() const;

	/** Multithread chunk size, when supported.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	int32 ChunkSize = 0;

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

namespace PCGExAsync
{
	class FPointTask : public FNonAbandonableTask
	{
	public:
		virtual ~FPointTask() = default;

		FPointTask(
			FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, PCGExMT::FTaskInfos InInfos) :
			TaskContext(InContext), PointData(InPointData), Infos(InInfos)
		{
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPointTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork()
		{
			FPCGExPointsProcessorContext* InContext = TaskContext;
			if (InContext->SourceComponent.IsValid() && !InContext->SourceComponent.IsStale(true, true) && InContext->Points) { ExecuteTask(InContext); }
		};

		void PostDoWork() { delete this; }

		virtual void ExecuteTask(FPCGExPointsProcessorContext* InContext) = 0;

		FPCGExPointsProcessorContext* TaskContext;
		UPCGExPointIO* PointData;
		PCGExMT::FTaskInfos Infos;

		FPCGPoint GetInPoint() const { return PointData->In->GetPoint(Infos.Index); }
		FPCGPoint GetOutPoint() const { return PointData->Out->GetPoint(Infos.Index); }
	};
}
