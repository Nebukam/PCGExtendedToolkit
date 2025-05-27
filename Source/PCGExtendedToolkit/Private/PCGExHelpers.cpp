// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHelpers.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGExMacros.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"
#include "Async/Async.h"

namespace PCGEx
{
	void FIntTracker::IncrementPending(const int32 Count)
	{
		{
			FReadScopeLock ReadScopeLock(Lock);
			if (bTriggered) { return; }
		}
		{
			FWriteScopeLock WriteScopeLock(Lock);
			if (PendingCount == 0 && StartFn) { StartFn(); }
			PendingCount += Count;
		}
	}

	void FIntTracker::IncrementCompleted(const int32 Count)
	{
		{
			FReadScopeLock ReadScopeLock(Lock);
			if (bTriggered) { return; }
		}
		{
			FWriteScopeLock WriteScopeLock(Lock);
			CompletedCount += Count;
			if (CompletedCount == PendingCount) { TriggerInternal(); }
		}
	}

	void FIntTracker::Trigger()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		TriggerInternal();
	}

	void FIntTracker::SafetyTrigger()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		if (PendingCount > 0) { TriggerInternal(); }
	}

	void FIntTracker::Reset()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		PendingCount = CompletedCount = 0;
		bTriggered = false;
	}

	void FIntTracker::Reset(const int32 InMax)
	{
		FWriteScopeLock WriteScopeLock(Lock);
		PendingCount = InMax;
		CompletedCount = 0;
		bTriggered = false;
	}

	void FIntTracker::TriggerInternal()
	{
		if (bTriggered) { return; }
		bTriggered = true;
		ThresholdFn();
		PendingCount = CompletedCount = 0;
	}

	FName FUniqueNameGenerator::Get(const FString& BaseName)
	{
		FName OutName = FName(BaseName + "_" + FString::Printf(TEXT("%d"), Idx));
		FPlatformAtomics::InterlockedIncrement(&Idx);
		return OutName;
	}

	FName FUniqueNameGenerator::Get(const FName& BaseName)
	{
		return Get(BaseName.ToString());
	}

	FManagedObjects::FManagedObjects(FPCGContext* InContext)
		: WeakHandle(InContext->GetOrCreateHandle())
	{
	}

	FManagedObjects::~FManagedObjects()
	{
		Flush();
	}

	bool FManagedObjects::IsAvailable() const
	{
		FReadScopeLock WriteScopeLock(ManagedObjectLock);
		return WeakHandle.IsValid() && !IsFlushing();
	}

	void FManagedObjects::Flush()
	{
		bool bExpected = false;
		if (bIsFlushing.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			// Flush remaining managed objects & mark them as garbage
			for (UObject* ObjectPtr : ManagedObjects)
			{
				//if (!IsValid(ObjectPtr)) { continue; }
				///*FCOLLECTOR_IMPL*/ObjectPtr->RemoveFromRoot();
				RecursivelyClearAsyncFlag_Unsafe(ObjectPtr);

				if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(ObjectPtr)) { ManagedObject->Cleanup(); }

				ObjectPtr->MarkAsGarbage();
			}

			ManagedObjects.Empty();
			bIsFlushing.store(false, std::memory_order_release);
		}
	}

	void FManagedObjects::Add(UObject* InObject)
	{
		check(!IsFlushing())

		if (!IsValid(InObject)) { return; }

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Add(InObject);
			///*FCOLLECTOR_IMPL*/InObject->AddToRoot();
		}
	}

	bool FManagedObjects::Remove(UObject* InObject)
	{
		if (IsFlushing()) { return false; } // Will be removed anyway

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			if (!IsValid(InObject)) { return false; }
			int32 Removed = ManagedObjects.Remove(InObject);
			if (Removed == 0) { return false; }

			///*FCOLLECTOR_IMPL*/InObject->RemoveFromRoot();
			RecursivelyClearAsyncFlag_Unsafe(InObject);
		}

		if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(InObject)) { ManagedObject->Cleanup(); }

		return true;
	}

	void FManagedObjects::Remove(const TArray<FPCGTaggedData>& InTaggedData)
	{
		if (IsFlushing()) { return; } // Will be removed anyway

		TRACE_CPUPROFILER_EVENT_SCOPE(FManagedObjects::Remove);

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			for (const FPCGTaggedData& FData : InTaggedData)
			{
				if (UObject* InObject = const_cast<UPCGData*>(FData.Data.Get()); IsValid(InObject))
				{
					if (ManagedObjects.Remove(InObject) == 0) { continue; }

					///*FCOLLECTOR_IMPL*/InObject->RemoveFromRoot();
					RecursivelyClearAsyncFlag_Unsafe(InObject);
					if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(InObject)) { ManagedObject->Cleanup(); }
				}
			}
		}
	}

	void FManagedObjects::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
	{
		FReadScopeLock ReadScopeLock(ManagedObjectLock);
		for (TObjectPtr<UObject>& Object : ManagedObjects) { Collector.AddReferencedObject(Object); }
	}

	void FManagedObjects::Destroy(UObject* InObject)
	{
		check(InObject)

		if (IsFlushing()) { return; } // Will be destroyed anyway

		{
			FReadScopeLock ReadScopeLock(ManagedObjectLock);
			if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return; }
		}

		Remove(InObject);
		InObject->MarkAsGarbage();
	}

	void FManagedObjects::RecursivelyClearAsyncFlag_Unsafe(UObject* InObject) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FManagedObjects::RecursivelyClearAsyncFlag_Unsafe);

		{
			FReadScopeLock ReadScopeLock(DuplicatedObjectLock);
			if (DuplicateObjects.Contains(InObject)) { return; }
		}

		if (InObject->HasAnyInternalFlags(EInternalObjectFlags::Async))
		{
			InObject->ClearInternalFlags(EInternalObjectFlags::Async);

			ForEachObjectWithOuter(
				InObject, [&](UObject* SubObject)
				{
					if (ManagedObjects.Contains(SubObject)) { return; }
					SubObject->ClearInternalFlags(EInternalObjectFlags::Async);
				}, true);
		}
	}

	FRWScope::FRWScope(const int32 NumElements, const bool bSetNum)
	{
		if (bSetNum)
		{
			ReadIndices.Reserve(NumElements);
			WriteIndices.Reserve(NumElements);
		}
		else
		{
			ReadIndices.SetNumUninitialized(NumElements);
			WriteIndices.SetNumUninitialized(NumElements);
		}
	}

	int32 FRWScope::Add(const int32 ReadIndex, const int32 WriteIndex)
	{
		ReadIndices.Add(ReadIndex);
		return WriteIndices.Add(WriteIndex);
	}

	int32 FRWScope::Add(const TArrayView<int32> ReadIndicesRange, int32& OutWriteIndex)
	{
		for (const int32 ReadIndex : ReadIndicesRange) { Add(ReadIndex, OutWriteIndex++); }
		return ReadIndices.Num() - 1;
	}

	void FRWScope::Set(const int32 Index, const int32 ReadIndex, const int32 WriteIndex)
	{
		ReadIndices[Index] = ReadIndex;
		WriteIndices[Index] = WriteIndex;
	}

	void FRWScope::CopyPoints(const UPCGBasePointData* Read, UPCGBasePointData* Write, const bool bClean)
	{
		Read->CopyPointsTo(Write, ReadIndices, WriteIndices);
		if (bClean)
		{
			ReadIndices.Empty();
			WriteIndices.Empty();
		}
	}

	void FRWScope::CopyProperties(const UPCGBasePointData* Read, UPCGBasePointData* Write, EPCGPointNativeProperties Properties, const bool bClean)
	{
		Read->CopyPropertiesTo(Write, ReadIndices, WriteIndices, Properties);
		if (bClean)
		{
			ReadIndices.Empty();
			WriteIndices.Empty();
		}
	}

	int32 SetNumPointsAllocated(UPCGBasePointData* InData, const int32 InNumPoints, const EPCGPointNativeProperties Properties)
	{
		InData->SetNumPoints(InNumPoints);
		InData->AllocateProperties(Properties);
		return InNumPoints;
	}

	bool EnsureMinNumPoints(UPCGBasePointData* InData, const int32 InNumPoints)
	{
		if (InData->GetNumPoints() < InNumPoints)
		{
			InData->SetNumPoints(InNumPoints);
			return true;
		}

		return false;
	}

	void ReorderPointArrayData(UPCGBasePointData* InData, const TArray<int32>& InOrder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExHelpers::ReorderPointArrayData);

		const int32 NumElements = InOrder.Num();
		check(NumElements <= InData->GetNumPoints());

		TBitArray<> Visited;
		Visited.Init(false, NumElements);

