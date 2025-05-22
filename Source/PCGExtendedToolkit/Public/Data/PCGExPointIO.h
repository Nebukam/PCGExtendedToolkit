// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Metadata/Accessors//PCGCustomAccessor.h"
#include "PCGElement.h"
#include "UObject/Object.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "PCGExMT.h"
#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExDataTag.h"
#include "PCGParamData.h"

#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExData
{
	class FPointIO;
}

namespace PCGExData
{
	enum class EIOInit : uint8
	{
		None UMETA(DisplayName = "No Output"),
		New UMETA(DisplayName = "Create Empty Output Object"),
		Duplicate UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};

	enum class EIOSide : uint8
	{
		In,
		Out
	};

	struct FScope
	{
		UPCGBasePointData* Data = nullptr;
		int32 Start = -1;
		int32 Count = -1;
		int32 End = -1;

		FScope() = default;

		FScope(UPCGBasePointData* InData, const int32 InStart, const int32 InCount);
		FScope(const UPCGBasePointData* InData, const int32 InStart, const int32 InCount);

		~FScope() = default;
		bool IsValid() const { return Start >= 0 && Count > 0 && Data->GetNumPoints() <= End; }
		void GetIndices(TArray<int32>& OutIndices) const;
	};

#pragma region FPoint

	// FPoint is used when we only care about point index
	// And possibly IOIndex as well, but without the need to track the actual data object.
	// This makes it a barebone way to track point indices with some additional mapping mechanism with a secondary index
	struct PCGEXTENDEDTOOLKIT_API FPoint
	{
		int32 Index = -1;
		int32 IO = -1;

		FPoint() = default;
		virtual ~FPoint() = default;

		explicit FPoint(const uint64 Hash);
		explicit FPoint(const int32 InIndex, const int32 InIO = -1);
		FPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex);

		FORCEINLINE bool IsValid() const { return Index >= 0; }
		FORCEINLINE uint64 H64() const { return PCGEx::H64U(Index, IO); }

		explicit operator int32() const { return Index; }

		bool operator==(const FPoint& Other) const { return Index == Other.Index && IO == Other.IO; }
		FORCEINLINE friend uint32 GetTypeHash(const FPoint& Key) { return HashCombineFast(Key.Index, Key.IO); }

		virtual const FTransform& GetTransform() const PCGEX_NOT_IMPLEMENTED_RET(GetTransform, FTransform::Identity)
		virtual FVector GetLocation() const PCGEX_NOT_IMPLEMENTED_RET(GetLocation, FVector::OneVector)
		virtual FVector GetScale3D() const PCGEX_NOT_IMPLEMENTED_RET(GetScale3D, FVector::OneVector)
		virtual FQuat GetRotation() const PCGEX_NOT_IMPLEMENTED_RET(GetRotation, FQuat::Identity)
		virtual FVector GetBoundsMin() const PCGEX_NOT_IMPLEMENTED_RET(GetBoundsMin, FVector::OneVector)
		virtual FVector GetBoundsMax() const PCGEX_NOT_IMPLEMENTED_RET(GetBoundsMax, FVector::OneVector)
		virtual FVector GetExtents() const PCGEX_NOT_IMPLEMENTED_RET(GetExtents, FVector::OneVector)
		virtual FVector GetScaledExtents() const PCGEX_NOT_IMPLEMENTED_RET(GetScaledExtents, FVector::OneVector)
		virtual FBox GetLocalBounds() const PCGEX_NOT_IMPLEMENTED_RET(GetLocalBounds, FBox(NoInit))
		virtual FBox GetLocalDensityBounds() const PCGEX_NOT_IMPLEMENTED_RET(GetLocalDensityBounds, FBox(NoInit))
		virtual FBox GetScaledBounds() const PCGEX_NOT_IMPLEMENTED_RET(GetScaledBounds, FBox(NoInit))
		virtual float GetSteepness() const PCGEX_NOT_IMPLEMENTED_RET(GetSteepness, 1)
		virtual float GetDensity() const PCGEX_NOT_IMPLEMENTED_RET(GetDensity, 1)
		virtual int64 GetMetadataEntry() const PCGEX_NOT_IMPLEMENTED_RET(GetMetadataEntry, 0)
	};

	struct PCGEXTENDEDTOOLKIT_API FWeightedPoint : FPoint
	{
		double Weight = 0;

		FWeightedPoint() = default;
		virtual ~FWeightedPoint() = default;

		explicit FWeightedPoint(const uint64 Hash, const double InWeight = 1);
		explicit FWeightedPoint(const int32 InIndex, const double InWeight = 1, const int32 InIO = -1);
		FWeightedPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex, const double InWeight = 1);
	};

