// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHelpers.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "PCGManagedResource.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "Details/PCGExMacros.h"
#include "PCGModule.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"
#include "Async/Async.h"
#include "Data/PCGPointArrayData.h"

namespace PCGEx
{
	EPCGPointNativeProperties GetPointNativeProperties(uint8 Flags)
	{
		const EPCGExPointNativeProperties InFlags = static_cast<EPCGExPointNativeProperties>(Flags);
		EPCGPointNativeProperties OutFlags = EPCGPointNativeProperties::None;

		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::Transform)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::Transform); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::Density)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::Density); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::BoundsMin)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::BoundsMin); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::BoundsMax)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::BoundsMax); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::Color)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::Color); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::Steepness)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::Steepness); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::Seed)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::Seed); }
		if (EnumHasAnyFlags(InFlags, EPCGExPointNativeProperties::MetadataEntry)) { EnumAddFlags(OutFlags, EPCGPointNativeProperties::MetadataEntry); }

		return OutFlags;
	}

	FName GetLongNameFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized)
	{
		// This return a domain-less unique identifier for the provided selector
		// It's mostly used to create uniquely identified value buffers

		if (!InData) { return InvalidName; }

		if (bInitialized)
		{
			if (InSelector.GetExtraNames().IsEmpty()) { return FName(InSelector.GetName().ToString()); }
			return FName(InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT(".")));
		}
		if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute && InSelector.GetName() == "@Last")
		{
			return GetLongNameFromSelector(InSelector.CopyAndFixLast(InData), InData, true);
		}

		return GetLongNameFromSelector(InSelector, InData, true);
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized)
	{
		// This return an identifier suitable to be used for data facade

		FPCGAttributeIdentifier Identifier;

		if (!InData) { return FPCGAttributeIdentifier(InvalidName, EPCGMetadataDomainFlag::Invalid); }

		if (bInitialized)
		{
			Identifier.Name = InSelector.GetAttributeName();
			Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(InSelector);
		}
		else
		{
			FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);

			check(FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)

			Identifier.Name = FixedSelector.GetAttributeName();
			Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(FixedSelector);
		}


		return Identifier;
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName, const UPCGData* InData)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		Selector = Selector.CopyAndFixLast(InData);
		return GetAttributeIdentifier(Selector, InData);
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName)
	{
		FString StrName = InName.ToString();
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return FPCGAttributeIdentifier(Selector.GetAttributeName(), StrName.StartsWith(TEXT("@Data.")) ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Elements);
	}

	FPCGAttributePropertyInputSelector GetSelectorFromIdentifier(const FPCGAttributeIdentifier& InIdentifier)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.SetAttributeName(InIdentifier.Name);
		Selector.SetDomainName(InIdentifier.MetadataDomain.DebugName);
		return Selector;
	}

	FPCGExAsyncStateScope::FPCGExAsyncStateScope(FPCGContext* InContext, const bool bDesired)
		: Context(InContext)
	{
		if (Context)
		{
			// Ensure PCG AsyncState is up to date
			bRestoreTo = Context->AsyncState.bIsRunningOnMainThread;
			Context->AsyncState.bIsRunningOnMainThread = bDesired;
		}
	}

	FPCGExAsyncStateScope::~FPCGExAsyncStateScope()
	{
		if (Context)
		{
			Context->AsyncState.bIsRunningOnMainThread = bRestoreTo;
		}
	}

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
				/*FCOLLECTOR_IMPL*/
				ObjectPtr->RemoveFromRoot();
				RecursivelyClearAsyncFlag_Unsafe(ObjectPtr);

				if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(ObjectPtr)) { ManagedObject->Cleanup(); }
			}

			ManagedObjects.Empty();
			bIsFlushing.store(false, std::memory_order_release);
		}
	}

	bool FManagedObjects::Add(UObject* InObject)
	{
		check(!IsFlushing())

		if (!IsValid(InObject)) { return false; }

		bool bIsAlreadyInSet = false;
		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Add(InObject, &bIsAlreadyInSet);
			/*FCOLLECTOR_IMPL*/
			InObject->AddToRoot();
		}

		return !bIsAlreadyInSet;
	}

	bool FManagedObjects::Remove(UObject* InObject)
	{
		if (IsFlushing()) { return false; } // Will be removed anyway

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			if (!IsValid(InObject)) { return false; }
			int32 Removed = ManagedObjects.Remove(InObject);
			if (Removed == 0) { return false; }

			/*FCOLLECTOR_IMPL*/
			InObject->RemoveFromRoot();
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

					/*FCOLLECTOR_IMPL*/
					InObject->RemoveFromRoot();
					RecursivelyClearAsyncFlag_Unsafe(InObject);
					if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(InObject)) { ManagedObject->Cleanup(); }
				}
			}
		}
	}

	void FManagedObjects::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
	{
		//FReadScopeLock ReadScopeLock(ManagedObjectLock);
		//for (TObjectPtr<UObject>& Object : ManagedObjects) { Collector.AddReferencedObject(Object); }
	}

	void FManagedObjects::Destroy(UObject* InObject)
	{
		// ♫ Let it go ♫

		/*
		check(InObject)

		if (IsFlushing()) { return; } // Will be destroyed anyway

		{
			FReadScopeLock ReadScopeLock(ManagedObjectLock);
			if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return; }
		}

		Remove(InObject);
		*/
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

	FReadWriteScope::FReadWriteScope(const int32 NumElements, const bool bSetNum)
	{
		if (bSetNum)
		{
			ReadIndices.SetNumUninitialized(NumElements);
			WriteIndices.SetNumUninitialized(NumElements);
		}
		else
		{
			ReadIndices.Reserve(NumElements);
			WriteIndices.Reserve(NumElements);
		}
	}

	int32 FReadWriteScope::Add(const int32 ReadIndex, const int32 WriteIndex)
	{
		ReadIndices.Add(ReadIndex);
		return WriteIndices.Add(WriteIndex);
	}

	int32 FReadWriteScope::Add(const TArrayView<int32> ReadIndicesRange, int32& OutWriteIndex)
	{
		for (const int32 ReadIndex : ReadIndicesRange) { Add(ReadIndex, OutWriteIndex++); }
		return ReadIndices.Num() - 1;
	}

	void FReadWriteScope::Set(const int32 Index, const int32 ReadIndex, const int32 WriteIndex)
	{
		ReadIndices[Index] = ReadIndex;
		WriteIndices[Index] = WriteIndex;
	}

	void FReadWriteScope::CopyPoints(const UPCGBasePointData* Read, UPCGBasePointData* Write, const bool bClean, const bool bInitializeMetadata)
	{
		if (bInitializeMetadata)
		{
			EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All;
			EnumRemoveFlags(Properties, EPCGPointNativeProperties::MetadataEntry);

			Read->CopyPropertiesTo(Write, ReadIndices, WriteIndices, Properties);

			TPCGValueRange<int64> OutMetadataEntries = Write->GetMetadataEntryValueRange();
			for (const int32 WriteIndex : WriteIndices) { Write->Metadata->InitializeOnSet(OutMetadataEntries[WriteIndex]); }
		}
		else
		{
			Read->CopyPointsTo(Write, ReadIndices, WriteIndices);
		}

		if (bClean)
		{
			ReadIndices.Empty();
			WriteIndices.Empty();
		}
	}

	void FReadWriteScope::CopyProperties(const UPCGBasePointData* Read, UPCGBasePointData* Write, EPCGPointNativeProperties Properties, const bool bClean)
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

	template <typename T>
	void ReorderValueRange(TPCGValueRange<T>& InRange, const TArray<int32>& InOrder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExHelpers::ReorderValueRange);

		const int32 NumIndices = InOrder.Num();
		TArray<T> ValuesCopy;
		PCGEx::InitArray(ValuesCopy, NumIndices);
		if (NumIndices < 4096)
		{
			for (int i = 0; i < NumIndices; i++) { ValuesCopy[i] = MoveTemp(InRange[InOrder[i]]); }
			for (int i = 0; i < NumIndices; i++) { InRange[i] = MoveTemp(ValuesCopy[i]); }
		}
		else
		{
			ParallelFor(NumIndices, [&](int32 i) { ValuesCopy[i] = MoveTemp(InRange[InOrder[i]]); });
			ParallelFor(NumIndices, [&](int32 i) { InRange[i] = MoveTemp(ValuesCopy[i]); });
		}
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API void ReorderValueRange<_TYPE>(TPCGValueRange<_TYPE>& InRange, const TArray<int32>& InOrder);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	void ReorderPointArrayData(UPCGBasePointData* InData, const TArray<int32>& InOrder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExHelpers::ReorderPointArrayData);

		EPCGPointNativeProperties AllocatedProperties = InData->GetAllocatedProperties();

#define PCGEX_REORDER_RANGE_DECL(_NAME, _TYPE, ...) \
	if(EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::_NAME)){ \
		TPCGValueRange<_TYPE> Range = InData->Get##_NAME##ValueRange(true); \
		ReorderValueRange<_TYPE>(Range, InOrder);}

		PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_REORDER_RANGE_DECL)