#define PCGEX_REORDER_RANGE_DECL(_NAME, _TYPE, ...) TPCGValueRange<_TYPE> _NAME##Range = InData->Get##_NAME##ValueRange();
		PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_REORDER_RANGE_DECL)
#undef PCGEX_REORDER_RANGE_DECL

		for (int32 i = 0; i < NumElements; ++i)
		{
			if (Visited[i]) continue;

			int32 Current = i;
			int32 Next = InOrder[Current];

			if (Next == Current)
			{
				Visited[Current] = true;
				continue;
			}

#define PCGEX_REORDER_MOVE_TEMP(_NAME, _TYPE, ...) _TYPE Temp##_NAME = MoveTemp(_NAME##Range[Current]);
			PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_REORDER_MOVE_TEMP)
#undef PCGEX_REORDER_MOVE_TEMP

			while (!Visited[Next])
			{
#define PCGEX_REORDER_MOVE_FORWARD(_NAME, _TYPE, ...) _NAME##Range[Current] = MoveTemp(_NAME##Range[Next]);
				PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_REORDER_MOVE_FORWARD)
#undef PCGEX_REORDER_MOVE_FORWARD

				Visited[Current] = true;
				Current = Next;
				Next = InOrder[Current];
			}

#define PCGEX_REORDER_MOVE_BACK(_NAME, _TYPE, ...) _NAME##Range[Current] = MoveTemp(Temp##_NAME);
			PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_REORDER_MOVE_BACK)
