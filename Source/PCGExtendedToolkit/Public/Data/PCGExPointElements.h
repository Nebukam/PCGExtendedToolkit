// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExH.h"
#include "PCGExMT.h"
#include "Data/PCGBasePointData.h"
#include "UObject/UObjectGlobals.h"

namespace PCGExData
{
	class FPointIO;
}

namespace PCGExData
{
#pragma region FPoint

	struct PCGEXTENDEDTOOLKIT_API FElement
	{
		int32 Index = -1;
		int32 IO = -1;

		FElement() = default;
		explicit FElement(const uint64 Hash);
		explicit FElement(const int32 InIndex, const int32 InIO = -1);
		FElement(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex);

		FORCEINLINE bool IsValid() const { return Index >= 0; }
		FORCEINLINE uint64 H64() const { return PCGEx::H64U(Index, IO); }

		explicit operator int32() const { return Index; }

		bool operator==(const FElement& Other) const { return Index == Other.Index && IO == Other.IO; }
		FORCEINLINE friend uint32 GetTypeHash(const FElement& Key) { return HashCombineFast(Key.Index, Key.IO); }
	};

	// FPoint is used when we only care about point index
	// And possibly IOIndex as well, but without the need to track the actual data object.
	// This makes it a barebone way to track point indices with some additional mapping mechanism with a secondary index
	struct PCGEXTENDEDTOOLKIT_API FPoint : FElement
	{
		FPoint() = default;
		virtual ~FPoint() = default;

		explicit FPoint(const uint64 Hash);
		explicit FPoint(const int32 InIndex, const int32 InIO = -1);
		FPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex);

		virtual const FTransform& GetTransform() const PCGEX_NOT_IMPLEMENTED_RET(GetTransform, FTransform::Identity)
		virtual void GetTransformNoScale(FTransform& OutTransform) const PCGEX_NOT_IMPLEMENTED(GetTransform)
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
		virtual ~FWeightedPoint() override = default;

		explicit FWeightedPoint(const uint64 Hash, const double InWeight = 1);
		explicit FWeightedPoint(const int32 InIndex, const double InWeight = 1, const int32 InIO = -1);
		FWeightedPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex, const double InWeight = 1);
	};

#define PCGEX_POINT_PROXY_OVERRIDES \
FORCEINLINE virtual const FTransform& GetTransform() const override { return Data->GetTransform(Index); }\
FORCEINLINE virtual void GetTransformNoScale(FTransform& OutTransform) const override { OutTransform = Data->GetTransform(Index); OutTransform.SetScale3D(FVector::OneVector); }\
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

#pragma endregion

	struct FScope : PCGExMT::FScope
	{
		UPCGBasePointData* Data = nullptr;

		FScope() = default;

		FScope(UPCGBasePointData* InData, const int32 InStart, const int32 InCount);
		FScope(const UPCGBasePointData* InData, const int32 InStart, const int32 InCount);

		FORCEINLINE FConstPoint CFirst() const { return FConstPoint(Data, Start); }
		FORCEINLINE FConstPoint CLast() const { return FConstPoint(Data, End - 1); }

		FORCEINLINE FMutablePoint MFirst() const { return FMutablePoint(Data, Start); }
		FORCEINLINE FMutablePoint MLast() const { return FMutablePoint(Data, End - 1); }

		~FScope() = default;
		bool IsValid() const { return Start >= 0 && Count > 0 && Data->GetNumPoints() <= End; }
	};
}
