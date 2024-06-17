// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExMT.h"
#include "PCGExMacros.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "PCGExOperation.h"
#include "PCGExPointsMT.h"

#include "PCGExPointsProcessor.generated.h"

class UPCGExFilterFactoryBase;

namespace PCGExDataFilter
{
	class TEarlyExitFilterManager;
}

struct FPCGExPointsProcessorContext;
struct FPCGExSubProcessor;

#pragma region Loops

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FAPointLoop
	{
		virtual ~FAPointLoop() = default;

		FAPointLoop()
		{
		}

		FPCGExPointsProcessorContext* Context = nullptr;

		PCGExData::FPointIO* PointIO = nullptr;

		int32 NumIterations = -1;
		int32 ChunkSize = 32;

		int32 CurrentIndex = -1;
		bool bAsyncEnabled = true;

		inline PCGExData::FPointIO& GetPointIO() const;

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) = 0;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) = 0;

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointLoop : public FAPointLoop
	{
		FPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FAsyncPointLoop : public FPointLoop
	{
		FAsyncPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};
}

#pragma endregion

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExPointsProcessorContext;
	friend class FPCGExPointsProcessorElementBase;

public:
	UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const override;

	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainInputLabel() const;
	virtual FName GetMainOutputLabel() const;
	virtual bool GetMainAcceptMultipleData() const;
	virtual PCGExData::EInit GetMainOutputInitMode() const;

	virtual FName GetPointFilterLabel() const;
	bool SupportsPointFilters() const;
	virtual bool RequiresPointFilters() const;

	/** Forces execution on main thread. Work is still chunked. Turning this off ensure linear order of operations, and, in most case, determinism.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bDoAsyncProcessing = true;

	/** Chunk size for parallel processing. <1 switches to preferred node value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(ClampMin=-1, ClampMax=8196))
	int32 ChunkSize = -1;

	/** Cache the results of this node. Can yield unexpected result in certain cases.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bCacheResult = false;

	/** Flatten the output of this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bFlattenOutput = false;


	template <typename T>
	static T* EnsureOperation(UPCGExOperation* Operation) { return Operation ? static_cast<T*>(Operation) : NewObject<T>(); }

protected:
	virtual int32 GetPreferredChunkSize() const;
	//~End UPCGExPointsProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGExContext
{
	friend class FPCGExPointsProcessorElementBase;
	friend struct FPCGExSubProcessor;

	virtual ~FPCGExPointsProcessorContext() override;

	UWorld* World = nullptr;

	mutable FRWLock ContextLock;
	mutable FRWLock StateLock;

	PCGExData::FPointIOCollection* MainPoints = nullptr;
	PCGExData::FPointIO* CurrentIO = nullptr;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true);
	virtual bool ExecuteAutomation();

	bool IsState(const PCGExMT::AsyncState OperationId) const
	{
		FReadScopeLock ReadScopeLock(StateLock);
		return CurrentState == OperationId;
	}

	bool IsSetup() const { return IsState(PCGExMT::State_Setup); }
	bool IsDone() const { return IsState(PCGExMT::State_Done); }
	virtual void Done();
	virtual void PostProcessOutputs();

	FPCGExAsyncManager* GetAsyncManager();

	void SetAsyncState(const PCGExMT::AsyncState WaitState) { SetState(WaitState, false); }
	void SetState(PCGExMT::AsyncState OperationId, bool bResetAsyncWork = true);

	int32 ChunkSize = 0;
	bool bDoAsyncProcessing = true;

	void OutputMainPoints() { MainPoints->OutputTo(this); }

#pragma region Async loops

	bool ProcessCurrentPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);
	bool ProcessCurrentPoints(TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);

	template <class InitializeFunc, class LoopBodyFunc>
	bool Process(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Execute(Initialize, LoopBody);
	}

	template <class LoopBodyFunc>
	bool Process(LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Execute(LoopBody);
	}

	template <typename FullTask>
	void StartAsyncLoop(PCGExData::FPointIO* PointIO, const int32 NumIterations, const int32 ChunkSizeOverride = -1)
	{
		GetAsyncManager()->Start<FullTask>(-1, PointIO, NumIterations, ChunkSizeOverride <= 0 ? ChunkSize : ChunkSizeOverride);
	}

	PCGExData::FPointIO* TryGetSingleInput(FName InputName, const bool bThrowError) const;

	template <typename T>
	T MakeLoop()
	{
		T Loop = T{};
		Loop.Context = this;
		Loop.ChunkSize = ChunkSize;
		Loop.bAsyncEnabled = bDoAsyncProcessing;
		return Loop;
	}

#pragma endregion

	template <typename T>
	T* RegisterOperation(UPCGExOperation* Operation = nullptr)
	{
		T* RetValue;
		if (!Operation)
		{
			RetValue = NewObject<T>();
			OwnedProcessorOperations.Add(RetValue);
		}
		else
		{
			RetValue = static_cast<T*>(Operation);
			PCGEX_SETTINGS_LOCAL(PointsProcessor)
		}
		RetValue->BindContext(this);
		return RetValue;
	}

#pragma region Filtering

	TArray<UPCGExFilterFactoryBase*> FilterFactories;

#pragma endregion

#pragma region Batching

	bool ProcessPointsBatch();

	PCGExMT::AsyncState State_PointsProcessingDone;
	PCGExPointsMT::FPointsProcessorBatchBase* MainBatch = nullptr;
	TArray<PCGExData::FPointIO*> BatchablePoints;

	template <typename T, class ValidateEntryFunc, class InitBatchFunc>
	bool StartBatchProcessingPoints(ValidateEntryFunc&& ValidateEntry, InitBatchFunc&& InitBatch, const PCGExMT::AsyncState InState)
	{
		State_PointsProcessingDone = InState;
		BatchablePoints.Empty();

		while (AdvancePointsIO(false))
		{
			if (!ValidateEntry(CurrentIO)) { continue; }
			BatchablePoints.Add(CurrentIO);
		}

		if (BatchablePoints.IsEmpty()) { return false; }

		MainBatch = new T(this, BatchablePoints);
		InitBatch(static_cast<T*>(MainBatch));

		PCGExPointsMT::ScheduleBatch(GetAsyncManager(), MainBatch);
		SetAsyncState(PCGExPointsMT::State_WaitingOnPointsProcessing);
		return true;
	}

#pragma endregion

protected:
	FPCGExSubProcessor* CurrentSubProcessor = nullptr;

	PCGExMT::FAsyncParallelLoop AsyncLoop;
	FPCGExAsyncManager* AsyncManager = nullptr;

	PCGEx::FPointLoop ChunkedPointLoop;
	PCGEx::FAsyncPointLoop AsyncPointLoop;

	PCGExMT::AsyncState CurrentState;
	int32 CurrentPointIOIndex = -1;

	TArray<UPCGExOperation*> ProcessorOperations;
	TSet<UPCGExOperation*> OwnedProcessorOperations;

	virtual void ResetAsyncWork();

public:
	virtual bool IsAsyncWorkComplete();
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override
	{
		const UPCGExPointsProcessorSettings* Settings = static_cast<const UPCGExPointsProcessorSettings*>(InSettings);
		return Settings->bCacheResult;
	}

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual FPCGContext* InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	virtual bool Boot(FPCGContext* InContext) const;
};