#undef PCGEX_REORDER_MOVE_BACK

			Visited[Current] = true;
		}
	}
}

void UPCGExComponentCallback::Callback(UActorComponent* InComponent)
{
	if (!CallbackFn) { return; }
	if (bIsOnce)
	{
		auto Callback = MoveTemp(CallbackFn);
		CallbackFn = nullptr;
		Callback(InComponent);
	}
	else
	{
		CallbackFn(InComponent);
	}
}

void UPCGExComponentCallback::BeginDestroy()
{
	CallbackFn = nullptr;
	UObject::BeginDestroy();
}

void UPCGExPCGComponentCallback::Callback(UPCGComponent* InComponent)
{
	if (!CallbackFn) { return; }
	if (bIsOnce)
	{
		auto Callback = MoveTemp(CallbackFn);
		CallbackFn = nullptr;
		Callback(InComponent);
	}
	else
	{
		CallbackFn(InComponent);
	}
}

void UPCGExPCGComponentCallback::BeginDestroy()
{
	CallbackFn = nullptr;
	UObject::BeginDestroy();
}

namespace PCGExHelpers
{
	bool HasDataOnPin(FPCGContext* InContext, const FName Pin)
	{
		for (const FPCGTaggedData& TaggedData : InContext->InputData.TaggedData) { if (TaggedData.Pin == Pin) { return true; } }
		return false;
	}

