// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UObject/UObjectGlobals.h"

#include "PCGContext.h"
#include "Details/PCGExMacros.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Async/Async.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "PCGExHelpers.generated.h"

class UPCGManagedComponent;
class UPCGData;
class UPCGComponent;

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Native Point Properties"))
enum class EPCGExPointNativeProperties : uint8
{
	None          = 0,
	Transform     = 1 << 0 UMETA(DisplayName = "Transform"),
	Density       = 1 << 1 UMETA(DisplayName = "Density"),
	BoundsMin     = 1 << 2 UMETA(DisplayName = "BoundsMin"),
	BoundsMax     = 1 << 3 UMETA(DisplayName = "BoundsMax"),
	Color         = 1 << 4 UMETA(DisplayName = "Color"),
	Steepness     = 1 << 5 UMETA(DisplayName = "Steepness"),
	Seed          = 1 << 6 UMETA(DisplayName = "Seed"),
	MetadataEntry = 1 << 7 UMETA(DisplayName = "MetadataEntry"),
};

ENUM_CLASS_FLAGS(EPCGExPointNativeProperties)
using EPCGExNativePointPropertiesBitmask = TEnumAsByte<EPCGExPointNativeProperties>;

UINTERFACE()
class PCGEXTENDEDTOOLKIT_API UPCGExManagedObjectInterface : public UInterface
{
	GENERATED_BODY()
};

class PCGEXTENDEDTOOLKIT_API IPCGExManagedObjectInterface
{
	GENERATED_BODY()

public:
	virtual void Cleanup() = 0;
};

UINTERFACE()
class PCGEXTENDEDTOOLKIT_API UPCGExManagedComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class PCGEXTENDEDTOOLKIT_API IPCGExManagedComponentInterface
{
	GENERATED_BODY()

public:
	virtual void SetManagedComponent(UPCGManagedComponent* InManagedComponent) = 0;
	virtual UPCGManagedComponent* GetManagedComponent() = 0;
};

namespace PCGExHelpers
{
	PCGEXTENDEDTOOLKIT_API FText GetClassDisplayName(const UClass* InClass);

	PCGEXTENDEDTOOLKIT_API bool TryGetAttributeName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, FName& OutName);

	PCGEXTENDEDTOOLKIT_API bool IsDataDomainAttribute(const FName& InName);

	PCGEXTENDEDTOOLKIT_API bool IsDataDomainAttribute(const FString& InName);

	PCGEXTENDEDTOOLKIT_API bool IsDataDomainAttribute(const FPCGAttributePropertyInputSelector& InputSelector);

	PCGEXTENDEDTOOLKIT_API void InitEmptyNativeProperties(const UPCGData* From, UPCGData* To, EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All);

	PCGEXTENDEDTOOLKIT_API void CopyStructProperties(const void* SourceStruct, void* TargetStruct, const UStruct* SourceStructType, const UStruct* TargetStructType);

	PCGEXTENDEDTOOLKIT_API bool CopyProperties(UObject* Target, const UObject* Source, const TSet<FString>* Exclusions = nullptr);

	PCGEXTENDEDTOOLKIT_API TArray<FString> GetStringArrayFromCommaSeparatedList(const FString& InCommaSeparatedString);

	PCGEXTENDEDTOOLKIT_API void AppendEntriesFromCommaSeparatedList(const FString& InCommaSeparatedString, TSet<FString>& OutStrings);
	
	PCGEXTENDEDTOOLKIT_API void AppendUniqueEntriesFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FString>& OutStrings);

	PCGEXTENDEDTOOLKIT_API void AppendUniqueSelectorsFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FPCGAttributePropertyInputSelector>& OutSelectors);

	PCGEXTENDEDTOOLKIT_API TArray<UFunction*> FindUserFunctions(const TSubclassOf<AActor>& ActorClass, const TArray<FName>& FunctionNames, const TArray<const UFunction*>& FunctionPrototypes, const FPCGContext* InContext);
}

/** Holds function prototypes used to match against actor function signatures. */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExFunctionPrototypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UFunction* GetPrototypeWithNoParams() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithNoParams")); }

private:
	UFUNCTION()
	void PrototypeWithNoParams()
	{
	}
};

namespace PCGEx
{
	const FName InvalidName = "INVALID_DATA";