#define PCGEX_POINT_PROXY_OVERRIDES \
FORCEINLINE virtual const FTransform& GetTransform() const override { return Data->GetTransform(Index); }\
FORCEINLINE virtual FVector GetLocation() const override { return Data->GetTransform(Index).GetLocation(); }\
FORCEINLINE virtual FVector GetScale3D() const override { return Data->GetTransform(Index).GetScale3D(); }\
FORCEINLINE virtual FQuat GetRotation() const override { return Data->GetTransform(Index).GetRotation(); }\
FORCEINLINE virtual FVector GetBoundsMin() const override { return Data->GetBoundsMin(Index); }\
FORCEINLINE virtual FVector GetBoundsMax() const override { return Data->GetBoundsMax(Index); }\
FORCEINLINE virtual FVector GetExtents() const override { return Data->GetExtents(Index); }\
FORCEINLINE virtual FVector GetScaledExtents() const override { return Data->GetScaledExtents(Index); }\
FORCEINLINE virtual FBox GetLocalBounds() const override { return Data->GetLocalBounds(Index); }\
FORCEINLINE virtual FBox GetLocalDensityBounds() const override { return Data->GetLocalDensityBounds(Index); }\
FORCEINLINE virtual FBox GetScaledBounds() const override{\
	const FVector Scale3D = Data->GetTransform(Index).GetScale3D();\
	return FBox(Data->GetBoundsMin(Index) * Scale3D, Data->GetBoundsMax(Index) * Scale3D);}\