	bool TryGetAttributeName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, FName& OutName)
	{
		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid()) { return false; }
		if (FixedSelector.GetSelection() != EPCGAttributePropertySelection::Attribute) { return false; }
		OutName = FixedSelector.GetName();
		return true;
	}

	void LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths)
	{
		if (IsInGameThread())
		{
			UAssetManager::GetStreamableManager().RequestSyncLoad(Paths->Array());
		}
		else
		{
			TWeakPtr<TSet<FSoftObjectPath>> WeakPaths = Paths;
			FEvent* BlockingEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(
				ENamedThreads::GameThread, [BlockingEvent, WeakPaths]()
				{
					const TSharedPtr<TSet<FSoftObjectPath>> ToBeLoaded = WeakPaths.Pin();
					if (!ToBeLoaded)
					{
						BlockingEvent->Trigger();
						return;
					}

					const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						ToBeLoaded->Array(), [BlockingEvent]() { BlockingEvent->Trigger(); });

					if (!Handle || !Handle->IsActive()) { BlockingEvent->Trigger(); }
				});

			BlockingEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(BlockingEvent);
		}
	}

	void CopyStructProperties(const void* SourceStruct, void* TargetStruct, const UStruct* SourceStructType, const UStruct* TargetStructType)
	{
		for (TFieldIterator<FProperty> SourceIt(SourceStructType); SourceIt; ++SourceIt)
		{
			const FProperty* SourceProperty = *SourceIt;
			const FProperty* TargetProperty = TargetStructType->FindPropertyByName(SourceProperty->GetFName());
			if (!TargetProperty || SourceProperty->GetClass() != TargetProperty->GetClass()) { continue; }

			if (SourceProperty->SameType(TargetProperty))
			{
				const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(SourceStruct);
				void* TargetValue = TargetProperty->ContainerPtrToValuePtr<void>(TargetStruct);

				SourceProperty->CopyCompleteValue(TargetValue, SourceValue);
			}
		}
	}

	bool CopyProperties(UObject* Target, const UObject* Source, const TSet<FString>* Exclusions)
	{
		const UClass* SourceClass = Source->GetClass();
		const UClass* TargetClass = Target->GetClass();
		const UClass* CommonBaseClass = nullptr;

		if (SourceClass->IsChildOf(TargetClass)) { CommonBaseClass = TargetClass; }
		else if (TargetClass->IsChildOf(SourceClass)) { CommonBaseClass = SourceClass; }
		else
		{
			// Traverse up the hierarchy to find a shared base class
			const UClass* TempClass = SourceClass;
			while (TempClass)
			{
				if (TargetClass->IsChildOf(TempClass))
				{
					CommonBaseClass = TempClass;
					break;
				}
				TempClass = TempClass->GetSuperClass();
			}
		}

		if (!CommonBaseClass) { return false; }

		// Iterate over source properties
		for (TFieldIterator<FProperty> It(CommonBaseClass); It; ++It)
		{
			const FProperty* Property = *It;
			if (Exclusions && Exclusions->Contains(Property->GetName())) { continue; }

			// Skip properties that shouldn't be copied (like transient properties)
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_ConstParm | CPF_OutParm)) { continue; }

			// Copy the value from source to target
			const void* SourceValue = Property->ContainerPtrToValuePtr<void>(Source);
			void* TargetValue = Property->ContainerPtrToValuePtr<void>(Target);
			Property->CopyCompleteValue(TargetValue, SourceValue);
		}

		return true;
	}

	TArray<FString> GetStringArrayFromCommaSeparatedList(const FString& InCommaSeparatedString)
	{
		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArray(Result, TEXT(","));
		// Trim leading and trailing spaces
		for (int i = 0; i < Result.Num(); i++)
		{
			FString& String = Result[i];
			String.TrimStartAndEndInline();
			if (String.IsEmpty())
			{
				Result.RemoveAt(i);
				i--;
			}
		}

		return Result;
	}

	void AppendUniqueEntriesFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FString>& OutStrings)
	{
		if (InCommaSeparatedString.IsEmpty()) { return; }

		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArray(Result, TEXT(","));
		// Trim leading and trailing spaces
		for (int i = 0; i < Result.Num(); i++)
		{
			FString& String = Result[i];
			String.TrimStartAndEndInline();
			if (String.IsEmpty()) { continue; }

			OutStrings.AddUnique(String);
		}
	}

	void AppendUniqueSelectorsFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FPCGAttributePropertyInputSelector>& OutSelectors)
	{
		if (InCommaSeparatedString.IsEmpty()) { return; }

		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArray(Result, TEXT(","));
		// Trim leading and trailing spaces
		for (int i = 0; i < Result.Num(); i++)
		{
			FString& String = Result[i];
			String.TrimStartAndEndInline();
			if (String.IsEmpty()) { continue; }

			FPCGAttributePropertyInputSelector Selector;
			Selector.Update(String);

			OutSelectors.AddUnique(Selector);
		}
	}

	TArray<UFunction*> FindUserFunctions(const TSubclassOf<AActor>& ActorClass, const TArray<FName>& FunctionNames, const TArray<const UFunction*>& FunctionPrototypes, const FPCGContext* InContext)
	{
		TArray<UFunction*> Functions;

		if (!ActorClass)
		{
			return Functions;
		}

		for (FName FunctionName : FunctionNames)
		{
			if (FunctionName == NAME_None)
			{
				continue;
			}

			if (UFunction* Function = ActorClass->FindFunctionByName(FunctionName))
			{
#if WITH_EDITOR
				if (!Function->GetBoolMetaData(TEXT("CallInEditor")))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT( "Function '{0}' in class '{1}' requires CallInEditor to be true while in-editor."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
					continue;
				}
#endif
				for (const UFunction* Prototype : FunctionPrototypes)
				{
					if (Function->IsSignatureCompatibleWith(Prototype))
					{
						Functions.Add(Function);
						break;
					}
				}

				if (Functions.IsEmpty() || Functions.Last() != Function)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' in class '{1}' has incorrect parameters."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
				}
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' was not found in class '{1}'."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
			}
		}

		return Functions;
	}
}