	PCGEXTENDEDTOOLKIT_API EPCGPointNativeProperties GetPointNativeProperties(uint8 Flags);

	PCGEXTENDEDTOOLKIT_API FName GetLongNameFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized = true);

	PCGEXTENDEDTOOLKIT_API FPCGAttributeIdentifier GetAttributeIdentifier(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized = true);

	PCGEXTENDEDTOOLKIT_API FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName, const UPCGData* InData);

	PCGEXTENDEDTOOLKIT_API FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName);

	PCGEXTENDEDTOOLKIT_API FPCGAttributePropertyInputSelector GetSelectorFromIdentifier(const FPCGAttributeIdentifier& InIdentifier);

	class PCGEXTENDEDTOOLKIT_API FPCGExAsyncStateScope
	{
	public:
		explicit FPCGExAsyncStateScope(FPCGContext* InContext, const bool bDesired);
		~FPCGExAsyncStateScope();

	private:
		FPCGContext* Context = nullptr;
		bool bRestoreTo = false;
	};

	class PCGEXTENDEDTOOLKIT_API FIntTracker final : public TSharedFromThis<FIntTracker>
	{
		FRWLock Lock;
		bool bTriggered = false;
		int32 PendingCount = 0;
		int32 CompletedCount = 0;

		TFunction<void()> StartFn = nullptr;
		TFunction<void()> ThresholdFn = nullptr;

	public:
		explicit FIntTracker(TFunction<void()>&& InThresholdFn)
		{
			ThresholdFn = InThresholdFn;
		}

		explicit FIntTracker(TFunction<void()>&& InStartFn, TFunction<void()>&& InThresholdFn)
		{
			StartFn = InStartFn;
			ThresholdFn = InThresholdFn;
		}

		~FIntTracker() = default;

		void IncrementPending(const int32 Count = 1);
		void IncrementCompleted(const int32 Count = 1);

		void Trigger();
		void SafetyTrigger();

		void Reset();
		void Reset(const int32 InMax);

	protected:
		void TriggerInternal();
	};

	class PCGEXTENDEDTOOLKIT_API FUniqueNameGenerator final : public TSharedFromThis<FUniqueNameGenerator>
	{
		int32 Idx = 0;

	public:
		FUniqueNameGenerator() = default;
		~FUniqueNameGenerator() = default;

		FName Get(const FString& BaseName);
		FName Get(const FName& BaseName);
	};

	class PCGEXTENDEDTOOLKIT_API FWorkHandle final : public TSharedFromThis<FWorkHandle>
	{
	public:
		FWorkHandle() = default;
		~FWorkHandle() = default;
	};

	class PCGEXTENDEDTOOLKIT_API FManagedObjects : public TSharedFromThis<FManagedObjects>
	{
	protected:
		mutable FRWLock ManagedObjectLock;
		mutable FRWLock DuplicatedObjectLock;

	public:
		TWeakPtr<FWorkHandle> WorkHandle;
		TWeakPtr<FPCGContextHandle> WeakHandle;
		TSet<TObjectPtr<UObject>> ManagedObjects;

		bool IsFlushing() const { return bIsFlushing.load(std::memory_order_acquire); }

		explicit FManagedObjects(FPCGContext* InContext, const TWeakPtr<FWorkHandle>& InWorkHandle);

		~FManagedObjects();

		bool IsAvailable() const;

		void Flush();

		bool Add(UObject* InObject);
		bool Remove(UObject* InObject);
		void Remove(const TArray<FPCGTaggedData>& InTaggedData);

		void AddExtraStructReferencedObjects(FReferenceCollector& Collector);

		template <class T, typename... Args>
		T* New(Args&&... InArgs)
		{
			if (!WorkHandle.IsValid()) { return nullptr; }
			
			FPCGContext::FSharedContext<FPCGContext> SharedContext(WeakHandle);
			if (!SharedContext.Get()) { return nullptr; }

			T* Object = nullptr;
			if (!IsInGameThread())
			{
				{
					FGCScopeGuard Scope;
					Object = NewObject<T>(std::forward<Args>(InArgs)...);
				}
				check(Object);
			}
			else
			{
				Object = NewObject<T>(std::forward<Args>(InArgs)...);
			}

			Add(Object);
			return Object;
		}

		template <class T>
		T* DuplicateData(const UPCGData* InData)
		{
			if (!WorkHandle.IsValid()) { return nullptr; }
			
			FPCGContext::FSharedContext<FPCGContext> SharedContext(WeakHandle);
			if (!SharedContext.Get()) { return nullptr; }

			T* Object = nullptr;

			if (!IsInGameThread())
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);
				FPCGExAsyncStateScope ForceAsyncState(SharedContext.Get(), false);

				// Do the duplicate (uses AnyThread that requires bIsRunningOnMainThread to be up-to-date)
				Object = Cast<T>(InData->DuplicateData(SharedContext.Get(), true));

				check(Object);
				{
					FWriteScopeLock DupeLock(DuplicatedObjectLock);
					DuplicateObjects.Add(Object);
				}
			}
			else
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);
				Object = Cast<T>(InData->DuplicateData(SharedContext.Get(), true));
				check(Object);
				{
					FWriteScopeLock DupeLock(DuplicatedObjectLock);
					DuplicateObjects.Add(Object);
				}
			}

			Add(Object);
			return Object;
		}

		void Destroy(UObject* InObject);

	protected:
		TSet<UObject*> DuplicateObjects;
		void RecursivelyClearAsyncFlag_Unsafe(UObject* InObject) const;

	private:
		std::atomic<bool> bIsFlushing{false};
	};