FORCEINLINE virtual float GetSteepness() const override { return Data->GetSteepness(Index); }\
FORCEINLINE virtual float GetDensity() const override { return Data->GetDensity(Index); }\
FORCEINLINE virtual int64 GetMetadataEntry() const override { return Data->GetMetadataEntry(Index); }

	// A beefed-up version of FPoint that implement FPoint getters
	// and comes with setters helpers
	// Should be used when the point it references actually exists, otherwise getter will return bad data
	struct PCGEXTENDEDTOOLKIT_API FMutablePoint : FPoint
	{
		UPCGBasePointData* Data = nullptr;

		FMutablePoint() = default;

		FMutablePoint(UPCGBasePointData* InData, const uint64 Hash);
		FMutablePoint(UPCGBasePointData* InData, const int32 InIndex, const int32 InIO = -1);
		FMutablePoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex);

		PCGEX_POINT_PROXY_OVERRIDES

		FTransform& GetMutableTransform();

		virtual void SetTransform(const FTransform& InValue);
		virtual void SetLocation(const FVector& InValue);
		virtual void SetScale3D(const FVector& InValue);
		virtual void SetRotation(const FQuat& InValue);
		virtual void SetBoundsMin(const FVector& InValue);
		virtual void SetBoundsMax(const FVector& InValue);
		virtual void SetExtents(const FVector& InValue, const bool bKeepLocalCenter = false);
		virtual void SetLocalBounds(const FBox& InValue);
		virtual void SetMetadataEntry(const int64 InValue);

		bool operator==(const FMutablePoint& Other) const { return Index == Other.Index && Data == Other.Data; }
	};

	// A beefed-up version of FPoint that implement FPoint getters
	// Only for reading data
	struct PCGEXTENDEDTOOLKIT_API FConstPoint : FPoint
	{
		const UPCGBasePointData* Data = nullptr;

		FConstPoint() = default;

		FConstPoint(const FMutablePoint& InPoint);

		FConstPoint(const UPCGBasePointData* InData, const uint64 Hash);
		FConstPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO = -1);
		FConstPoint(const UPCGBasePointData* InData, const FPoint& InPoint);
		FConstPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex);

		PCGEX_POINT_PROXY_OVERRIDES

		bool operator==(const FConstPoint& Other) const { return Index == Other.Index && IO == Other.IO && Data == Other.Data; }
	};

	// An oddball struct that store some basic information but exposes the same as API FPoint
	// Mostly to be used by helpers that will mutate transform & bounds in-place
	// We need this because in some cases with want to do modification on points that aren't allocated yet
	// or simply work need a basic, mutable spatial representation of a point in the form of Transform + Local bounds

	struct PCGEXTENDEDTOOLKIT_API FProxyPoint : FPoint
	{
		// A point-like object that makes some maths stuff easy to manipulate
		// Also store indices and stuff in order to ret

		FTransform Transform = FTransform::Identity;
		FVector BoundsMin = FVector::OneVector * -1;
		FVector BoundsMax = FVector::OneVector;
		float Steepness = 1; // Required for local density bounds

		FProxyPoint() = default;

		explicit FProxyPoint(const FMutablePoint& InPoint);
		explicit FProxyPoint(const FConstPoint& InPoint);
		FProxyPoint(const UPCGBasePointData* InData, const uint64 Hash);
		FProxyPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO = -1);
		FProxyPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex);

		FORCEINLINE virtual const FTransform& GetTransform() const override { return Transform; }
		FORCEINLINE virtual FVector GetLocation() const override { return Transform.GetLocation(); }
		FORCEINLINE virtual FVector GetScale3D() const override { return Transform.GetScale3D(); }
		FORCEINLINE virtual FQuat GetRotation() const override { return Transform.GetRotation(); }
		FORCEINLINE virtual FVector GetBoundsMin() const override { return BoundsMin; }
		FORCEINLINE virtual FVector GetBoundsMax() const override { return BoundsMax; }
		FORCEINLINE virtual FVector GetExtents() const override { return PCGPointHelpers::GetExtents(BoundsMin, BoundsMax); }
		FORCEINLINE virtual FVector GetScaledExtents() const override { return PCGPointHelpers::GetScaledExtents(Transform, BoundsMin, BoundsMax); }
		FORCEINLINE virtual FBox GetLocalBounds() const override { return FBox(BoundsMin, BoundsMax); }
		FORCEINLINE virtual FBox GetScaledBounds() const override { return FBox(BoundsMin * Transform.GetScale3D(), BoundsMax * Transform.GetScale3D()); }
		FORCEINLINE virtual FBox GetLocalDensityBounds() const override { return PCGPointHelpers::GetLocalDensityBounds(Steepness, BoundsMin, BoundsMax); }

		// Metadata entry stays unimplemented
		//FORCEINLINE virtual int64 GetMetadataEntry() const override { return Data->GetMetadataEntry(Index); }

		//

		FORCEINLINE virtual void SetTransform(const FTransform& InValue) { Transform = InValue; }
		FORCEINLINE virtual void SetLocation(const FVector& InValue) { Transform.SetLocation(InValue); }
		FORCEINLINE virtual void SetScale3D(const FVector& InValue) { Transform.SetScale3D(InValue); }
		FORCEINLINE virtual void SetQuat(const FQuat& InValue) { Transform.SetRotation(InValue); }
		FORCEINLINE virtual void SetBoundsMin(const FVector& InValue) { BoundsMin = InValue; }
		FORCEINLINE virtual void SetBoundsMax(const FVector& InValue) { BoundsMax = InValue; }

		virtual void SetExtents(const FVector& InValue, const bool bKeepLocalCenter = false)
		{
			if (bKeepLocalCenter)
			{
				const FVector LocalCenter = PCGPointHelpers::GetLocalCenter(BoundsMin, BoundsMax);
				BoundsMin = LocalCenter - InValue;
				BoundsMax = LocalCenter + InValue;
			}
			else
			{
				BoundsMin = -InValue;
				BoundsMax = InValue;
			}
		}

		FORCEINLINE virtual void SetLocalBounds(const FBox& InValue)
		{
			BoundsMin = InValue.Min;
			BoundsMax = InValue.Max;
		}

		bool operator==(const FProxyPoint& Other) const { return Index == Other.Index && IO == Other.IO; }

		void CopyTo(UPCGBasePointData* InData) const;
		void CopyTo(FMutablePoint& InPoint) const;
	};