#undef PCGEX_REORDER_RANGE_DECL
	}

	FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector)
	{
		if (InSelector.GetExtraNames().IsEmpty()) { return InSelector.GetName().ToString(); }
		return InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT("."));
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

namespace PCGExHelpers
{
	FText GetClassDisplayName(const UClass* InClass)
	{
#if WITH_EDITOR
		return InClass->GetDisplayNameText();
#else
		return FText::FromString(InClass->GetName());
#endif
	}

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

	bool IsDataDomainAttribute(const FName& InName)
	{
		return InName.ToString().TrimStartAndEnd().StartsWith("@Data.");
	}

	bool IsDataDomainAttribute(const FString& InName)
	{
		return InName.TrimStartAndEnd().StartsWith("@Data.");
	}

	bool IsDataDomainAttribute(const FPCGAttributePropertyInputSelector& InputSelector)
	{
		return InputSelector.GetDomainName() == PCGDataConstants::DataDomainName ||
			IsDataDomainAttribute(InputSelector.GetName());
	}

	void InitEmptyNativeProperties(const UPCGData* From, UPCGData* To, EPCGPointNativeProperties Properties)
	{
		const UPCGPointArrayData* FromPoints = Cast<UPCGPointArrayData>(From);
		UPCGPointArrayData* ToPoints = Cast<UPCGPointArrayData>(To);

		if (!FromPoints || !ToPoints || FromPoints == ToPoints) { return; }

		ToPoints->CopyUnallocatedPropertiesFrom(FromPoints);
		ToPoints->AllocateProperties(FromPoints->GetAllocatedProperties());
	}

	void LoadBlocking_AnyThread(const FSoftObjectPath& Path)
	{
		if (IsInGameThread())
		{
			// We're in the game thread, request synchronous load
			UAssetManager::GetStreamableManager().RequestSyncLoad(Path);
		}
		else
		{
			// We're not in the game thread, we need to dispatch loading to the main thread
			// and wait in the current one
			FEvent* BlockingEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(
				ENamedThreads::GameThread, [BlockingEvent, Path]()
				{
					const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						Path, [BlockingEvent]() { BlockingEvent->Trigger(); });

					if (!Handle || !Handle->IsActive()) { BlockingEvent->Trigger(); }
				});

			BlockingEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(BlockingEvent);
		}
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