#pragma region Metadata Type

	PCGEXTENDEDTOOLKIT_API
	bool HasAttribute(const UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier);

	static bool HasAttribute(const UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return false; }
		return HasAttribute(InData->ConstMetadata(), Identifier);
	}

	template <typename T>
	static const FPCGMetadataAttribute<T>* TryGetConstAttribute(const UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InMetadata) { return nullptr; }
		if (!InMetadata->GetConstMetadataDomain(Identifier.MetadataDomain)) { return nullptr; }

		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		return InMetadata->template GetConstTypedAttribute<T>(Identifier);
	}

	template <typename T>
	static const FPCGMetadataAttribute<T>* TryGetConstAttribute(const UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return nullptr; }
		return TryGetConstAttribute<T>(InData->ConstMetadata(), Identifier);
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* TryGetMutableAttribute(UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InMetadata) { return nullptr; }
		if (!InMetadata->GetConstMetadataDomain(Identifier.MetadataDomain)) { return nullptr; }

		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		return InMetadata->template GetMutableTypedAttribute<T>(Identifier);
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* TryGetMutableAttribute(UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return nullptr; }
		return TryGetMutableAttribute<T>(InData->MutableMetadata(), Identifier);
	}
	
	constexpr static EPCGMetadataTypes GetPropertyType(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density:
		case EPCGPointProperties::Steepness: return EPCGMetadataTypes::Float;
		case EPCGPointProperties::BoundsMin:
		case EPCGPointProperties::BoundsMax:
		case EPCGPointProperties::Extents:
		case EPCGPointProperties::Position:
		case EPCGPointProperties::Scale:
		case EPCGPointProperties::LocalCenter:
		case EPCGPointProperties::LocalSize:
		case EPCGPointProperties::ScaledLocalSize: return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Color: return EPCGMetadataTypes::Vector4;
		case EPCGPointProperties::Rotation: return EPCGMetadataTypes::Quaternion;
		case EPCGPointProperties::Transform: return EPCGMetadataTypes::Transform;
		case EPCGPointProperties::Seed: return EPCGMetadataTypes::Integer32;
		default: return EPCGMetadataTypes::Unknown;
		}
	}

	constexpr static EPCGPointNativeProperties GetPropertyNativeTypes(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density: return EPCGPointNativeProperties::Density;
		case EPCGPointProperties::BoundsMin: return EPCGPointNativeProperties::BoundsMin;
		case EPCGPointProperties::BoundsMax: return EPCGPointNativeProperties::BoundsMax;
		case EPCGPointProperties::Color: return EPCGPointNativeProperties::Color;
		case EPCGPointProperties::Position: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Rotation: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Scale: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Transform: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Steepness: return EPCGPointNativeProperties::Steepness;
		case EPCGPointProperties::Seed: return EPCGPointNativeProperties::Seed;
		case EPCGPointProperties::Extents:
		case EPCGPointProperties::LocalCenter:
		case EPCGPointProperties::LocalSize: return EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax;
		case EPCGPointProperties::ScaledLocalSize: return EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Transform;
		default: return EPCGPointNativeProperties::None;
		}
	}

	constexpr static EPCGMetadataTypes GetPropertyType(const EPCGExtraProperties Property)
	{
		switch (Property)
		{
		case EPCGExtraProperties::Index: return EPCGMetadataTypes::Integer32;
		default: return EPCGMetadataTypes::Unknown;
		}
	}

	constexpr bool DummyBoolean = bool{};
	constexpr int32 DummyInteger32 = int32{};
	constexpr int64 DummyInteger64 = int64{};
	constexpr float DummyFloat = float{};
	constexpr double DummyDouble = double{};
	const FVector2D DummyVector2 = FVector2D::ZeroVector;
	const FVector DummyVector = FVector::ZeroVector;
	const FVector4 DummyVector4 = FVector4::Zero();
	const FQuat DummyQuaternion = FQuat::Identity;
	const FRotator DummyRotator = FRotator::ZeroRotator;
	const FTransform DummyTransform = FTransform::Identity;
	const FString DummyString = TEXT("");
	const FName DummyName = NAME_None;
	const FSoftClassPath DummySoftClassPath = FSoftClassPath{};
	const FSoftObjectPath DummySoftObjectPath = FSoftObjectPath{};

	template <typename Func>
	static void ExecuteWithRightType(const EPCGMetadataTypes Type, Func&& Callback)
	{
#define PCGEX_EXECUTE_WITH_TYPE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID : Callback(Dummy##_ID); break;

		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTE_WITH_TYPE)
		default: ;
		}

