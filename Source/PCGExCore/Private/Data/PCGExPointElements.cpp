// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointElements.h"
#include "Data/PCGExPointIO.h"

namespace PCGExData
{
	FScope::FScope(UPCGBasePointData* InData, const int32 InStart, const int32 InCount)
		: PCGExMT::FScope(InStart, InCount), Data(InData)
	{
	}

	FScope::FScope(const UPCGBasePointData* InData, const int32 InStart, const int32 InCount)
		: PCGExMT::FScope(InStart, InCount), Data(const_cast<UPCGBasePointData*>(InData))
	{
	}

#pragma region FPoint

	FElement::FElement(const uint64 Hash)
		: Index(PCGEx::H64A(Hash)), IO(PCGEx::H64B(Hash))
	{
	}

	FElement::FElement(const int32 InIndex, const int32 InIO)
		: Index(InIndex), IO(InIO)
	{
	}

	FElement::FElement(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex)
		: Index(InIndex), IO(InIO->IOIndex)
	{
	}

	FPoint::FPoint(const uint64 Hash)
		: FElement(Hash)
	{
	}

	FPoint::FPoint(const int32 InIndex, const int32 InIO)
		: FElement(InIndex, InIO)
	{
	}

	FPoint::FPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex)
		: FElement(InIO, InIndex)
	{
	}

	FWeightedPoint::FWeightedPoint(const uint64 Hash, const double InWeight)
		: FPoint(Hash)
	{
	}

	FWeightedPoint::FWeightedPoint(const int32 InIndex, const double InWeight, const int32 InIO)
		: FPoint(InIndex, InIO), Weight(InWeight)
	{
	}

	FWeightedPoint::FWeightedPoint(const TSharedPtr<FPointIO>& InIO, const uint32 InIndex, const double InWeight)
		: FPoint(InIO, InIndex), Weight(InWeight)
	{
	}

	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FMutablePoint::FMutablePoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetOut())
	{
	}

	FTransform& FMutablePoint::GetMutableTransform()
	{
		return Data->GetTransformValueRange(false)[Index];
	}

	void FMutablePoint::SetDensity(const float InValue)
	{
		Data->GetDensityValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetSteepness(const float InValue)
	{
		Data->GetSteepnessValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetTransform(const FTransform& InValue)
	{
		Data->GetTransformValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetLocation(const FVector& InValue)
	{
		Data->GetTransformValueRange(false)[Index].SetLocation(InValue);
	}

	void FMutablePoint::SetScale3D(const FVector& InValue)
	{
		Data->GetTransformValueRange(false)[Index].SetScale3D(InValue);
	}

	void FMutablePoint::SetRotation(const FQuat& InValue)
	{
		Data->GetTransformValueRange(false)[Index].SetRotation(InValue);
	}

	void FMutablePoint::SetBoundsMin(const FVector& InValue)
	{
		Data->GetBoundsMinValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetBoundsMax(const FVector& InValue)
	{
		Data->GetBoundsMaxValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetLocalCenter(const FVector& InValue)
	{
		PCGPointHelpers::SetLocalCenter(InValue, Data->GetBoundsMinValueRange(false)[Index], Data->GetBoundsMaxValueRange(false)[Index]);
	}

	void FMutablePoint::SetExtents(const FVector& InValue, const bool bKeepLocalCenter)
	{
		const TPCGValueRange<FVector> BoundsMin = Data->GetBoundsMinValueRange(false);
		const TPCGValueRange<FVector> BoundsMax = Data->GetBoundsMaxValueRange(false);

		if (bKeepLocalCenter)
		{
			const FVector LocalCenter = Data->GetLocalCenter(Index);
			BoundsMin[Index] = LocalCenter - InValue;
			BoundsMax[Index] = LocalCenter + InValue;
		}
		else
		{
			BoundsMin[Index] = -InValue;
			BoundsMax[Index] = InValue;
		}
	}

	void FMutablePoint::SetLocalBounds(const FBox& InValue)
	{
		Data->GetBoundsMinValueRange(false)[Index] = InValue.Min;
		Data->GetBoundsMaxValueRange(false)[Index] = InValue.Max;
	}

	void FMutablePoint::SetMetadataEntry(const int64 InValue)
	{
		Data->GetMetadataEntryValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetColor(const FVector4& InValue)
	{
		Data->GetColorValueRange(false)[Index] = InValue;
	}

	void FMutablePoint::SetSeed(const int32 InValue)
	{
		Data->GetSeedValueRange(false)[Index] = InValue;
	}

	FConstPoint::FConstPoint(const FMutablePoint& InPoint)
		: FConstPoint(InPoint.Data, InPoint.Index)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FPoint(InIndex, InIO), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const UPCGBasePointData* InData, const FPoint& InPoint)
		: FPoint(InPoint.Index, InPoint.IO), Data(InData)
	{
	}

	FConstPoint::FConstPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FPoint(InFacade, InIndex), Data(InFacade->GetIn())
	{
	}


	FMutablePoint::FMutablePoint(UPCGBasePointData* InData, const uint64 Hash)
		: FPoint(Hash), Data(InData)
	{
	}

	FProxyPoint::FProxyPoint(const FMutablePoint& InPoint)
		: Transform(InPoint.GetTransform()), BoundsMin(InPoint.GetBoundsMin()), BoundsMax(InPoint.GetBoundsMax()), Steepness(InPoint.GetSteepness()), Color(InPoint.GetColor())
	{
	}

	FProxyPoint::FProxyPoint(const FConstPoint& InPoint)
		: Transform(InPoint.GetTransform()), BoundsMin(InPoint.GetBoundsMin()), BoundsMax(InPoint.GetBoundsMax()), Steepness(InPoint.GetSteepness()), Color(InPoint.GetColor())
	{
	}

	FProxyPoint::FProxyPoint(const UPCGBasePointData* InData, const uint64 Hash)
		: FProxyPoint(FConstPoint(InData, Hash))
	{
	}

	FProxyPoint::FProxyPoint(const UPCGBasePointData* InData, const int32 InIndex, const int32 InIO)
		: FProxyPoint(FConstPoint(InData, InIndex, InIO))
	{
	}

	FProxyPoint::FProxyPoint(const TSharedPtr<FPointIO>& InFacade, const int32 InIndex)
		: FProxyPoint(FConstPoint(InFacade, InIndex))
	{
	}

	void FProxyPoint::CopyTo(UPCGBasePointData* InData) const
	{
		InData->GetTransformValueRange(false)[Index] = Transform;
		InData->GetBoundsMinValueRange(false)[Index] = BoundsMin;
		InData->GetBoundsMinValueRange(false)[Index] = BoundsMax;
		InData->GetColorValueRange(false)[Index] = Color;
	}

	void FProxyPoint::CopyTo(FMutablePoint& InPoint) const
	{
		InPoint.SetTransform(Transform);
		InPoint.SetBoundsMin(BoundsMin);
		InPoint.SetBoundsMax(BoundsMax);
		InPoint.SetColor(Color);
	}

#pragma endregion
}