#undef PCGEX_POINT_PROXY_OVERRIDES

	static const FPoint NONE_Point = FPoint(1, -1);
	static const FMutablePoint NONE_MutablePoint = FMutablePoint(nullptr, -1, -1);
	static const FConstPoint NONE_ConstPoint = FConstPoint(nullptr, -1, -1);

	PCGEXTENDEDTOOLKIT_API
	void SetPointProperty(FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty);

#pragma endregion

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
		TWeakPtr<FPCGContextHandle> ContextHandle;

		mutable FRWLock PointsLock;
		mutable FRWLock InKeysLock;
		mutable FRWLock OutKeysLock;
		mutable FRWLock AttributesLock;

		bool bWritten = false;
		int32 NumInPoints = -1;

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> InKeys; // Shared because reused by duplicates
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> OutKeys;

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

		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle):
			ContextHandle(InContextHandle), In(nullptr)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(const TWeakPtr<FPCGContextHandle>& InContextHandle, const UPCGBasePointData* InData):
			ContextHandle(InContextHandle), In(InData)
		{
			PCGEX_LOG_CTR(FPointIO)
		}

		explicit FPointIO(const TSharedRef<FPointIO>& InPointIO):
			ContextHandle(InPointIO->GetContextHandle()), In(InPointIO->GetIn())
		{
			PCGEX_LOG_CTR(FPointIO)
			RootIO = InPointIO;

			TSet<FString> TagDump;
			InPointIO->Tags->DumpTo(TagDump); // Not ideal.

			Tags = MakeShared<FTags>(TagDump);
		}

		TWeakPtr<FPCGContextHandle> GetContextHandle() const { return ContextHandle; }

		void SetInfos(const int32 InIndex,
		              const FName InOutputPin,
		              const TSet<FString>* InTags = nullptr);

		bool InitializeOutput(EIOInit InitOut = EIOInit::None);

		template <typename T>
		bool InitializeOutput(const EIOInit InitOut = EIOInit::None)
		{
			PCGEX_SHARED_CONTEXT(ContextHandle)

			if (IsValid(Out) && Out != In)
			{
				SharedContext.Get()->ManagedObjects->Destroy(Out);
				Out = nullptr;
			}

			bMutable = true;

			if (InitOut == EIOInit::New)
			{
				T* TypedOut = SharedContext.Get()->ManagedObjects->New<T>();
				Out = Cast<UPCGBasePointData>(TypedOut);

				check(Out)

				if (IsValid(In)) { Out->InitializeFromData(In); }

				return true;
			}

			if (InitOut == EIOInit::Duplicate)
			{
				check(In)
				const T* TypedIn = Cast<T>(In);

				if (!TypedIn)
				{
					T* TypedOut = SharedContext.Get()->ManagedObjects->DuplicateData<T>(In);
					Out = Cast<UPCGBasePointData>(TypedOut);
				}
				else
				{
					SharedContext.Get()->ManagedObjects->New<T>();
					Out->InitializeFromData(In);
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
		int32 GetOutInNum() const { return Out && !Out->GetNumPoints() ? Out->GetNumPoints() : In ? In->GetNumPoints() : -1; }

		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetInKeys();
		TSharedPtr<FPCGAttributeAccessorKeysPointIndices> GetOutKeys(const bool bEnsureValidKeys = false);

		void PrintOutKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;
		void PrintOutInKeysMap(TMap<PCGMetadataEntryKey, int32>& InMap) const;

		FORCEINLINE FConstPoint GetInPoint(const int32 Index) const { return FConstPoint(In, Index, IOIndex); }
		FORCEINLINE FMutablePoint GetOutPoint(const int32 Index) const { return FMutablePoint(Out, Index, IOIndex); }

		FScope GetInScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetInScope(const PCGExMT::FScope& Scope) const { return GetInScope(Scope.Start, Scope.Count, true); }
		FScope GetInRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

		FScope GetOutScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetOutScope(const PCGExMT::FScope& Scope) const { return GetOutScope(Scope.Start, Scope.Count, true); }
		FScope GetOutRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

		void SetPoints(const TArray<FPCGPoint>& InPCGPoints);
		void SetPoints(const int32 StartIndex, const TArray<FPCGPoint>& InPCGPoints, const EPCGPointNativeProperties Properties = EPCGPointNativeProperties::All);

		FName OutputPin = PCGEx::OutputPointsLabel;

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

		// WriteIndices is expected to be the size of in point count here
		// Shorthand to simplify point insertion in cases where we want to preserve original points
		void InheritPoints(const TArrayView<const int32>& WriteIndices) const;

		// ReadIndices is expected to the size of OUT point counts
		// Shorthand to simplify point insertion in cases where we want to copy a subset of the original points
		// !!! Note that this method resizes the data !!!
		void InheritPoints(const TArrayView<const int32>& SelectedIndices, const int32 StartIndex) const;

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

		void Gather(const TArrayView<int32> InIndices) const;
		void Gather(const TArrayView<int8> InMask, const bool bInvert = false) const;

		void DeleteAttribute(FName AttributeName) const;

		template <typename T>
		FPCGMetadataAttribute<T>* CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->CreateAttribute(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
			}

			return OutAttribute;
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindOrCreateAttribute(FName AttributeName, const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* OutAttribute = nullptr;
			if (!Out) { return OutAttribute; }

			{
				FWriteScopeLock WriteScopeLock(AttributesLock);
				OutAttribute = Out->Metadata->FindOrCreateAttribute(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			}

			return OutAttribute;
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
		FPointIOCollection(FPCGExContext* InContext, FName InputLabel, EIOInit InitOut = EIOInit::None, bool bIsTransactional = false);
		FPointIOCollection(FPCGExContext* InContext, TArray<FPCGTaggedData>& Sources, EIOInit InitOut = EIOInit::None, bool bIsTransactional = false);

		~FPointIOCollection();

		FName OutputPin = PCGEx::OutputPointsLabel;
		TArray<TSharedPtr<FPointIO>> Pairs;

		/**
		 * Initialize from Sources
		 * @param Sources 
		 * @param InitOut 
		 */
		void Initialize(
			TArray<FPCGTaggedData>& Sources,
			EIOInit InitOut = EIOInit::None);

		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr);
		TSharedPtr<FPointIO> Emplace_GetRef(EIOInit InitOut = EIOInit::New);
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::None);
		TSharedPtr<FPointIO> Insert_Unsafe(const int32 Index, const TSharedPtr<FPointIO>& PointIO);
		TSharedPtr<FPointIO> Add_Unsafe(const TSharedPtr<FPointIO>& PointIO);
		void Add_Unsafe(const TArray<TSharedPtr<FPointIO>>& IOs);


		template <typename T>
		TSharedPtr<FPointIO> Emplace_GetRef(const UPCGBasePointData* In, const EIOInit InitOut = EIOInit::None, const TSet<FString>* Tags = nullptr)
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
		TSharedPtr<FPointIO> Emplace_GetRef(const TSharedPtr<FPointIO>& PointIO, const EIOInit InitOut = EIOInit::None)
		{
			TSharedPtr<FPointIO> Branch = Emplace_GetRef<T>(PointIO->GetIn(), InitOut);
			if (!Branch) { return nullptr; }

			Branch->Tags->Reset(PointIO->Tags);
			Branch->RootIO = PointIO;
			return Branch;
		}

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
		FString TagId;
		IDType TagValue;
		TArray<TSharedRef<FPointIO>> Entries;

		FPointIOTaggedEntries(const FString& InTagId, const IDType& InTagValue)
			: TagId(InTagId), TagValue(InTagValue)
		{
		}

		~FPointIOTaggedEntries() = default;

		void Add(const TSharedRef<FPointIO>& Value);
	};

	class PCGEXTENDEDTOOLKIT_API FPointIOTaggedDictionary
	{
	public:
		FString TagId;             // LeftSide
		TMap<int32, int32> TagMap; // :RightSize @ Entry Index
		TArray<TSharedPtr<FPointIOTaggedEntries>> Entries;

		explicit FPointIOTaggedDictionary(const FString& InTagId)
			: TagId(InTagId)
		{
		}

		~FPointIOTaggedDictionary() = default;

		bool CreateKey(const TSharedRef<FPointIO>& PointIOKey);
		bool TryAddEntry(const TSharedRef<FPointIO>& PointIOEntry);
		TSharedPtr<FPointIOTaggedEntries> GetEntries(int32 Key);
	};

#pragma endregion

	namespace PCGExPointIO
	{
		int32 GetTotalPointsNum(const TArray<TSharedPtr<FPointIO>>& InIOs, const EIOSide InSide = EIOSide::In);

		static const UPCGBasePointData* GetPointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGBasePointData* PointData = Cast<const UPCGBasePointData>(Source.Data);
			if (!PointData) { return nullptr; }

			return PointData;
		}

		static UPCGBasePointData* GetMutablePointData(const FPCGContext* Context, const FPCGTaggedData& Source)
		{
			const UPCGBasePointData* PointData = GetPointData(Context, Source);
			if (!PointData) { return nullptr; }

			return const_cast<UPCGBasePointData*>(PointData);
		}

		static const UPCGBasePointData* ToPointData(FPCGExContext* Context, const FPCGTaggedData& Source)
		{
			// NOTE : This has a high probability of creating new data on the fly
			// so it should absolutely not be used to be inherited or duplicated
			// since it would mean point data that inherit potentially destroyed parents
			if (const UPCGBasePointData* RealPointData = Cast<const UPCGBasePointData>(Source.Data))
			{
				return RealPointData;
			}
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data))
			{
				// Currently we support collapsing to point data only, but at some point in the future that might be different
				const UPCGBasePointData* PointData = Cast<const UPCGSpatialData>(SpatialData)->ToPointData(Context);

				//Keep track of newly created data internally
				if (PointData != SpatialData) { Context->ManagedObjects->Add(const_cast<UPCGBasePointData*>(PointData)); }
				return PointData;
			}

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(Source.Data))
			{
				const UPCGMetadata* ParamMetadata = ParamData->Metadata;

				const int64 ParamItemCount = ParamMetadata->GetLocalItemCount();
				if (ParamItemCount == 0) { return nullptr; }

				UPCGBasePointData* PointData = Context->ManagedObjects->New<PCGEX_NEW_POINT_DATA_TYPE>();

				check(PointData->Metadata);

				PointData->Metadata->Initialize(ParamMetadata);
				PointData->SetNumPoints(ParamItemCount);

				const TPCGValueRange<int64> MetadataEntries = PointData->GetMetadataEntryValueRange();

				for (int i = 0; i < ParamItemCount; ++i) { MetadataEntries[i] = i; }

				return PointData;
			}

			return nullptr;
		}
	}

	PCGEXTENDEDTOOLKIT_API
	void GetPoints(const FScope& Scope, TArray<FPCGPoint>& OutPCGPoints);

	static TSharedPtr<FPointIO> TryGetSingleInput(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bThrowError)
	{
		TSharedPtr<FPointIO> SingleIO;
		const TSharedPtr<FPointIOCollection> Collection = MakeShared<FPointIOCollection>(InContext, InputPinLabel, EIOInit::None, bTransactional);

		if (!Collection->Pairs.IsEmpty() && Collection->Pairs[0]->GetNum() > 0)
		{
			SingleIO = Collection->Pairs[0];
		}
		else if (bThrowError)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Missing or zero-points '{0}' inputs")), FText::FromName(InputPinLabel)));
		}

		return SingleIO;
	}
}