#undef PCGEX_EXECUTE_WITH_TYPE
	}

	template <typename Func>
	static void ExecuteWithRightType(const int16 Type, Func&& Callback)
	{
#define PCGEX_EXECUTE_WITH_TYPE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID : Callback(Dummy##_ID); break;

		switch (static_cast<EPCGMetadataTypes>(Type))
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTE_WITH_TYPE)
		default: ;
		}

#undef PCGEX_EXECUTE_WITH_TYPE
	}

#pragma endregion

	class FReadWriteScope
	{
		TArray<int32> ReadIndices;
		TArray<int32> WriteIndices;

	public:
		FReadWriteScope(const int32 NumElements, const bool bSetNum);

		int32 Add(const int32 ReadIndex, const int32 WriteIndex);
		int32 Add(const TArrayView<int32> ReadIndicesRange, int32& OutWriteIndex);
		void Set(const int32 Index, const int32 ReadIndex, const int32 WriteIndex);

		void CopyPoints(const UPCGBasePointData* Read, UPCGBasePointData* Write, const bool bClean = true, const bool bInitializeMetadata = false);
		void CopyProperties(const UPCGBasePointData* Read, UPCGBasePointData* Write, EPCGPointNativeProperties Properties, const bool bClean = true);

		bool IsEmpty() const { return ReadIndices.IsEmpty(); }
		int32 Num() const { return ReadIndices.Num(); }

		const TArray<int32>& GetReadIndices() const { return ReadIndices; }
		const TArray<int32>& GetWriteIndices() const { return WriteIndices; }
	};

	PCGEXTENDEDTOOLKIT_API int32 SetNumPointsAllocated(UPCGBasePointData* InData, const int32 InNumPoints, EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All);

	PCGEXTENDEDTOOLKIT_API bool EnsureMinNumPoints(UPCGBasePointData* InData, const int32 InNumPoints);

