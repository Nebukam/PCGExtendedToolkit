// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExEditorSettings.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "PCGExOperation.h"
#include "Helpers/PCGGraphParametersHelpers.h"

#include "PCGExPointsProcessor.generated.h"

#define PCGEX_NODE_INFOS(_SHORTNAME, _NAME, _TOOLTIP)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return bCacheResult ? FName(FString("* ")+GetDefaultNodeTitle().ToString()) : FName(GetDefaultNodeTitle().ToString()); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return _TASK_NAME.IsNone() ? FName(GetDefaultNodeTitle().ToString()) : FName(FString(GetDefaultNodeTitle().ToString() + "\r" + _TASK_NAME.ToString())); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }
#else
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return FName(GetDefaultNodeTitle().ToString()); }\
virtual FString GetAdditionalTitleInformation() const override{ return _TASK_NAME.IsNone() ? FString() : _TASK_NAME.ToString(); }\
virtual bool HasFlippedTitleLines() const { return !_TASK_NAME.IsNone(); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }
#endif

#define PCGEX_INITIALIZE_CONTEXT(_NAME)\
FPCGContext* FPCGEx##_NAME##Element::Initialize( const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)\
{	FPCGEx##_NAME##Context* Context = new FPCGEx##_NAME##Context();	return InitializeContext(Context, InputData, SourceComponent, Node); }
#define PCGEX_INITIALIZE_ELEMENT(_NAME)\
PCGEX_INITIALIZE_CONTEXT(_NAME)\
FPCGElementPtr UPCGEx##_NAME##Settings::CreateElement() const{	return MakeShared<FPCGEx##_NAME##Element>();}
#define PCGEX_CONTEXT(_NAME) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(InContext);
#define PCGEX_SETTINGS(_NAME) const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_SETTINGS_LOCAL(_NAME) const UPCGEx##_NAME##Settings* Settings = GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_CONTEXT_AND_SETTINGS(_NAME) PCGEX_CONTEXT(_NAME) PCGEX_SETTINGS(_NAME)
#define PCGEX_OPERATION_DEFAULT(_NAME, _TYPE)  // if(!_NAME){_NAME = NewObject<_TYPE>(this, TEXT(#_NAME), RF_Transactional); _NAME->UpdateUserFacingInfos();} //ObjectInitializer.CreateDefaultSubobject<_TYPE>(this, TEXT(#_NAME));
#define PCGEX_OPERATION_VALIDATE(_NAME) if(!Settings->_NAME){PCGE_LOG(Error, GraphAndLog, FTEXT("No operation selected for : "#_NAME)); return false;}
#define PCGEX_OPERATION_BIND(_NAME, _TYPE) PCGEX_OPERATION_VALIDATE(_NAME) Context->_NAME = Context->RegisterOperation<_TYPE>(Settings->_NAME);
#define PCGEX_VALIDATE_NAME(_NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_SOFT_VALIDATE_NAME(_BOOL, _NAME, _CTX) if(_BOOL){if (!PCGEx::IsValidName(_NAME)){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_FWD(_NAME) Context->_NAME = Settings->_NAME;
#define PCGEX_TERMINATE_ASYNC PCGEX_DELETE(AsyncManager)

#define PCGEX_WAIT_ASYNC if (!Context->IsAsyncWorkComplete()) {return false;}

#if WITH_EDITOR
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) Pin.Tooltip = FTEXT(_TOOLTIP);
#else
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) {}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#define PCGEX_PIN_STATUS(_STATUS) Pin.PinStatus = EPCGPinStatus::_STATUS;
#else
#define PCGEX_PIN_STATUS(_STATUS) Pin.bAdvancedPin = EPCGPinStatus::_STATUS == EPCGPinStatus::Advanced;
#endif

#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::PolyLine); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }

struct FPCGExPointsProcessorContext;

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

	struct PCGEXTENDEDTOOLKIT_API FBulkPointLoop : public FPointLoop
	{
		FBulkPointLoop()
		{
		}

		TArray<FPointLoop> SubLoops;

		virtual void Init();
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

	struct PCGEXTENDEDTOOLKIT_API FBulkAsyncPointLoop : public FAsyncPointLoop
	{
		FBulkAsyncPointLoop()
		{
		}

		TArray<FAsyncPointLoop> SubLoops;

		virtual void Init();
		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};
}


