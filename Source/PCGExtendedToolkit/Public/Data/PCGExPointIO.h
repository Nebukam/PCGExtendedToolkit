﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Data/PCGPointArrayData.h"
#include "PCGElement.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "PCGExMT.h"
#include "PCGEx.h"
#include "PCGExCommon.h"
#include "PCGExContext.h"
#include "PCGExPointElements.h"
#include "PCGExHelpers.h"
#include "PCGParamData.h"

namespace PCGExData
{
	class FTags;
	class FPointIO;
}

namespace PCGExData
{
	enum class EIOInit : uint8
	{
		NoInit UMETA(DisplayName = "No Output"),
		New UMETA(DisplayName = "Create Empty Output Object"),
		Duplicate UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};

#pragma region FPointIO
	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIO : public TSharedFromThis<FPointIO>
	{
		friend class FPointIOCollection;

	protected:
		bool bTransactional = false;
		bool bMutable = false;
		bool bPinless = false;

		TWeakPtr<FPCGContextHandle> ContextHandle;

		mutable FRWLock PointsLock;
		mutable FRWLock InKeysLock;
		mutable FRWLock OutKeysLock;
		mutable FRWLock AttributesLock;
		mutable FRWLock IdxMappingLock;

		bool bWritten = false;
		int32 NumInPoints = -1;

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> InKeys; // Shared because reused by duplicates
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> OutKeys;

		const UPCGData* OriginalIn = nullptr;  // Input PointData	
		const UPCGBasePointData* In = nullptr; // Input PointData	
		UPCGBasePointData* Out = nullptr;      // Output PointData

		TWeakPtr<FPointIO> RootIO;
		std::atomic<bool> bIsEnabled{true};

		TSharedPtr<TArray<int32>> IdxMapping;

	public:
		TSharedPtr<FTags> Tags;
		int32 IOIndex = 0;
		int32 InitializationIndex = -1;
		TObjectPtr<const UPCGData> InitializationData = nullptr;

		bool bAllowEmptyOutput = false;

		FORCEINLINE bool IsForwarding() const { return Out && Out == In; }

		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle);
		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle, const UPCGBasePointData* InData);
		explicit FPointIO(const TSharedRef<FPointIO>& InPointIO);

		EPCGPointNativeProperties GetAllocations() const { return In ? In->GetAllocatedProperties() : EPCGPointNativeProperties::None; }

		TWeakPtr<FPCGContextHandle> GetContextHandle() const { return ContextHandle; }
		FPCGExContext* GetContext() const;

		void SetInfos(const int32 InIndex, const FName InOutputPin, const TSet<FString>* InTags = nullptr);

		bool InitializeOutput(EIOInit InitOut = EIOInit::NoInit);

		template <typename T>
		bool InitializeOutput(const EIOInit InitOut = EIOInit::NoInit)
		{
			PCGEX_SHARED_CONTEXT(ContextHandle)

			if (InitOut == EIOInit::Forward && IsValid(Out) && Out == In)
			{
				// Already forwarding
				return true;
			}

			if (IsValid(Out) && Out != In)
			{
				SharedContext.Get()->ManagedObjects->Destroy(Out);
				Out = nullptr;
			}

			if (InitOut == EIOInit::NoInit)
			{
				bMutable = false;
				Out = nullptr;
				return true;
			}

			bMutable = true;

			if (InitOut == EIOInit::New)
			{
				T* TypedOut = SharedContext.Get()->ManagedObjects->New<T>();
				Out = Cast<UPCGBasePointData>(TypedOut);

				check(Out)

				if (IsValid(In))
				{
					PCGExHelpers::InitEmptyNativeProperties(In, Out);

					FPCGInitializeFromDataParams InitializeFromDataParams(In);
					InitializeFromDataParams.bInheritSpatialData = false;
					Out->InitializeFromDataWithParams(InitializeFromDataParams);
				}

				return true;
			}

			if (InitOut == EIOInit::Duplicate)
			{
				check(In)

				if (const T* TypedIn = Cast<T>(In))
				{
					T* TypedOut = SharedContext.Get()->ManagedObjects->DuplicateData<T>(TypedIn);
					Out = Cast<UPCGBasePointData>(TypedOut);
				}
				else
				{
					T* TypedOut = SharedContext.Get()->ManagedObjects->New<T>();
					Out = Cast<UPCGBasePointData>(TypedOut);

					FPCGInitializeFromDataParams InitializeFromDataParams(In);
					Out->InitializeFromDataWithParams(InitializeFromDataParams);
				}

				return true;
			}

			return InitializeOutput(InitOut);
		}

		~FPointIO();

		bool IsDataValid(const EIOSide InSource) const { return InSource == EIOSide::In ? IsValid(In) : IsValid(Out); }

		FORCEINLINE const UPCGBasePointData* GetData(const EIOSide InSide) const { return InSide == EIOSide::In ? In : Out; }
		FORCEINLINE UPCGBasePointData* GetMutableData(const EIOSide InSide) const { return const_cast<UPCGBasePointData*>(InSide == EIOSide::In ? In : Out); }
		FORCEINLINE const UPCGBasePointData* GetIn() const { return In; }
		FORCEINLINE UPCGBasePointData* GetOut() const { return Out; }
		FORCEINLINE const UPCGBasePointData* GetOutIn() const { return Out ? Out : In; }
		FORCEINLINE const UPCGBasePointData* GetInOut() const { return In ? In : Out; }
		const UPCGBasePointData* GetOutIn(EIOSide& OutSide) const;
		const UPCGBasePointData* GetInOut(EIOSide& OutSide) const;
		bool GetSource(const UPCGData* InData, EIOSide& OutSide) const;

		int32 GetNum() const { return In ? In->GetNumPoints() : Out ? Out->GetNumPoints() : -1; }
		int32 GetNum(const EIOSide Source) const { return Source == EIOSide::In ? In->GetNumPoints() : Out->GetNumPoints(); }
		FTaggedData GetTaggedData(const EIOSide Source = EIOSide::In) { return FTaggedData(GetData(Source), Tags, GetInKeys()); }

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetInKeys();
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetOutKeys(const bool bEnsureValidKeys = false);

		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintOutInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;

		FORCEINLINE FConstPoint GetInPoint(const int32 Index) const { return FConstPoint(In, Index, IOIndex); }
		FORCEINLINE FMutablePoint GetOutPoint(const int32 Index) const { return FMutablePoint(Out, Index, IOIndex); }

		FScope GetInScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetInScope(const PCGExMT::FScope& Scope) const { return GetInScope(Scope.Start, Scope.Count, true); }
		FScope GetInFullScope() const { return GetInScope(0, In->GetNumPoints(), true); }
		FScope GetInRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

		FScope GetOutScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetOutScope(const PCGExMT::FScope& Scope) const { return GetOutScope(Scope.Start, Scope.Count, true); }
		FScope GetOutFullScope() const { return GetOutScope(0, Out->GetNumPoints(), true); }
		FScope GetOutRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

		void SetPoints(const TArray<FPCGPoint>& InPCGPoints);
		void SetPoints(const int32 StartIndex, const TArray<FPCGPoint>& InPCGPoints, const EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All);

		FName OutputPin = PCGPinConstants::DefaultOutputLabel;

		void InitPoint(int32 Index, const PCGMetadataEntryKey ParentKey) const
		{
			TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange();
			Out->Metadata->InitializeOnSet(MetadataEntries[Index], ParentKey, In->Metadata);
		}

		void InitPoint(int32 Index) const
		{
			TPCGValueRange<int64> MetadataEntries = Out->GetMetadataEntryValueRange();
			Out->Metadata->InitializeOnSet(MetadataEntries[Index]);
		}

		// There's a recurring need for this to deal with ::New
		TArray<int32>& GetIdxMapping(const int32 NumElements = -1);
		void ClearIdxMapping();
		void ConsumeIdxMapping(const EPCGPointNativeProperties Properties, const bool bClear = true);

		// In -> Out
		void InheritProperties(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count, const EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All) const;
		void InheritProperties(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices, const EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All) const;

		// ReadIndices is expected to be the size of out point count here
		// Shorthand to simplify copying properties from original points when all outputs should be initialized as copies
		void InheritProperties(const TArrayView<const int32>& ReadIndices, const EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All) const;

		void InheritPoints(const int32 ReadStartIndex, const int32 WriteStartIndex, const int32 Count) const;
		void InheritPoints(const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices) const;
		int32 InheritPoints(const TArrayView<const int8>& Mask, const bool bInvert) const;
		int32 InheritPoints(const TBitArray<>& Mask, const bool bInvert) const;

		// WriteIndices is expected to be the size of in point count here
		// Shorthand to simplify point insertion in cases where we want to preserve original points
		void InheritPoints(const TArrayView<const int32>& WriteIndices) const;

		// ReadIndices is expected to the size of OUT point counts
		// Shorthand to simplify point insertion in cases where we want to copy a subset of the original points
		// !!! Note that this method resizes the data !!!
		void InheritPoints(const TArrayView<const int32>& SelectedIndices, const int32 StartIndex, EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All) const;

		// Copies a single index over to target writes
		void RepeatPoint(const int32 ReadIndex, const TArrayView<const int32>& WriteIndices, EIOSide ReadSide = EIOSide::In) const;

		// Copies a single index N times starting at write index
		void RepeatPoint(const int32 ReadIndex, const int32 WriteIndex, const int32 Count, EIOSide ReadSide = EIOSide::In) const;

		void CopyToNewPoint(const int32 InIndex, int32& OutIndex) const
		{
			FWriteScopeLock WriteLock(PointsLock);
			OutIndex = Out->GetNumPoints();
			Out->SetNumPoints(OutIndex + 1);
			InheritProperties(InIndex, OutIndex, 1);
			InitPoint(OutIndex, In->GetMetadataEntry(InIndex));
		}

		void ClearCachedKeys();

		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }
		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }

		bool StageOutput(FPCGExContext* TargetContext) const;
		bool StageOutput(FPCGExContext* TargetContext, const int32 MinPointCount, const int32 MaxPointCount) const;
		bool StageAnyOutput(FPCGExContext* TargetContext) const;

		int32 Gather(const TArrayView<int32> InIndices) const;
		int32 Gather(const TArrayView<int8> InMask, const bool bInvert = false) const;
		int32 Gather(const TBitArray<>& InMask, const bool bInvert = false) const;

		void DeleteAttribute(const FPCGAttributeIdentifier& Identifier) const;
		void DeleteAttribute(const FPCGMetadataAttributeBase* Attribute) const;

		void GetDataAsProxyPoint(FProxyPoint& OutPoint, const EIOSide Side = EIOSide::In) const;

		template <typename T>
		FPCGMetadataAttribute<T>* CreateAttribute(const FPCGAttributeIdentifier& Identifier, const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			FPCGAttributeIdentifier SanitizedIdentifier =
				Identifier.MetadataDomain.IsDefault() ?
					PCGEx::GetAttributeIdentifier(Identifier.Name, Out) :
					Identifier;

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->CreateAttribute(SanitizedIdentifier, DefaultValue, bAllowsInterpolation, bOverrideParent);
			}

			return OutAttribute;
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindOrCreateAttribute(const FPCGAttributeIdentifier& Identifier, const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			FPCGAttributeIdentifier SanitizedIdentifier =
				Identifier.MetadataDomain.IsDefault() ?
					PCGEx::GetAttributeIdentifier(Identifier.Name, Out) :
					Identifier;

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->FindOrCreateAttribute(SanitizedIdentifier, DefaultValue, bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			}

			return OutAttribute;
		}

		const FPCGMetadataAttributeBase* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return PCGEx::TryGetMutableAttribute<T>(GetMutableData(InSide), InIdentifier);
		}

		FPCGMetadataAttributeBase* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return PCGEx::TryGetConstAttribute<T>(GetData(InSide), InIdentifier);
		}
	};

	static TSharedPtr<FPointIO> NewPointIO(FPCGExContext* InContext, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InContext->GetOrCreateHandle())
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

	static TSharedPtr<FPointIO> NewPointIO(FPCGExContext* InContext, const UPCGBasePointData* InData, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InContext->GetOrCreateHandle(), InData)
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

	static TSharedPtr<FPointIO> NewPointIO(const TSharedRef<FPointIO>& InPointIO, FName InOutputPin = NAME_None, int32 Index = -1)
	{
		PCGEX_MAKE_SHARED(NewIO, FPointIO, InPointIO)
		NewIO->SetInfos(Index, InOutputPin);
		return NewIO;
	}