#pragma region Array

	template <typename T>
	static void Reverse(TArrayView<T> View)
	{
		const int32 Count = View.Num();
		for (int32 i = 0; i < Count / 2; ++i) { Swap(View[i], View[Count - 1 - i]); }
	}

	template <typename T>
	static void Reverse(TPCGValueRange<T> Range)
	{
		const int32 Count = Range.Num();
		for (int32 i = 0; i < Count / 2; ++i) { Swap(Range[i], Range[Count - 1 - i]); }
	}

	template <typename T>
	static void InitArray(TArray<T>& InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	static void InitArray(TSharedPtr<TArray<T>>& InArray, const int32 Num)
	{
		if (!InArray) { InArray = MakeShared<TArray<T>>(); }
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

	template <typename T>
	static void InitArray(TSharedRef<TArray<T>> InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	static void InitArray(TArray<T>* InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

	template <typename T>
	void ReorderArray(TArray<T>& InArray, const TArray<int32>& InOrder)
	{
		const int32 NumElements = InOrder.Num();
		check(NumElements <= InArray.Num());

		TBitArray<> Visited;
		Visited.Init(false, NumElements);

		for (int32 i = 0; i < NumElements; ++i)
		{
			if (Visited[i])
			{
				continue;
			}

			int32 Current = i;
			int32 Next = InOrder[Current];

			if (Next == Current)
			{
				Visited[Current] = true;
				continue;
			}

			T Temp = MoveTemp(InArray[Current]);

			while (!Visited[Next])
			{
				InArray[Current] = MoveTemp(InArray[Next]);

				Visited[Current] = true;
				Current = Next;
				Next = InOrder[Current];
			}

			InArray[Current] = MoveTemp(Temp);
			Visited[Current] = true;
		}
	}

	template <typename D>
	struct TOrder
	{
		int32 Index = -1;
		D Det;

		TOrder(const int32 InIndex, const D& InDet)
			: Index(InIndex), Det(InDet)
		{
		}
	};

	template <typename T>
	void ReorderValueRange(TPCGValueRange<T>& InRange, const TArray<int32>& InOrder);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template void ReorderValueRange<_TYPE>(TPCGValueRange<_TYPE>& InRange, const TArray<int32>& InOrder);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	PCGEXTENDEDTOOLKIT_API void ReorderPointArrayData(UPCGBasePointData* InData, const TArray<int32>& InOrder);

	template <typename T>
	static void ShiftArrayToSmallest(TArray<T>& InArray)
	{
		const int32 Num = InArray.Num();
		if (Num <= 1) { return; }

		int32 MinIndex = 0;
		for (int32 i = 1; i < Num; ++i) { if (InArray[i] < InArray[MinIndex]) { MinIndex = i; } }

		if (MinIndex > 0)
		{
			TArray<T> TempArray;
			TempArray.Append(InArray.GetData() + MinIndex, Num - MinIndex);
			TempArray.Append(InArray.GetData(), MinIndex);

			FMemory::Memcpy(InArray.GetData(), TempArray.GetData(), sizeof(T) * Num);
		}
	}

	template <typename T, typename FPredicate>
	static void ShiftArrayToPredicate(TArray<T>& InArray, FPredicate&& Predicate)
	{
		const int32 Num = InArray.Num();
		if (Num <= 1) { return; }

		int32 MinIndex = 0;
		for (int32 i = 1; i < Num; ++i) { if (Predicate(InArray[i], InArray[MinIndex])) { MinIndex = i; } }

		if (MinIndex > 0)
		{
			TArray<T> TempArray;
			TempArray.Append(InArray.GetData() + MinIndex, Num - MinIndex);
			TempArray.Append(InArray.GetData(), MinIndex);

			FMemory::Memcpy(InArray.GetData(), TempArray.GetData(), sizeof(T) * Num);
		}
	}

	template <typename T, typename D>
	void ReorderArray(TArray<T>& InArray, const TArray<TOrder<D>>& InOrder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExHelpers::ReorderArray);

		check(InArray.Num() == InOrder.Num());

		const int32 NumElements = InArray.Num();
		TBitArray<> Visited;
		Visited.Init(false, NumElements);

		for (int32 i = 0; i < NumElements; ++i)
		{
			if (Visited[i])
			{
				continue; // Skip already visited elements in a cycle.
			}

			int32 Current = i;
			T Temp = MoveTemp(InArray[i]); // Temporarily hold the current element.

			// Follow the cycle defined by the indices in InOrder.
			while (!Visited[Current])
			{
				Visited[Current] = true;

				int32 Next = InOrder[Current].Index;
				if (Next == i)
				{
					InArray[Current] = MoveTemp(Temp);
					break;
				}

				InArray[Current] = MoveTemp(InArray[Next]);
				Current = Next;
			}
		}
	}

#pragma endregion

	PCGEXTENDEDTOOLKIT_API FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector);
}