/**
 * A Base node to process a set of point using GraphParams.
 */
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

	/** Forces execution on main thread. Work is still chunked. Turning this off ensure linear order of operations, and, in most case, determinism.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bDoAsyncProcessing = true;

	/** Chunk size for parallel processing. <1 switches to preferred node value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(ClampMin=-1, ClampMax=8196))
	int32 ChunkSize = -1;

	/** Cache the results of this node. Can yield unexpected result in certain cases.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bCacheResult = false;

	template <typename T>
	static T* EnsureOperation(UPCGExOperation* Operation) { return Operation ? static_cast<T*>(Operation) : NewObject<T>(); }

protected:
	virtual int32 GetPreferredChunkSize() const;
	//~End UPCGExPointsProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;

	virtual ~FPCGExPointsProcessorContext() override;

	UWorld* World = nullptr;

	mutable FRWLock ContextLock;
	PCGExData::FPointIOCollection* MainPoints = nullptr;

	PCGExData::FPointIO* CurrentIO = nullptr;

	const UPCGPointData* GetCurrentIn() const { return CurrentIO->GetIn(); }
	UPCGPointData* GetCurrentOut() const { return CurrentIO->GetOut(); }

	virtual bool AdvancePointsIO();
	PCGExMT::AsyncState GetState() const { return CurrentState; }
	bool IsState(const PCGExMT::AsyncState OperationId) const { return CurrentState == OperationId; }
	bool IsSetup() const { return IsState(PCGExMT::State_Setup); }
	bool IsDone() const { return IsState(PCGExMT::State_Done); }
	virtual void Done();

	FPCGExAsyncManager* GetAsyncManager();
	void SetAsyncState(const PCGExMT::AsyncState WaitState) { SetState(WaitState, false); }

	virtual void SetState(PCGExMT::AsyncState OperationId, bool bResetAsyncWork = true);
	virtual void Reset();

	int32 ChunkSize = 0;
	bool bDoAsyncProcessing = true;

	void OutputPoints(const bool bFlatten = false) { MainPoints->OutputTo(this); }

	bool BulkProcessMainPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody);
	bool ProcessCurrentPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);
	bool ProcessCurrentPoints(TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);

	template <class InitializeFunc, class LoopBodyFunc>
	bool Process(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Advance(Initialize, LoopBody);
	}

	template <class LoopBodyFunc>
	bool Process(LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Advance(LoopBody);
	}

	template <class LoopBodyFunc>
	void ParallelProcess(LoopBodyFunc&& LoopBody, const int32 NumIterations)
	{
		ParallelProcess(
			[&]()
			{
			}, LoopBody, NumIterations);
	}

	template <class InitializeFunc, class LoopBodyFunc>
	void ParallelProcess(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, const int32 NumIterations)
	{
		GetAsyncManager()->Start<FPCGExParallelLoopTask>(-1, nullptr, Initialize, LoopBody, NumIterations, ChunkSize);
	}

	FPCGTaggedData* Output(UPCGData* OutData, const FName OutputLabel);

	template <typename T>
	T MakeLoop()
	{
		T Loop = T{};
		Loop.Context = this;
		Loop.ChunkSize = ChunkSize;
		Loop.bAsyncEnabled = bDoAsyncProcessing;
		return Loop;
	}

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
			//Settings->
			//UPCGGraphParametersHelpers::GetInt32Parameter(Con)
		}
		RetValue->BindContext(this);
		return RetValue;
	}

protected:
	PCGExMT::FAsyncParallelLoop AsyncLoop;
	FPCGExAsyncManager* AsyncManager = nullptr;

	PCGEx::FPointLoop ChunkedPointLoop;
	PCGEx::FAsyncPointLoop AsyncPointLoop;
	PCGEx::FBulkAsyncPointLoop BulkAsyncPointLoop;

	PCGExMT::AsyncState CurrentState;
	int32 CurrentPointIOIndex = -1;

	TArray<UPCGExOperation*> ProcessorOperations;
	TSet<UPCGExOperation*> OwnedProcessorOperations;

	void CleanupOperations();
	virtual void ResetAsyncWork();

public:
	virtual bool IsAsyncWorkComplete();
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
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