#pragma endregion

#pragma region FPointIOCollection

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPointIOCollection : public TSharedFromThis<FPointIOCollection>
	{
	protected:
		mutable FRWLock PairsLock;
		TWeakPtr<FPCGContextHandle> ContextHandle;
		bool bTransactional = false;

	public:
		explicit FPointIOCollection(FPCGExContext* InContext, bool bIsTransactional = false);
		FPointIOCollection(FPCGExContext* InContext, FName InputLabel, EIOInit InitOut = EIOInit::NoInit, bool bIsTransactional = false);
		FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, EIOInit InitOut = EIOInit::NoInit, bool bIsTransactional = false);

		~FPointIOCollection();

		FName OutputPin = PCGPinConstants::DefaultOutputLabel;
		TArray<TSharedPtr<FPointIO>> Pairs;

		/**
		 * Initialize from Sources
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			TArray<FPCGTaggedData>& Sources,
			EIOInit InitOut = EIOInit::NoInit);

		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::NoInit, const TSet<FString>* Tags = nullptr);
		TSharedPtr<FPointIO> Emplace_GetRef(EIOInit InitOut = EIOInit::New);
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::NoInit);

		TSharedPtr<FPointIO> Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO);

		bool ContainsData_Unsafe(const UPCGData* InData) const;
		TSharedPtr<FPointIO> Add_Unsafe(const TSharedPtr<FPointIO>& PointIO);
		TSharedPtr<FPointIO> Add(const TSharedPtr<FPointIO>& PointIO);

		void Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs);
		void Add(const TArray<TSharedPtr<FPointIO>>& IOs);


		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::NoInit, const TSet<FString>* Tags = nullptr)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle, In));
			NewIO->SetInfos(Pairs.Num() - 1, OutputPin, Tags);
			if (!NewIO->InitializeOutput<T>(InitOut)) { return nullptr; }
			return NewIO;
		}

		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const EIOInit InitOut = EIOInit::New)
		{
			FWriteScopeLock WriteLock(PairsLock);
			TSharedPtr<FPointIO> NewIO = Pairs.Add_GetRef(MakeShared<FPointIO>(ContextHandle));
			NewIO->SetInfos(Pairs.Num() - 1, OutputPin);
			if (!NewIO->InitializeOutput<T>(InitOut)) { return nullptr; }
			return NewIO;
		}

		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::NoInit)
		{
			TSharedPtr<FPointIO> Branch = Emplace_GetRef<T>(PointIO->GetIn(), InitOut);
			if (!Branch) { return nullptr; }

			OverrideTags(PointIO, Branch);
			Branch->RootIO = PointIO;
			return Branch;
		}

		static void OverrideTags(const TSharedPtr<FPointIO>& InFrom, const TSharedPtr<FPointIO>& InTo);

		bool IsEmpty() const { return Pairs.IsEmpty(); }
		int32 Num() const { return Pairs.Num(); }

		TSharedPtr<FPointIO> operator[](const int32 Index) const { return Pairs[Index]; }

		void IncreaseReserve(int32 InIncreaseNum);

		void StageOutputs();
		void StageOutputs(const int32 MinPointCount, const int32 MaxPointCount);
		void StageAnyOutputs();

		void Sort();

		FBox GetInBounds() const;
		FBox GetOutBounds() const;

		void PruneNullEntries(const bool bUpdateIndices);

		void Flush();
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedEntries : public TSharedFromThis<FPointIOTaggedEntries>
	{
	public:
		TSharedPtr<FPointIO> Key;
		FString TagId;
		PCGExCommon::DataIDType TagValue;
		TArray<TSharedRef<FPointIO>> Entries;

		FPointIOTaggedEntries(TSharedPtr<FPointIO> InKey, const FString& InTagId, const PCGExCommon::DataIDType& InTagValue)
			: Key(InKey), TagId(InTagId), TagValue(InTagValue)
		{
		}

		~FPointIOTaggedEntries() = default;

		void Add(const TSharedRef<FPointIO>& Value);
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedDictionary
	{
	public:
		FString TagIdentifier;     // LeftSide
		TMap<int32, int32> TagMap; // :RightSize @ Entry Index
		TArray<TSharedPtr<FPointIOTaggedEntries>> Entries;

		explicit FPointIOTaggedDictionary(const FString& InTagId)
			: TagIdentifier(InTagId)
		{
		}

		~FPointIOTaggedDictionary() = default;

		bool CreateKey(const TSharedRef<FPointIO>& PointIOKey);
		bool RemoveKey(const TSharedRef<FPointIO>& PointIOKey);
		bool TryAddEntry(const TSharedRef<FPointIO>& PointIOEntry);
		TSharedPtr<FPointIOTaggedEntries> GetEntries(int32 Key);
	};

#pragma endregion

	namespace PCGExPointIO
	{
		int32 GetTotalPointsNum(const TArray<TSharedPtr<FPointIO>>& InIOs, const EIOSide InSide = EIOSide::In);

		PCGEXTENDEDTOOLKIT_API
		const UPCGBasePointData* GetPointData(const FPCGContext* Context, const FPCGTaggedData& Source);

		PCGEXTENDEDTOOLKIT_API
		UPCGBasePointData* GetMutablePointData(const FPCGContext* Context, const FPCGTaggedData& Source);

		PCGEXTENDEDTOOLKIT_API
		const UPCGBasePointData* ToPointData(FPCGExContext* Context, const FPCGTaggedData& Source);
	}

	PCGEXTENDEDTOOLKIT_API
	void GetPoints(const FScope& Scope, TArray<FPCGPoint>& OutPCGPoints);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FPointIO> TryGetSingleInput(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bThrowError);
}
